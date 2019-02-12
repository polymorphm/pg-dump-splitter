// setlocale, LC_CTYPE
#include <locale.h>

// abort, free
#include <stdlib.h>

// wprintf, fwprintf, stderr
#include <stdio.h>

// wchar_t, wcslen, wcscmp, wcsdup
#include <wchar.h>

#include <lua.h>
#include <lauxlib.h>

// luaL_openlibs
#include <lualib.h>

#include "pg-dump-splitter-config.h"
#include "git-rev.h"

#include "pg-dump-splitter.h"
#include "os-helpers-winapi.h"

static void
wprint_version (void)
{
    wchar_t *name_wcs =
            pds_os_helpers_make_wcs_from_mbs (PG_DUMP_SPLITTER_NAME);
    wchar_t *version_wcs =
            pds_os_helpers_make_wcs_from_mbs (PG_DUMP_SPLITTER_VERSION);
    wchar_t *revision_wcs =
            pds_os_helpers_make_wcs_from_mbs (pg_dump_splitter_git_rev);

    wprintf (L"%ls %ls (%ls)\n", name_wcs, version_wcs, revision_wcs);

    free (name_wcs);
    free (version_wcs);
    free (revision_wcs);
}

static void
cut_off_extra_dir_slash (wchar_t *dir_path)
{
    // deleting extra slash from directory's path.
    // for example path '/path/to/dir/' would be transformed to '/path/to/dir'

    size_t len = wcslen (dir_path);

    while (len && (dir_path[len - 1] == L'/' || dir_path[len - 1] == L'\\'))
    {
        --len;
        dir_path[len] = 0;
    }
}

struct arguments
{
    int save_unprocessed;
    int no_schema_dirs;
    int relaxed_order;
    int split_stateless;
    wchar_t *sql_footer;
    wchar_t *dump_path;
    wchar_t *output_dir;
    wchar_t *hooks_path;
};

static int
parse_args (struct arguments *arguments, int argc, wchar_t *argv[])
{
    int no_opts = 0;
    int arg_num = 0;

    for (int i = 1; i < argc; ++i)
    {
        const wchar_t *arg = argv[i];

        if (!no_opts && arg[0] == L'-')
        {
            const wchar_t *next_arg;

            if (i + 1 < argc) next_arg = argv[i + 1];
            else next_arg = 0;

            if (!wcscmp (L"--", arg))
            {
                no_opts = 1;
                continue;
            }
            if (!wcscmp (L"-V", arg))
            {
                wprint_version ();
                return -1;
            }

            if (!wcscmp (L"-u", arg) || !wcscmp (L"--save-unprocessed", arg))
            {
                arguments->save_unprocessed = 1;
                continue;
            }
            if (!wcscmp (L"-S", arg) || !wcscmp (L"--no-schema-dirs", arg))
            {
                arguments->no_schema_dirs = 1;
                continue;
            }
            if (!wcscmp (L"-O", arg) || !wcscmp (L"--relaxed-order", arg))
            {
                arguments->relaxed_order = 1;
                continue;
            }
            if (!wcscmp (L"-T", arg) || !wcscmp (L"--split-stateless", arg))
            {
                arguments->split_stateless = 1;
                continue;
            }
            if (!wcscmp (L"-f", arg) || !wcscmp (L"--sql-footer", arg))
            {
                if (!next_arg)
                {
                    fwprintf (stderr,
                            L"option requires an argument: %ls", arg);
                    return 1;
                }
                if (arguments->sql_footer)
                {
                    fwprintf (stderr,
                            L"attempt to redefine argument for option: %ls",
                            arg);
                    return 1;
                }

                arguments->sql_footer = wcsdup (next_arg);
                ++i;
                continue;
            }
            if (!wcscmp (L"-k", arg) || !wcscmp (L"--hooks", arg))
            {
                if (!next_arg)
                {
                    fwprintf (stderr,
                            L"option requires an argument: %ls", arg);
                    return 1;
                }
                if (arguments->hooks_path)
                {
                    fwprintf (stderr,
                            L"attempt to redefine argument for option: %ls",
                            arg);
                    return 1;
                }

                arguments->hooks_path = wcsdup (next_arg);
                ++i;
                continue;
            }

            fwprintf (stderr, L"unrecognized option: %ls\n", arg);
            return 1;
        }

        if (arg_num >= 2)
        {
            fwprintf (stderr, L"too many arguments\n");
            return 1;
        }

        switch (arg_num)
        {
            case 0:
                arguments->dump_path = wcsdup (arg);
                break;
            case 1:
                arguments->output_dir = wcsdup (arg);
                cut_off_extra_dir_slash (arguments->output_dir);
                break;
        }

        ++arg_num;
    }

    if (arg_num < 2)
    {
        fwprintf (stderr, L"too few arguments\n");
        return 1;
    }

    return 0;
}

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
    if (lua_toboolean (L, 4)) // arg: split_stateless
    {
        lua_pushboolean (L, 1);
        lua_setfield (L, -2, "split_stateless");
    }
    if (lua_toboolean (L, 5)) // arg: sql_footer
    {
        lua_pushvalue (L, 5);
        lua_setfield (L, -2, "sql_footer");
    }

    lua_getfield (L, -2, "pg_dump_splitter");
    lua_pushvalue (L, 6); // arg: dump_path
    lua_pushvalue (L, 7); // arg: output_dir
    lua_pushvalue (L, 8); // arg: hooks_path
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

    struct arguments arguments = {};
    int exit_code = parse_args (&arguments, argc, argv);

    if (exit_code)
    {
        if (exit_code == -1) exit_code = 0;

        goto out;
    }

    lua_State *L = luaL_newstate ();
    char *mbs;

    if (__builtin_expect (!L, 0))
    {
        fwprintf (stderr, L"memory allocation error for lua state\n");
        abort ();
    }

    luaL_openlibs (L);

    lua_pushcfunction (L, traceback_msgh);
    lua_pushcfunction (L, bootstrap);

    lua_pushboolean (L, arguments.save_unprocessed);
    lua_pushboolean (L, arguments.no_schema_dirs);
    lua_pushboolean (L, arguments.relaxed_order);
    lua_pushboolean (L, arguments.split_stateless);
    mbs = pds_os_helpers_make_mbs_from_wcs (arguments.sql_footer);
    lua_pushstring (L, mbs);
    free (mbs);
    mbs = pds_os_helpers_make_mbs_from_wcs (arguments.dump_path);
    lua_pushstring (L, mbs);
    free (mbs);
    mbs = pds_os_helpers_make_mbs_from_wcs (arguments.output_dir);
    lua_pushstring (L, mbs);
    free (mbs);
    mbs = pds_os_helpers_make_mbs_from_wcs (arguments.hooks_path);
    lua_pushstring (L, mbs);
    free (mbs);

    int lua_err = lua_pcall (L, 8, 0, -10);

    if (lua_err)
    {
        const char *err_mbs = lua_tostring (L, -1);
        wchar_t *err_wcs = pds_os_helpers_make_wcs_from_mbs (err_mbs);

        fwprintf (stderr, L"%ls\n", err_wcs);
        free (err_wcs);
        exit_code = 1;
    }

    lua_close (L);

out:
    free (arguments.hooks_path);
    free (arguments.output_dir);
    free (arguments.dump_path);
    free (arguments.sql_footer);

    return exit_code;
}

// vi:ts=4:sw=4:et
