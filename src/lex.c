// abort, realloc, free
#include <stdlib.h>

// memcpy
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
    // TODO     ... union for each specific type
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
    struct lex_ctx *ctx = luaL_checkudata (L, 1, lex_ctx_tname);
    size_t input_len = 0;
    const char *input = lua_tolstring (L, 2, &input_len);
    int not_exists_callback = lua_isnil (L, 3); // arg: callback function
    int trans_more = lua_toboolean (L, 4); // arg: translate more?

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
            // TODO     case simple ident, quote ident

            default:
                if (trans_more)
                {
                    switch (ctx->subtype)
                    {


                        // TODO ... ... ...

                        case lex_subtype_sing_line_comment:
                            push_sing_line_comment_translated (
                                    L, ctx->len, ctx->buf);
                            break;

                        case lex_subtype_mult_line_comment:
                            push_mult_line_comment_translated (
                                    L, ctx->len, ctx->buf);
                            break;

                        default:
                            lua_pushnil (L); // translated value is unknown
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
        set_lpos (ctx);
        ctx->len = 0;
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
        __label__ retry_c;
        char stash;
        char c = input[input_i];

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
                case '+':
                case '=':
                case '|':
                case '<':
                case '>':
                case '?':
                    // no cases '-' '/' here, cause of them stash behaviour

                    ctx->type = lex_type_symbols;
                    ctx->subtype = lex_subtype_random_symbols;
                    set_lpos (ctx);
                    push_c_to_buf (L, ctx, c);
                    break;

                case '-':
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
        sing_line_comment () {
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
            if (stash == '*' && c == '/')
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


                // TODO ... ...


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
