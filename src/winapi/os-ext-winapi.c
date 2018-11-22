// abort, calloc, free
#include <stdlib.h>

// fwprintf, swprintf, stderr
#include <stdio.h>

// wchar_t
#include <wchar.h>

#include <lua.h>
#include <lauxlib.h>

#include <windows.h>

#include "os-helpers-winapi.h"

static int
os_ext_mkdir (lua_State *L)
{
    const char *dir_path_mbs = luaL_checkstring (L, 1);
    wchar_t *dir_path_wcs = pds_os_helpers_make_wcs_from_mbs (dir_path_mbs);

    int status = CreateDirectoryW (dir_path_wcs, 0);
    int err = GetLastError ();
    free (dir_path_wcs);

    if (!status)
    {
        wchar_t *err_buf_wcs = pds_os_helpers_strerror (err);
        char *err_buf_mbs = pds_os_helpers_make_mbs_from_wcs (err_buf_wcs);

        lua_pushboolean (L, 0);
        lua_pushstring (L, err_buf_mbs);

        free (err_buf_mbs);
        free (err_buf_wcs);

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
