// setlocale, LC_CTYPE
#include <locale.h>

// abort, free
#include <stdlib.h>

// wprintf, fwprintf, stderr
#include <stdio.h>

// wchar_t
#include <wchar.h>

#include <lua.h>
#include <lauxlib.h>

// luaL_openlibs
#include <lualib.h>

#include "pg-dump-splitter-config.h"
#include "git-rev.h"

#include "pg-dump-splitter.h"
#include "os-helpers-winapi.h"

// TODO     ...

static int
bootstrap (lua_State *L)
{
    open_pg_dump_splitter (L);

    lua_getfield (L, -1, "make_default_options");
    lua_call (L, 0, 1); // returns var: options

    if (lua_toboolean (L, 1)) // arg: save_unprocessed
    {
        lua_pushboolean (L, 1);
        lua_setfield (L, -2, "save_unprocessed");
    }
    if (lua_toboolean (L, 2)) // arg: no_schema_dirs
    {
        lua_pushboolean (L, 1);
        lua_setfield (L, -2, "no_schema_dirs");
    }
    if (lua_toboolean (L, 3)) // arg: relaxed_order
    {
        lua_pushboolean (L, 1);
        lua_setfield (L, -2, "relaxed_order");
    }
    if (lua_toboolean (L, 4)) // arg: sql_footer
    {
        lua_pushvalue (L, 4);
        lua_setfield (L, -2, "sql_footer");
    }

    lua_getfield (L, -2, "pg_dump_splitter");
    lua_pushvalue (L, 5); // arg: dump_path
    lua_pushvalue (L, 6); // arg: output_dir
    lua_pushvalue (L, 7); // arg: hooks_path
    lua_pushvalue (L, -5); // var: options
    lua_call (L, 4, 0);

    return 0;
}

static int
traceback_msgh (lua_State *L)
{
    const char *msg = lua_tostring (L, 1);

    luaL_traceback (L, L, msg, 0);

    return 1;
}

int
wmain (int argc, wchar_t *argv[], wchar_t *envp[] __attribute__ ((unused)))
{
    setlocale (LC_CTYPE, ".65001");

    if (argc != 3) return 1;
    char *dump_path_mbs = pds_os_helpers_make_mbs_from_wcs (argv[1]);
    char *output_dir_mbs = pds_os_helpers_make_mbs_from_wcs (argv[2]);



    // TODO ... .. ...




    int exit_code = 0;

    lua_State *L = luaL_newstate ();

    if (__builtin_expect (!L, 0))
    {
        fwprintf (stderr, L"memory allocation error for lua state\n");
        abort ();
    }

    luaL_openlibs (L);

    lua_pushcfunction (L, traceback_msgh);
    lua_pushcfunction (L, bootstrap);

    lua_pushboolean (L, 1); //lua_pushboolean (L, arguments.save_unprocessed);
    lua_pushboolean (L, 0); //lua_pushboolean (L, arguments.no_schema_dirs);
    lua_pushboolean (L, 0); //lua_pushboolean (L, arguments.relaxed_order);
    lua_pushstring (L, 0); //lua_pushstring (L, arguments.sql_footer);
    lua_pushstring (L, dump_path_mbs); //lua_pushstring (L, arguments.dump_path);
    lua_pushstring (L, output_dir_mbs); //lua_pushstring (L, arguments.output_dir);
    lua_pushstring (L, 0); //lua_pushstring (L, arguments.hooks_path);
    //free (arguments.sql_footer);
    free (dump_path_mbs); //free (arguments.dump_path);
    free (output_dir_mbs); //free (arguments.output_dir);
    //free (arguments.hooks_path);

    int lua_err = lua_pcall (L, 7, 0, -9);

    if (lua_err)
    {
        const char *err_mbs = lua_tostring (L, -1);
        wchar_t *err_wcs = pds_os_helpers_make_wcs_from_mbs (err_mbs);

        fwprintf (stderr, L"%ls\n", err_wcs);
        free (err_wcs);
        exit_code = 1;
    }

    lua_close (L);

    return exit_code;
}

// vi:ts=4:sw=4:et
