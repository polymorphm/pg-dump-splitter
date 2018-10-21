// abort, realloc, free
#include <stdlib.h>

// memcpy, memcmp
#include <string.h>

// fprintf, stderr
#include <stdio.h>

#include <lua.h>
#include <lauxlib.h>

#include "pg-dump-splitter.h"

static const long lex_buf_init_size = 1024;

enum
{
    lex_type_undefined,
    lex_type_ident,
    lex_type_number,
    lex_type_string,
    lex_type_symbols,
    lex_type_comment,
};

enum
{
    lex_subtype_undefined,
    lex_subtype_simple_ident,
    lex_subtype_quoted_ident,
    lex_subtype_number,
    lex_subtype_simple_string,
    lex_subtype_escape_string,
    lex_subtype_dollar_string,
    lex_subtype_special_symbols,
    lex_subtype_random_symbols,
    lex_subtype_sing_line_comment,
    lex_subtype_mult_line_comment,
};

struct lex_ctx
{
    long max_size;  // limit of lexeme buffer size
    long pos;       // current char position in stream, 1 based
    long line;      // current line position in stream, 1 based
    long col;       // current column position in stream, 1 based
    long ppos;       // previous char position in stream, 1 based
    long pline;      // previous line position in stream, 1 based
    long pcol;       // previous column position in stream, 1 based
    int type;       // current lexeme type
    int subtype;    // current lexeme subtype
    long lpos;      // current lexeme char position in stream, 1 based
    long lline;     // current lexeme line position in stream, 1 based
    long lcol;      // current lexeme column positon in stream, 1 based
    long size;      // lexeme allocated buffer size
    long len;       // current lexeme length
    char *buf;      // pointer to lexeme buffer, not zero terminated
    char stash;     // stashed char, to return to it next iteration
    union lex_ctx_state
    {
        struct lex_ctx_state_number
        {
            long e_len; // length of the first part's number
            int has_dot; // has dot
        } number;
        struct lex_ctx_state_dollar_string
        {
            long marker_len; // start/stop marker's length
        } dollar_string;
    } state;       // type's specific state
};

static inline void
set_lpos (struct lex_ctx *ctx)
{
    ctx->lpos = ctx->pos;
    ctx->lline = ctx->line;
    ctx->lcol = ctx->col;
}

static inline void
set_plpos (struct lex_ctx *ctx)
{
    ctx->lpos = ctx->ppos;
    ctx->lline = ctx->pline;
    ctx->lcol = ctx->pcol;
}

static void
realloc_buf (struct lex_ctx *ctx, long need_size)
{
    long size = ctx->size;

    if (__builtin_expect (!size, 0))
    {
        size = lex_buf_init_size;
    }

    do
    {
        size *= 2;
    }
    while (need_size > size);

    if (__builtin_expect (size > ctx->max_size, 0))
    {
        size = need_size;
    }

    char *buf = realloc (ctx->buf, size);

    if (__builtin_expect (!buf, 0))
    {
        fprintf (stderr,
                "memory reallocation error for lex_ctx buffer\n");
        abort ();
    }

    ctx->buf = buf;
    ctx->size = size;
}

static inline void
push_c_to_buf (lua_State *L,
        struct lex_ctx *ctx, char add)
{
    long need_size = ctx->len + 1;

    if (__builtin_expect (need_size > ctx->size, 0))
    {
        if (__builtin_expect (need_size > ctx->max_size, 0))
        {
            luaL_error (L,
                    "max_size lexeme buffer limit has exceeded: %I > %I",
                    need_size, ctx->max_size);
            __builtin_unreachable ();
        }

        realloc_buf (ctx, need_size);
    }

    ctx->buf[ctx->len] = add;
    ctx->len += 1;
}

static inline void
push_str_to_buf (lua_State *L,
        struct lex_ctx *ctx, const char *add, long add_size)
{
    if (__builtin_expect (!add_size, 0))
    {
        return;
    }

    long need_size = ctx->len + add_size;

    if (__builtin_expect (need_size > ctx->size, 0))
    {
        if (__builtin_expect (need_size > ctx->max_size, 0))
        {
            luaL_error (L,
                    "max_size lexeme buffer limit has exceeded: %I > %I",
                    need_size, ctx->max_size);
            __builtin_unreachable ();
        }

        realloc_buf (ctx, need_size);
    }

    memcpy (ctx->buf + ctx->len, add, add_size);
    ctx->len += add_size;
}

static void
push_quoted_lexeme_translated (lua_State *L,
        long len, const char *buf, char q)
{
    if (len < 2 || buf[0] != q || buf[len - 1] != q)
    {
        lua_pushnil (L);
        return;
    }

    luaL_Buffer B;
    char *out = luaL_buffinitsize (L, &B, len);
    long sz = 0;
    long l = len - 1;
    char c = 0;
    char pc = 0;

    for (long i = 1; i < l; ++i)
    {
        c = buf[i];
        if (pc == q && c == q)
        {
            c = 0;
        }
        else
        {
            out[sz] = c;
            ++sz;
        }
        pc = c;
    }

    luaL_pushresultsize (&B, sz);
}

static void
push_dollar_string_translated (lua_State *L, long len, long marker_len,
        const char *buf)
{
    if (!marker_len || len < marker_len * 2 ||
            memcmp (buf + len - marker_len, buf, marker_len))
    {
        lua_pushnil (L);
        return;
    }

    lua_pushlstring (L, buf + marker_len, len - marker_len * 2);
}

static void
push_sing_line_comment_translated (lua_State *L, long len, const char *buf)
{
    if (len < 2 || buf[0] != '-' || buf[1] != '-')
    {
        lua_pushnil (L);
        return;
    }

    lua_pushlstring (L, buf + 2, len - 2);
}

static void
push_mult_line_comment_translated (lua_State *L, long len, const char *buf)
{
    if (len < 4 || buf[0] != '/' || buf[1] != '*' ||
            buf[len - 2] != '*' || buf[len - 1] != '/')
    {
        lua_pushnil (L);
        return;
    }

    lua_pushlstring (L, buf + 2, len - 4);
}

static const char *lex_ctx_tname = "lex_ctx";

static int
lex_make_ctx (lua_State *L)
{
    long max_size = luaL_checkinteger (L, 1);
    struct lex_ctx *ctx = lua_newuserdata (L, sizeof (struct lex_ctx));

    *ctx = (struct lex_ctx)
    {
        .max_size = max_size,
    };

    luaL_setmetatable (L, lex_ctx_tname);

    return 1;
}

static int
lex_feed (lua_State *L)
{
    __label__ retry_c, retry_stash;

    struct lex_ctx *ctx = luaL_checkudata (L, 1, lex_ctx_tname);
    size_t input_len = 0;
    const char *input = lua_tolstring (L, 2, &input_len);
    int not_exists_callback = lua_isnil (L, 3); // arg: callback function
    int trans_more = lua_toboolean (L, 4); // arg: translate more?

    char stash, c;

    void
    yield_lexeme ()
    {
        if (not_exists_callback)
        {
            return;
        }

        lua_pushvalue (L, 3);

        lua_pushinteger (L, ctx->type);
        lua_pushinteger (L, ctx->subtype);
        lua_createtable (L, 0, 3);
        lua_pushinteger (L, ctx->lpos);
        lua_setfield (L, -2, "lpos");
        lua_pushinteger (L, ctx->lline);
        lua_setfield (L, -2, "lline");
        lua_pushinteger (L, ctx->lcol);
        lua_setfield (L, -2, "lcol");
        lua_pushlstring (L, ctx->buf, ctx->len);

        switch (ctx->subtype)
        {
            case lex_subtype_simple_ident:
                lua_pushlstring (L, ctx->buf, ctx->len);
                lua_getfield (L, -1, "lower");
                lua_insert (L, lua_absindex (L, -2));
                lua_call (L, 1, 1);
                break;

            case lex_subtype_quoted_ident:
                push_quoted_lexeme_translated (L, ctx->len, ctx->buf, '"');
                break;

            default:
                if (trans_more)
                {
                    switch (ctx->subtype)
                    {
                        case lex_subtype_simple_string:
                            push_quoted_lexeme_translated (
                                    L, ctx->len, ctx->buf, '\'');
                            break;

                        // XXX  unimplemented yet:
                        //          ``case lex_subtype_escape_string: ...``

                        case lex_subtype_dollar_string:
                            push_dollar_string_translated (L, ctx->len,
                                    ctx->state.dollar_string.marker_len,
                                    ctx->buf);
                            break;

                        case lex_subtype_sing_line_comment:
                            push_sing_line_comment_translated (
                                    L, ctx->len, ctx->buf);
                            break;

                        case lex_subtype_mult_line_comment:
                            push_mult_line_comment_translated (
                                    L, ctx->len, ctx->buf);
                            break;

                        default:
                            lua_pushnil (L);
                    }
                }
                else
                {
                    lua_pushnil (L);
                }
        }

        lua_call (L, 5, 0);
    }

    void
    finish_lexeme ()
    {
        yield_lexeme ();

        ctx->type = lex_type_undefined;
        ctx->subtype = lex_subtype_undefined;
        ctx->len = 0;
    }

    void
    finish_lexeme_with_state ()
    {
        finish_lexeme ();
        ctx->state = (union lex_ctx_state) {};
    }

    inline void
    undefined_wo_stash ()
    {
        switch (c)
        {
            case 'a' ... 'd':
            case 'f' ... 't':
            case 'v' ... 'z':
            case 'A' ... 'D':
            case 'F' ... 'T':
            case 'V' ... 'Z':
            case '_':
                // no cases 'e' 'E' 'u' 'U' here, cause of them stash behaviour.
                // no case '0' ... '9' here, cause of starting ident lexeme

                ctx->type = lex_type_ident;
                ctx->subtype = lex_subtype_simple_ident;
                set_lpos (ctx);
                push_c_to_buf (L, ctx, c);
                break;

            case '0' ... '9':
                ctx->type = lex_type_number;
                ctx->subtype = lex_subtype_number;
                set_lpos (ctx);
                push_c_to_buf (L, ctx, c);
                break;

            case '\'':
                ctx->type = lex_type_string;
                ctx->subtype = lex_subtype_simple_string;
                set_lpos (ctx);
                push_c_to_buf (L, ctx, '\'');
                break;

            case '"':
                ctx->type = lex_type_ident;
                ctx->subtype = lex_subtype_quoted_ident;
                set_lpos (ctx);
                push_c_to_buf (L, ctx, '"');
                break;

            case '$':
                ctx->type = lex_type_string;
                ctx->subtype = lex_subtype_dollar_string;
                set_lpos (ctx);
                push_c_to_buf (L, ctx, '$');
                break;

            case ',':
            case ';':
            case ':':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
                // no case '.' here, cause of its stash behaviour

                ctx->type = lex_type_symbols;
                ctx->subtype = lex_subtype_special_symbols;
                set_lpos (ctx);
                push_c_to_buf (L, ctx, c);
                break;

            case '`':
            case '~':
            case '!':
            case '@':
            case '#':
            case '%':
            case '^':
            case '&':
            case '*':
            case '=':
            case '|':
            case '<':
            case '>':
            case '?':
                // no cases '-' '+' '/' here, cause of them stash behaviour

                ctx->type = lex_type_symbols;
                ctx->subtype = lex_subtype_random_symbols;
                set_lpos (ctx);
                push_c_to_buf (L, ctx, c);
                break;

            case '-':
            case '+':
            case '/':
            case '.':
            case 'e':
            case 'E':
            case 'u':
            case 'U':
                ctx->stash = c;
                break;

            case ' ':
            case '\t':
            case '\v':
            case '\n':
            case '\r':
                break;

            case 0:
                ctx->pos = 0;
                ctx->line = 0;
                ctx->col = 0;
                ctx->ppos = 0;
                ctx->pline = 0;
                ctx->pcol = 0;
                ctx->lpos = 0;
                ctx->lline = 0;
                ctx->lcol = 0;
                break;

            case '\\':
                luaL_error (L,
                        "pos(%I) line(%I) col(%I): "
                        "lexeme type started with \"\\\" "
                        "is forbidden",
                        ctx->pos, ctx->line, ctx->col);
                __builtin_unreachable ();

            default:
                luaL_error (L,
                        "pos(%I) line(%I) col(%I): "
                        "unknown lexeme type started with character: "
                        "%d %c",
                        ctx->pos, ctx->line, ctx->col, c, c);
                __builtin_unreachable ();
        }
    }

    inline void
    undefined_with_stash ()
    {
        switch (stash)
        {
            case '-':
            case '+':
            case '/':
            case '.':
                if (stash == '-' && c == '-')
                {
                    ctx->type = lex_type_comment;
                    ctx->subtype = lex_subtype_sing_line_comment;
                    set_plpos (ctx);
                    push_str_to_buf (L, ctx, "--", 2);
                }
                else if (stash == '/' && c == '*')
                {
                    ctx->type = lex_type_comment;
                    ctx->subtype = lex_subtype_mult_line_comment;
                    set_plpos (ctx);
                    push_str_to_buf (L, ctx, "/*", 2);
                }
                else if ((stash == '-' || stash == '.') &&
                        c >= '0' && c <= '9')
                {
                    // XXX  we don't support numbers like ``-.123`` yet

                    ctx->type = lex_type_number;
                    ctx->subtype = lex_subtype_number;
                    set_plpos (ctx);
                    push_c_to_buf (L, ctx, stash);
                    push_c_to_buf (L, ctx, c);
                }
                else if (stash == '.')
                {
                    ctx->type = lex_type_symbols;
                    ctx->subtype = lex_subtype_special_symbols;
                    set_plpos (ctx);
                    push_c_to_buf (L, ctx, stash);
                    goto retry_c;
                }
                else
                {
                    ctx->type = lex_type_symbols;
                    ctx->subtype = lex_subtype_random_symbols;
                    set_plpos (ctx);
                    push_c_to_buf (L, ctx, stash);
                    goto retry_c;
                }
                break;

            case 'e':
            case 'E':
                if (c == '\'')
                {
                    ctx->type = lex_type_string;
                    ctx->subtype = lex_subtype_escape_string;
                    set_plpos (ctx);
                    push_c_to_buf (L, ctx, stash);
                    push_c_to_buf (L, ctx, c);
                }
                else
                {
                    ctx->type = lex_type_ident;
                    ctx->subtype = lex_subtype_simple_ident;
                    set_plpos (ctx);
                    push_c_to_buf (L, ctx, stash);
                    goto retry_c;
                }
                break;

            case 'u':
            case 'U':
                if (c == '&')
                {
                    luaL_error (L,
                            "pos(%I) line(%I) col(%I): "
                            "lexeme type started with \"u&\" "
                            "is not supported yet",
                            ctx->pos, ctx->line, ctx->col);
                    __builtin_unreachable ();
                }
                else
                {
                    ctx->type = lex_type_ident;
                    ctx->subtype = lex_subtype_simple_ident;
                    set_plpos (ctx);
                    push_c_to_buf (L, ctx, stash);
                    goto retry_c;
                }
                break;

            default:
                luaL_error (L,
                        "pos(%I) line(%I) col(%I): "
                        "unknown lexeme type started with characters: "
                        "%d %d %c %c",
                        ctx->pos, ctx->line, ctx->col,
                        stash, c, stash, c);
                __builtin_unreachable ();
        }
    }

    inline void
    simple_ident ()
    {
        switch (c)
        {
            case 'a' ... 'z':
            case 'A' ... 'Z':
            case '_':
            case '0' ... '9':
                push_c_to_buf (L, ctx, c);
                break;

            default:
                finish_lexeme ();
                goto retry_c;
        }
    }

    inline void
    quoted_ident_wo_stash ()
    {
        switch (c)
        {
            case '"':
                if (__builtin_expect (!ctx->len, 0))
                {
                    fprintf (stderr, "unexpected program flow\n");
                    abort ();
                }

                ctx->stash = c;
                break;

            case 0:
                luaL_error (L,
                        "pos(%I) line(%I) col(%I): "
                        "unterminated lexeme: quoted_ident",
                        ctx->pos, ctx->line, ctx->col);
                __builtin_unreachable ();

            default:
                push_c_to_buf (L, ctx, c);
        }
    }

    inline void
    quoted_ident_with_stash ()
    {
        if (__builtin_expect (stash != '"', 0)) {
            fprintf (stderr, "unexpected program flow\n");
            abort ();
        }

        if (c == '"')
        {
            push_str_to_buf (L, ctx, "\"\"", 2);
        }
        else
        {
            push_c_to_buf (L, ctx, stash);
            finish_lexeme ();
            goto retry_c;
        }
    }

    inline void
    number_wo_stash ()
    {
        switch (c)
        {
            case '-':
            case '+':
                if (ctx->len &&
                        ctx->len != ctx->state.number.e_len)
                {
                    finish_lexeme_with_state ();
                    goto retry_c;
                }
                else
                {
                    ctx->stash = c;
                    break;
                }

            case '0' ... '9':
                push_c_to_buf (L, ctx, c);
                break;

            case 'e':
            case 'E':
                if (ctx->state.number.e_len)
                {
                    finish_lexeme_with_state ();
                    goto retry_c;
                }
                else
                {
                    ctx->stash = c;
                    break;
                }

            case '.':
                if (ctx->state.number.has_dot)
                {
                    finish_lexeme_with_state ();
                    goto retry_c;
                }
                else
                {
                    ctx->stash = c;
                    break;
                }

            default:
                finish_lexeme_with_state ();
                goto retry_c;
        }
    }

    inline void
    number_with_stash ()
    {
        switch (stash)
        {
            case '-':
            case '+':
                switch (c)
                {
                    case '.':
                        if (ctx->state.number.e_len)
                        {
                            finish_lexeme_with_state ();
                            goto retry_stash;
                        }
                        __attribute__ ((fallthrough));
                    case '0' ... '9':
                        push_c_to_buf (L, ctx, stash);
                        push_c_to_buf (L, ctx, c);
                        break;

                    default:
                        finish_lexeme_with_state ();
                        goto retry_stash;
                }
                break;

            case '.':
                if (c == '.')
                {
                    finish_lexeme_with_state ();
                    goto retry_stash;
                }
                else
                {
                    push_c_to_buf (L, ctx, stash);
                    ctx->state.number.has_dot = 1;
                    goto retry_c;
                }

            case 'e':
            case 'E':
                switch (c)
                {
                    case '-':
                    case '+':
                    case '0' ... '9':
                        push_c_to_buf (L, ctx, stash);
                        ctx->state.number.e_len = ctx->len;
                        ctx->state.number.has_dot = 1;

                        if (c == '-' || c == '+')
                        {
                            goto retry_c;
                        }
                        else
                        {
                            push_c_to_buf (L, ctx, c);
                            break;
                        }

                    default:
                        finish_lexeme_with_state ();
                        goto retry_stash;
                }
                break;

            default:
                fprintf (stderr, "unexpected program flow\n");
                abort ();
        }
    }

    inline void
    simple_string_wo_stash ()
    {
        switch (c)
        {
            case '\'':
                if (__builtin_expect (!ctx->len, 0))
                {
                    fprintf (stderr, "unexpected program flow\n");
                    abort ();
                }

                ctx->stash = c;
                break;

            case 0:
                luaL_error (L,
                        "pos(%I) line(%I) col(%I): "
                        "unterminated lexeme: simple_string",
                        ctx->pos, ctx->line, ctx->col);
                __builtin_unreachable ();

            default:
                push_c_to_buf (L, ctx, c);
        }
    }

    inline void
    escape_string_wo_stash ()
    {
        switch (c)
        {
            case '\'':
                if (__builtin_expect (ctx->len < 2, 0))
                {
                    fprintf (stderr, "unexpected program flow\n");
                    abort ();
                }

                ctx->stash = c;
                break;

            case 0:
                luaL_error (L,
                        "pos(%I) line(%I) col(%I): "
                        "unterminated lexeme: escape_string",
                        ctx->pos, ctx->line, ctx->col);
                __builtin_unreachable ();

            default:
                push_c_to_buf (L, ctx, c);
        }
    }

    inline void
    simple_or_escape_string_with_stash ()
    {
        if (__builtin_expect (stash != '\'', 0)) {
            fprintf (stderr, "unexpected program flow\n");
            abort ();
        }

        if (c == '\'')
        {
            push_str_to_buf (L, ctx, "''", 2);
        }
        else
        {
            push_c_to_buf (L, ctx, stash);
            finish_lexeme ();
            goto retry_c;
        }
    }

    inline void
    dollar_string ()
    {
        if (__builtin_expect (c == 0, 0))
        {
            luaL_error (L,
                    "pos(%I) line(%I) col(%I): "
                    "unterminated lexeme: dollar_string ",
                    ctx->pos, ctx->line, ctx->col);
            __builtin_unreachable ();
        }

        push_c_to_buf (L, ctx, c);

        if (c != '$')
        {
            return;
        }

        long len = ctx->len;
        long marker_len = ctx->state.dollar_string.marker_len;
        const char *buf = ctx->buf;

        if (!marker_len)
        {
            if (__builtin_expect (len < 2, 0))
            {
                fprintf (stderr, "unexpected program flow\n");
                abort ();
            }

            ctx->state.dollar_string.marker_len = len;
        }
        else if (len >= marker_len * 2 &&
                !memcmp (buf + len - marker_len, buf, marker_len))
        {
            finish_lexeme_with_state ();
        }
    }

    inline void
    special_symbols ()
    {
        if (__builtin_expect (!ctx->len, 0))
        {
            fprintf (stderr, "unexpected program flow\n");
            abort ();
        }

        if (ctx->len == 1 && (c == '.' || c == ':') &&
                    ctx->buf[0] == c)
        {
            push_c_to_buf (L, ctx, c);
            finish_lexeme ();
        }
        else
        {
            finish_lexeme ();
            goto retry_c;
        }
    }

    inline void
    random_symbols ()
    {
        switch (c)
        {
            case '`':
            case '~':
            case '!':
            case '@':
            case '#':
            case '%':
            case '^':
            case '&':
            case '*':
            case '-':
            case '=':
            case '+':
            case '|':
            case '<':
            case '>':
            case '?':
            case '/':
                push_c_to_buf (L, ctx, c);
                break;

            default:
                finish_lexeme ();
                goto retry_c;
        }
    }

    inline void
    sing_line_comment ()
    {
        switch (c)
        {
            case '\n':
            case 0:
                finish_lexeme ();
                goto retry_c;

            default:
                push_c_to_buf (L, ctx, c);
        }
    }

    inline void
    mult_line_comment_wo_stash ()
    {
        switch (c)
        {
            case '*':
                if (__builtin_expect (ctx->len < 2, 0))
                {
                    fprintf (stderr, "unexpected program flow\n");
                    abort ();
                }

                ctx->stash = c;
                break;

            case 0:
                luaL_error (L,
                        "pos(%I) line(%I) col(%I): "
                        "unterminated lexeme: mult_line_comment",
                        ctx->pos, ctx->line, ctx->col);
                __builtin_unreachable ();

            default:
                push_c_to_buf (L, ctx, c);
        }
    }

    inline void
    mult_line_comment_with_stash ()
    {
        if (__builtin_expect (stash != '*', 0)) {
            fprintf (stderr, "unexpected program flow\n");
            abort ();
        }

        if (c == '/')
        {
            push_str_to_buf (L, ctx, "*/", 2);
            finish_lexeme ();
        }
        else
        {
            push_c_to_buf (L, ctx, stash);
            goto retry_c;
        }
    }

    if (__builtin_expect (!input_len, 0))
    {
        // the final mark for flushing rest of buffer.
        // it should not be counted in position counters

        input = "\0";
        input_len = 1;
    }

    for (size_t input_i = 0; input_i < input_len; ++input_i)
    {
        c = input[input_i];

        if (__builtin_expect (c, 1))
        {
            ctx->ppos = ctx->pos;
            ctx->pline = ctx->line;
            ctx->pcol = ctx->col;

            if (!ctx->line)
            {
                ctx->line = 1;
            }

            ++ctx->pos;

            if (c == '\n')
            {
                ++ctx->line;
                ctx->col = 0;
            }
            else
            {
                ++ctx->col;
            }
        }

retry_c:
        stash = ctx->stash;
        ctx->stash = 0;

retry_stash:
        switch (ctx->subtype)
        {
            case lex_subtype_undefined:
                if (stash)
                {
                    undefined_with_stash ();
                }
                else
                {
                    undefined_wo_stash ();
                }
                break;

            case lex_subtype_simple_ident:
                simple_ident ();
                break;

            case lex_subtype_quoted_ident:
                if (stash)
                {
                    quoted_ident_with_stash ();
                }
                else
                {
                    quoted_ident_wo_stash ();
                }
                break;

            case lex_subtype_number:
                if (stash)
                {
                    number_with_stash ();
                }
                else
                {
                    number_wo_stash ();
                }
                break;

            case lex_subtype_simple_string:
                if (stash)
                {
                    simple_or_escape_string_with_stash ();
                }
                else
                {
                    simple_string_wo_stash ();
                }
                break;

            case lex_subtype_escape_string:
                if (stash)
                {
                    simple_or_escape_string_with_stash ();
                }
                else
                {
                    escape_string_wo_stash ();
                }
                break;

            case lex_subtype_dollar_string:
                dollar_string ();
                break;

            case lex_subtype_special_symbols:
                special_symbols ();
                break;

            case lex_subtype_random_symbols:
                random_symbols ();
                break;

            case lex_subtype_sing_line_comment:
                sing_line_comment ();
                break;

            case lex_subtype_mult_line_comment:
                if (stash)
                {
                    mult_line_comment_with_stash ();
                }
                else
                {
                    mult_line_comment_wo_stash ();
                }
                break;

            default:
                fprintf (stderr,
                        "unexpected program flow: switch (ctx->subtype): "
                        "%d\n", ctx->subtype);
                abort ();
        }
    }

    return 0;
}

static int
lex_free (lua_State *L)
{
    struct lex_ctx *ctx = luaL_checkudata (L, 1, lex_ctx_tname);

    free (ctx->buf);
    *ctx = (struct lex_ctx) {};

    return 0;
}

static const luaL_Reg lex_reg[] =
{
    {"make_ctx", lex_make_ctx},
    {"feed", lex_feed},
    {"free", lex_free},
    {0, 0},
};

int
luaopen_lex (lua_State *L)
{
    lua_createtable (L, 0, 3);
    lua_pushstring (L, lex_ctx_tname);
    lua_setfield (L, -2, "__name");
    lua_createtable (L, 0, 2);
    lua_pushcfunction (L, lex_feed);
    lua_setfield (L, -2, "feed");
    lua_pushcfunction (L, lex_free);
    lua_setfield (L, -2, "free");
    lua_setfield (L, -2, "__index");
    lua_pushcfunction (L, lex_free);
    lua_setfield (L, -2, "__gc");
    lua_setfield (L, LUA_REGISTRYINDEX, lex_ctx_tname);

    lua_createtable (L, 0, 3 + 1);
    luaL_setfuncs (L, lex_reg, 0);

    lua_createtable (L, 0, 6 + 11);

    lua_pushinteger (L, lex_type_undefined);
    lua_setfield (L, -2, "type_undefined");
    lua_pushinteger (L, lex_type_ident);
    lua_setfield (L, -2, "type_ident");
    lua_pushinteger (L, lex_type_number);
    lua_setfield (L, -2, "type_number");
    lua_pushinteger (L, lex_type_string);
    lua_setfield (L, -2, "type_string");
    lua_pushinteger (L, lex_type_symbols);
    lua_setfield (L, -2, "type_symbols");
    lua_pushinteger (L, lex_type_comment);
    lua_setfield (L, -2, "type_comment");

    lua_pushinteger (L, lex_subtype_undefined);
    lua_setfield (L, -2, "subtype_undefined");
    lua_pushinteger (L, lex_subtype_simple_ident);
    lua_setfield (L, -2, "subtype_simple_ident");
    lua_pushinteger (L, lex_subtype_quoted_ident);
    lua_setfield (L, -2, "subtype_quoted_ident");
    lua_pushinteger (L, lex_subtype_number);
    lua_setfield (L, -2, "subtype_number");
    lua_pushinteger (L, lex_subtype_simple_string);
    lua_setfield (L, -2, "subtype_simple_string");
    lua_pushinteger (L, lex_subtype_escape_string);
    lua_setfield (L, -2, "subtype_escape_string");
    lua_pushinteger (L, lex_subtype_dollar_string);
    lua_setfield (L, -2, "subtype_dollar_string");
    lua_pushinteger (L, lex_subtype_random_symbols);
    lua_setfield (L, -2, "subtype_random_symbols");
    lua_pushinteger (L, lex_subtype_special_symbols);
    lua_setfield (L, -2, "subtype_special_symbols");
    lua_pushinteger (L, lex_subtype_sing_line_comment);
    lua_setfield (L, -2, "subtype_sing_line_comment");
    lua_pushinteger (L, lex_subtype_mult_line_comment);
    lua_setfield (L, -2, "subtype_mult_line_comment");

    lua_setfield (L, -2, "consts");

    return 1;
}

// vi:ts=4:sw=4:et
