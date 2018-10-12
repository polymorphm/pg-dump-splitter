// vim: set et ts=4 sw=4:

#include <lua.h>
#include <lauxlib.h>

#include "os-ext.h"

static int
os_ext_mkdir (lua_State *L)
{
    fprintf (stderr, "~~~~~~~~!!!!!!!!!~~~~~~\n");
}

static const luaL_Reg os_ext_reg[] =
{
    {"mkdir", os_ext_mkdir},
    {0, 0},
};

int
luaopen_os_ext (lua_State *L)
{
    luaL_newlib (L, os_ext_reg);

    return 1;
}
