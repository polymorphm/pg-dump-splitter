// vim: set et ts=4 sw=4:

// abort, malloc, realloc, free
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>

#include "pg-dump-splitter.h"

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
    lex_subtype_random_symbols,
    lex_subtype_special_symbols,
    lex_subtype_sing_line_comment,
    lex_subtype_mult_line_comment,
};

struct lex_ctx
{
    long max_size;  // limit of lexeme buffer size
    long pos;       // current char position in stream, 1 based
    long line;      // current line position in stream, 1 based
    long col;       // current column position in stream, 1 based
    long lpos;      // current lexeme char position in stream, 1 based
    long lline;     // current lexeme line position in stream, 1 based
    long lcol;      // current lexeme column positon in stream, 1 based
    long size;      // lexeme allocated buffer size
    long len;       // current lexeme length
    char *buf;      // pointer to lexeme buffer, not zero terminated
    char stash;     // stashed char, to return to it next iteration
    int type;       // current lexeme type
    int subtype;    // current lexeme subtype
    // TODO     ... union for each specific type
};




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
    const char *input = luaL_checklstring (L, 2, &input_len);
    luaL_checkany (L, 3); // callback function

    // TODO     ...

    return 0;
}

static int
lex_free (lua_State *L)
{
    struct lex_ctx *ctx = luaL_checkudata (L, 1, lex_ctx_tname);

    free (ctx->buf);
    ctx->buf = 0;

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
