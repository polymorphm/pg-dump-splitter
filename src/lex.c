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

    if (__builtin_expect (!size, 0)) size = lex_buf_init_size;

    do
    {
        size *= 2;
    }
    while (need_size > size);

    if (__builtin_expect (size > ctx->max_size, 0)) size = need_size;

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
    if (__builtin_expect (!add_size, 0)) return;

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

struct lex_feed_ctx
{
    lua_State *L;
    struct lex_ctx *ctx;
    int not_exists_callback;
    int trans_more;
    char stash;
    char c;
};

static void
yield_lexeme (struct lex_feed_ctx *f)
{
    if (f->not_exists_callback) return;

    lua_pushvalue (f->L, 3);

    lua_pushinteger (f->L, f->ctx->type);
    lua_pushinteger (f->L, f->ctx->subtype);
    lua_createtable (f->L, 0, 3);
    lua_pushinteger (f->L, f->ctx->lpos);
    lua_setfield (f->L, -2, "lpos");
    lua_pushinteger (f->L, f->ctx->lline);
    lua_setfield (f->L, -2, "lline");
    lua_pushinteger (f->L, f->ctx->lcol);
    lua_setfield (f->L, -2, "lcol");
    lua_pushlstring (f->L, f->ctx->buf, f->ctx->len);

    switch (f->ctx->subtype)
    {
        case lex_subtype_simple_ident:
            lua_pushlstring (f->L, f->ctx->buf, f->ctx->len);
            lua_getfield (f->L, -1, "lower");
            lua_insert (f->L, lua_absindex (f->L, -2));
            lua_call (f->L, 1, 1);
            break;

        case lex_subtype_quoted_ident:
            push_quoted_lexeme_translated (f->L, f->ctx->len, f->ctx->buf, '"');
            break;

        default:
            if (f->trans_more)
            {
                switch (f->ctx->subtype)
                {
                    case lex_subtype_simple_string:
                        push_quoted_lexeme_translated (
                                f->L, f->ctx->len, f->ctx->buf, '\'');
                        break;

                    // XXX  unimplemented yet:
                    //          ``case lex_subtype_escape_string: ...``

                    case lex_subtype_dollar_string:
                        push_dollar_string_translated (f->L, f->ctx->len,
                                f->ctx->state.dollar_string.marker_len,
                                f->ctx->buf);
                        break;

                    case lex_subtype_sing_line_comment:
                        push_sing_line_comment_translated (
                                f->L, f->ctx->len, f->ctx->buf);
                        break;

                    case lex_subtype_mult_line_comment:
                        push_mult_line_comment_translated (
                                f->L, f->ctx->len, f->ctx->buf);
                        break;

                    default:
                        lua_pushnil (f->L);
                }
            }
            else
            {
                lua_pushnil (f->L);
            }
    }

    lua_call (f->L, 5, 0);
}

static void
finish_lexeme (struct lex_feed_ctx *f)
{
    yield_lexeme (f);

    f->ctx->type = lex_type_undefined;
    f->ctx->subtype = lex_subtype_undefined;
    f->ctx->len = 0;
}

static void
finish_lexeme_with_state (struct lex_feed_ctx *f)
{
    finish_lexeme (f);
    f->ctx->state = (union lex_ctx_state) {};
}

static inline int
undefined_wo_stash (struct lex_feed_ctx *f)
{
    switch (f->c)
    {
        case 'a' ... 'd':
        case 'f' ... 't':
        case 'v' ... 'z':
        case 'A' ... 'D':
        case 'F' ... 'T':
        case 'V' ... 'Z':
        case '_':
            // no cases 'e' 'E' 'u' 'U' here, cause of them f->stash behaviour.
            // no case '0' ... '9' '$' here, cause of starting ident lexeme

            f->ctx->type = lex_type_ident;
            f->ctx->subtype = lex_subtype_simple_ident;
            set_lpos (f->ctx);
            push_c_to_buf (f->L, f->ctx, f->c);
            break;

        case '0' ... '9':
            f->ctx->type = lex_type_number;
            f->ctx->subtype = lex_subtype_number;
            set_lpos (f->ctx);
            push_c_to_buf (f->L, f->ctx, f->c);
            break;

        case '\'':
            f->ctx->type = lex_type_string;
            f->ctx->subtype = lex_subtype_simple_string;
            set_lpos (f->ctx);
            push_c_to_buf (f->L, f->ctx, '\'');
            break;

        case '"':
            f->ctx->type = lex_type_ident;
            f->ctx->subtype = lex_subtype_quoted_ident;
            set_lpos (f->ctx);
            push_c_to_buf (f->L, f->ctx, '"');
            break;

        case '$':
            f->ctx->type = lex_type_string;
            f->ctx->subtype = lex_subtype_dollar_string;
            set_lpos (f->ctx);
            push_c_to_buf (f->L, f->ctx, '$');
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
            // no case '.' here, cause of its f->stash behaviour

            f->ctx->type = lex_type_symbols;
            f->ctx->subtype = lex_subtype_special_symbols;
            set_lpos (f->ctx);
            push_c_to_buf (f->L, f->ctx, f->c);
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
            // no cases '-' '+' '/' here, cause of them f->stash behaviour

            f->ctx->type = lex_type_symbols;
            f->ctx->subtype = lex_subtype_random_symbols;
            set_lpos (f->ctx);
            push_c_to_buf (f->L, f->ctx, f->c);
            break;

        case '-':
        case '+':
        case '/':
        case '.':
        case 'e':
        case 'E':
        case 'u':
        case 'U':
            f->ctx->stash = f->c;
            break;

        case ' ':
        case '\t':
        case '\v':
        case '\n':
        case '\r':
            break;

        case 0:
            f->ctx->pos = 0;
            f->ctx->line = 0;
            f->ctx->col = 0;
            f->ctx->ppos = 0;
            f->ctx->pline = 0;
            f->ctx->pcol = 0;
            f->ctx->lpos = 0;
            f->ctx->lline = 0;
            f->ctx->lcol = 0;
            break;

        case '\\':
            luaL_error (f->L,
                    "pos(%I) line(%I) col(%I): "
                    "lexeme type started with \"\\\" "
                    "is forbidden",
                    f->ctx->pos, f->ctx->line, f->ctx->col);
            __builtin_unreachable ();

        default:
            luaL_error (f->L,
                    "pos(%I) line(%I) col(%I): "
                    "unknown lexeme type started with character: "
                    "%d %c",
                    f->ctx->pos, f->ctx->line, f->ctx->col, f->c, f->c);
            __builtin_unreachable ();
    }

    return 0;
}

static inline int
undefined_with_stash (struct lex_feed_ctx *f)
{
    switch (f->stash)
    {
        case '-':
        case '+':
        case '/':
        case '.':
            if (f->stash == '-' && f->c == '-')
            {
                f->ctx->type = lex_type_comment;
                f->ctx->subtype = lex_subtype_sing_line_comment;
                set_plpos (f->ctx);
                push_str_to_buf (f->L, f->ctx, "--", 2);
            }
            else if (f->stash == '/' && f->c == '*')
            {
                f->ctx->type = lex_type_comment;
                f->ctx->subtype = lex_subtype_mult_line_comment;
                set_plpos (f->ctx);
                push_str_to_buf (f->L, f->ctx, "/*", 2);
            }
            else if ((f->stash == '-' || f->stash == '.') &&
                    f->c >= '0' && f->c <= '9')
            {
                // XXX  we don't support numbers like ``-.123`` yet

                f->ctx->type = lex_type_number;
                f->ctx->subtype = lex_subtype_number;
                set_plpos (f->ctx);
                push_c_to_buf (f->L, f->ctx, f->stash);
                push_c_to_buf (f->L, f->ctx, f->c);
            }
            else if (f->stash == '.')
            {
                f->ctx->type = lex_type_symbols;
                f->ctx->subtype = lex_subtype_special_symbols;
                set_plpos (f->ctx);
                push_c_to_buf (f->L, f->ctx, f->stash);
                return 1;
            }
            else
            {
                f->ctx->type = lex_type_symbols;
                f->ctx->subtype = lex_subtype_random_symbols;
                set_plpos (f->ctx);
                push_c_to_buf (f->L, f->ctx, f->stash);
                return 1;
            }
            break;

        case 'e':
        case 'E':
            if (f->c == '\'')
            {
                f->ctx->type = lex_type_string;
                f->ctx->subtype = lex_subtype_escape_string;
                set_plpos (f->ctx);
                push_c_to_buf (f->L, f->ctx, f->stash);
                push_c_to_buf (f->L, f->ctx, f->c);
            }
            else
            {
                f->ctx->type = lex_type_ident;
                f->ctx->subtype = lex_subtype_simple_ident;
                set_plpos (f->ctx);
                push_c_to_buf (f->L, f->ctx, f->stash);
                return 1;
            }
            break;

        case 'u':
        case 'U':
            if (f->c == '&')
            {
                luaL_error (f->L,
                        "pos(%I) line(%I) col(%I): "
                        "lexeme type started with \"u&\" "
                        "is not supported yet",
                        f->ctx->pos, f->ctx->line, f->ctx->col);
                __builtin_unreachable ();
            }
            else
            {
                f->ctx->type = lex_type_ident;
                f->ctx->subtype = lex_subtype_simple_ident;
                set_plpos (f->ctx);
                push_c_to_buf (f->L, f->ctx, f->stash);
                return 1;
            }
            break;

        default:
            luaL_error (f->L,
                    "pos(%I) line(%I) col(%I): "
                    "unknown lexeme type started with characters: "
                    "%d %d %c %c",
                    f->ctx->pos, f->ctx->line, f->ctx->col,
                    f->stash, f->c, f->stash, f->c);
            __builtin_unreachable ();
    }

    return 0;
}

static inline int
simple_ident (struct lex_feed_ctx *f)
{
    switch (f->c)
    {
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '_':
        case '0' ... '9':
        case '$':
            push_c_to_buf (f->L, f->ctx, f->c);
            break;

        default:
            finish_lexeme (f);
            return 1;
    }

    return 0;
}

static inline int
quoted_ident_wo_stash (struct lex_feed_ctx *f)
{
    switch (f->c)
    {
        case '"':
            if (__builtin_expect (!f->ctx->len, 0))
            {
                fprintf (stderr, "unexpected program flow\n");
                abort ();
            }

            f->ctx->stash = f->c;
            break;

        case 0:
            luaL_error (f->L,
                    "pos(%I) line(%I) col(%I): "
                    "unterminated lexeme: quoted_ident",
                    f->ctx->pos, f->ctx->line, f->ctx->col);
            __builtin_unreachable ();

        default:
            push_c_to_buf (f->L, f->ctx, f->c);
    }

    return 0;
}

static inline int
quoted_ident_with_stash (struct lex_feed_ctx *f)
{
    if (__builtin_expect (f->stash != '"', 0)) {
        fprintf (stderr, "unexpected program flow\n");
        abort ();
    }

    if (f->c == '"')
    {
        push_str_to_buf (f->L, f->ctx, "\"\"", 2);
    }
    else
    {
        push_c_to_buf (f->L, f->ctx, f->stash);
        finish_lexeme (f);
        return 1;
    }

    return 0;
}

static inline int
number_wo_stash (struct lex_feed_ctx *f)
{
    switch (f->c)
    {
        case '-':
        case '+':
            if (f->ctx->len &&
                    f->ctx->len != f->ctx->state.number.e_len)
            {
                finish_lexeme_with_state (f);
                return 1;
            }
            else
            {
                f->ctx->stash = f->c;
                break;
            }

        case '0' ... '9':
            push_c_to_buf (f->L, f->ctx, f->c);
            break;

        case 'e':
        case 'E':
            if (f->ctx->state.number.e_len)
            {
                finish_lexeme_with_state (f);
                return 1;
            }
            else
            {
                f->ctx->stash = f->c;
                break;
            }

        case '.':
            if (f->ctx->state.number.has_dot)
            {
                finish_lexeme_with_state (f);
                return 1;
            }
            else
            {
                f->ctx->stash = f->c;
                break;
            }

        default:
            finish_lexeme_with_state (f);
            return 1;
    }

    return 0;
}

static inline int
number_with_stash (struct lex_feed_ctx *f)
{
    switch (f->stash)
    {
        case '-':
        case '+':
            switch (f->c)
            {
                case '.':
                    if (f->ctx->state.number.e_len)
                    {
                        finish_lexeme_with_state (f);
                        return 2;
                    }
                    __attribute__ ((fallthrough));
                case '0' ... '9':
                    push_c_to_buf (f->L, f->ctx, f->stash);
                    push_c_to_buf (f->L, f->ctx, f->c);
                    break;

                default:
                    finish_lexeme_with_state (f);
                    return 2;
            }
            break;

        case '.':
            if (f->c == '.')
            {
                finish_lexeme_with_state (f);
                return 2;
            }
            else
            {
                push_c_to_buf (f->L, f->ctx, f->stash);
                f->ctx->state.number.has_dot = 1;
                return 1;
            }

        case 'e':
        case 'E':
            switch (f->c)
            {
                case '-':
                case '+':
                case '0' ... '9':
                    push_c_to_buf (f->L, f->ctx, f->stash);
                    f->ctx->state.number.e_len = f->ctx->len;
                    f->ctx->state.number.has_dot = 1;

                    if (f->c == '-' || f->c == '+')
                    {
                        return 1;
                    }
                    else
                    {
                        push_c_to_buf (f->L, f->ctx, f->c);
                        break;
                    }

                default:
                    finish_lexeme_with_state (f);
                    return 2;
            }
            break;

        default:
            fprintf (stderr, "unexpected program flow\n");
            abort ();
    }

    return 0;
}

static inline int
simple_string_wo_stash (struct lex_feed_ctx *f)
{
    switch (f->c)
    {
        case '\'':
            if (__builtin_expect (!f->ctx->len, 0))
            {
                fprintf (stderr, "unexpected program flow\n");
                abort ();
            }

            f->ctx->stash = f->c;
            break;

        case 0:
            luaL_error (f->L,
                    "pos(%I) line(%I) col(%I): "
                    "unterminated lexeme: simple_string",
                    f->ctx->pos, f->ctx->line, f->ctx->col);
            __builtin_unreachable ();

        default:
            push_c_to_buf (f->L, f->ctx, f->c);
    }

    return 0;
}

static inline int
escape_string_wo_stash (struct lex_feed_ctx *f)
{
    switch (f->c)
    {
        case '\'':
            if (__builtin_expect (f->ctx->len < 2, 0))
            {
                fprintf (stderr, "unexpected program flow\n");
                abort ();
            }

            f->ctx->stash = f->c;
            break;

        case 0:
            luaL_error (f->L,
                    "pos(%I) line(%I) col(%I): "
                    "unterminated lexeme: escape_string",
                    f->ctx->pos, f->ctx->line, f->ctx->col);
            __builtin_unreachable ();

        default:
            push_c_to_buf (f->L, f->ctx, f->c);
    }

    return 0;
}

static inline int
simple_or_escape_string_with_stash (struct lex_feed_ctx *f)
{
    if (__builtin_expect (f->stash != '\'', 0)) {
        fprintf (stderr, "unexpected program flow\n");
        abort ();
    }

    if (f->c == '\'')
    {
        push_str_to_buf (f->L, f->ctx, "''", 2);
    }
    else
    {
        push_c_to_buf (f->L, f->ctx, f->stash);
        finish_lexeme (f);
        return 1;
    }

    return 0;
}

static inline int
dollar_string (struct lex_feed_ctx *f)
{
    if (__builtin_expect (f->c == 0, 0))
    {
        luaL_error (f->L,
                "pos(%I) line(%I) col(%I): "
                "unterminated lexeme: dollar_string ",
                f->ctx->pos, f->ctx->line, f->ctx->col);
        __builtin_unreachable ();
    }

    push_c_to_buf (f->L, f->ctx, f->c);

    if (f->c != '$')
    {
        return 0;
    }

    long len = f->ctx->len;
    long marker_len = f->ctx->state.dollar_string.marker_len;
    const char *buf = f->ctx->buf;

    if (!marker_len)
    {
        if (__builtin_expect (len < 2, 0))
        {
            fprintf (stderr, "unexpected program flow\n");
            abort ();
        }

        f->ctx->state.dollar_string.marker_len = len;
    }
    else if (len >= marker_len * 2 &&
            !memcmp (buf + len - marker_len, buf, marker_len))
    {
        finish_lexeme_with_state (f);
    }

    return 0;
}

static inline int
special_symbols (struct lex_feed_ctx *f)
{
    if (__builtin_expect (!f->ctx->len, 0))
    {
        fprintf (stderr, "unexpected program flow\n");
        abort ();
    }

    if (f->ctx->len == 1 && (f->c == '.' || f->c == ':') &&
                f->ctx->buf[0] == f->c)
    {
        push_c_to_buf (f->L, f->ctx, f->c);
        finish_lexeme (f);
    }
    else
    {
        finish_lexeme (f);
        return 1;
    }

    return 0;
}

static inline int
random_symbols (struct lex_feed_ctx *f)
{
    switch (f->c)
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
            push_c_to_buf (f->L, f->ctx, f->c);
            break;

        default:
            finish_lexeme (f);
            return 1;
    }

    return 0;
}

static inline int
sing_line_comment (struct lex_feed_ctx *f)
{
    switch (f->c)
    {
        case '\n':
        case 0:
            finish_lexeme (f);
            return 1;

        default:
            push_c_to_buf (f->L, f->ctx, f->c);
    }

    return 0;
}

static inline int
mult_line_comment_wo_stash (struct lex_feed_ctx *f)
{
    switch (f->c)
    {
        case '*':
            if (__builtin_expect (f->ctx->len < 2, 0))
            {
                fprintf (stderr, "unexpected program flow\n");
                abort ();
            }

            f->ctx->stash = f->c;
            break;

        case 0:
            luaL_error (f->L,
                    "pos(%I) line(%I) col(%I): "
                    "unterminated lexeme: mult_line_comment",
                    f->ctx->pos, f->ctx->line, f->ctx->col);
            __builtin_unreachable ();

        default:
            push_c_to_buf (f->L, f->ctx, f->c);
    }

    return 0;
}

static inline int
mult_line_comment_with_stash (struct lex_feed_ctx *f)
{
    if (__builtin_expect (f->stash != '*', 0)) {
        fprintf (stderr, "unexpected program flow\n");
        abort ();
    }

    if (f->c == '/')
    {
        push_str_to_buf (f->L, f->ctx, "*/", 2);
        finish_lexeme (f);
    }
    else
    {
        push_c_to_buf (f->L, f->ctx, f->stash);
        return 1;
    }

    return 0;
}

static int
lex_feed (lua_State *L)
{
    struct lex_feed_ctx f = {
        .L = L,
        .ctx = luaL_checkudata (L, 1, lex_ctx_tname),
        .not_exists_callback = lua_isnil (L, 3), // arg: callback function
        .trans_more = lua_toboolean (L, 4), // arg: translate more?
    };
    size_t input_len = 0;
    const char *input = lua_tolstring (L, 2, &input_len);
    const int jmps[] = {0, &&retry_c - &&retry_c, &&retry_c - &&retry_stash};
    int jmp;

    if (__builtin_expect (!input_len, 0))
    {
        // the final mark for flushing rest of buffer.
        // it should not be counted in position counters

        input = "\0";
        input_len = 1;
    }

    for (size_t input_i = 0; input_i < input_len; ++input_i)
    {
        f.c = input[input_i];

        if (__builtin_expect (f.c, 1))
        {
            f.ctx->ppos = f.ctx->pos;
            f.ctx->pline = f.ctx->line;
            f.ctx->pcol = f.ctx->col;

            if (!f.ctx->line) f.ctx->line = 1;

            ++f.ctx->pos;

            if (f.c == '\n')
            {
                ++f.ctx->line;
                f.ctx->col = 0;
            }
            else
            {
                ++f.ctx->col;
            }
        }

retry_c:
        f.stash = f.ctx->stash;
        f.ctx->stash = 0;

retry_stash:
        jmp = 0;

        switch (f.ctx->subtype)
        {
            case lex_subtype_undefined:
                if (f.stash) jmp = undefined_with_stash (&f);
                else jmp = undefined_wo_stash (&f);
                break;

            case lex_subtype_simple_ident:
                jmp = simple_ident (&f);
                break;

            case lex_subtype_quoted_ident:
                if (f.stash) jmp = quoted_ident_with_stash (&f);
                else jmp = quoted_ident_wo_stash (&f);
                break;

            case lex_subtype_number:
                if (f.stash) jmp = number_with_stash (&f);
                else jmp = number_wo_stash (&f);
                break;

            case lex_subtype_simple_string:
                if (f.stash) jmp = simple_or_escape_string_with_stash (&f);
                else jmp = simple_string_wo_stash (&f);
                break;

            case lex_subtype_escape_string:
                if (f.stash) jmp = simple_or_escape_string_with_stash (&f);
                else jmp = escape_string_wo_stash (&f);
                break;

            case lex_subtype_dollar_string:
                jmp = dollar_string (&f);
                break;

            case lex_subtype_special_symbols:
                jmp = special_symbols (&f);
                break;

            case lex_subtype_random_symbols:
                jmp = random_symbols (&f);
                break;

            case lex_subtype_sing_line_comment:
                jmp = sing_line_comment (&f);
                break;

            case lex_subtype_mult_line_comment:
                if (f.stash) jmp = mult_line_comment_with_stash (&f);
                else jmp = mult_line_comment_wo_stash (&f);
                break;

            default:
                fprintf (stderr, "unexpected program flow\n");
                abort ();
        }

        if (jmp) goto *(&&retry_c + jmps[jmp]);
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
