// vim: set et ts=4 sw=4:

#include <lua.h>
#include <lauxlib.h>

static int
lex_make_ctx (lua_State *L)
{
    // TODO     ...

    return 0;//1;
}

static int
lex_feed (lua_State *L)
{
    // TODO     ...

    return 0;
}

static int
lex_free (lua_State *L)
{
    // TODO     ...

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
    luaL_newlib (L, lex_reg);

    return 1;
}
