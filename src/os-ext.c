#include <lua.h>
#include <lauxlib.h>

// errono
#include <errno.h>

// strerror_l
#include <string.h>

// mkdir S_I*
#include <sys/stat.h>

#include "pg-dump-splitter.h"

static int
os_ext_mkdir (lua_State *L)
{
    const char *dir_path = luaL_checkstring (L, 1);

    int status = mkdir (dir_path, 0777);

    if (status)
    {
        lua_pushboolean (L, 0);
        lua_pushstring (L, strerror_l (errno, 0));

        return 2;
    }

    lua_pushboolean (L, 1);

    return 1;
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

// vi:ts=4:sw=4:et
