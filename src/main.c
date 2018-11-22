// abort, free
#include <stdlib.h>

// strdup
#include <string.h>

// fprintf, stderr
#include <stdio.h>

#include <argp.h>
#include <lua.h>
#include <lauxlib.h>

// luaL_openlibs
#include <lualib.h>

#include "pg-dump-splitter-config.h"
#include "git-rev.h"

#include "pg-dump-splitter.h"

#define ARGP_DOC ("Splits Postgresql's dump file " \
        "for easily using source code comparing tools " \
        "to its data. You are able to use this utility " \
        "as a part of big automate script.")

static void
argp_print_version (FILE *stream,
        struct argp_state *state __attribute__ ((unused)))
{
    fprintf (stream, "%s %s (%s)\n",
            PG_DUMP_SPLITTER_NAME, PG_DUMP_SPLITTER_VERSION,
            pg_dump_splitter_git_rev);
}

void (*argp_program_version_hook) (FILE *, struct argp_state *) =
        argp_print_version;

struct arguments
{
    int save_unprocessed;
    int no_schema_dirs;
    int relaxed_order;
    char *sql_footer;
    char *dump_path;
    char *output_dir;
    char *hooks_path;
};

static struct argp_option argp_options[] =
{
    {
        .name = "save-unprocessed",
        .key = 701,
        .doc = "Save unprocessed dump chunks instead of yielding error",
    },
    {
        .name = "no-schema-dirs",
        .key = 702,
        .doc = "Alternative file tree layout to saving sorted dump chunks",
    },
    {
        .name = "relaxed-order",
        .key = 703,
        .doc = "Relaxed order of sorting dump chunks",
    },
    {
        .name = "sql-footer",
        .key = 704,
        .arg = "SQL-FOOTER",
        .doc = "A footer that will be added at end of each sorted dump chunk",
    },
    {
        .name = "hooks",
        .key = 'k',
        .arg = "LUA-FILE",
        .doc = "Path to a lua file, the script which defines hooks",
    },
    {},
};

static error_t
argp_parser (int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    switch (key)
    {
        case 701:
            arguments->save_unprocessed = 1;

            break;

        case 702:
            arguments->no_schema_dirs = 1;

            break;

        case 703:
            arguments->relaxed_order = 1;

            break;

        case 704:
            if (arguments->sql_footer)
            {
                argp_error (state,
                        "attempt to redefine argument for option \"sql-footer\"");

                return EINVAL;
            }

            arguments->sql_footer = strdup (arg);

            break;

        case 'k':
            if (arguments->hooks_path)
            {
                argp_error (state,
                        "attempt to redefine argument for option \"hooks\"");

                return EINVAL;
            }

            arguments->hooks_path = strdup (arg);

            break;

        case ARGP_KEY_ARG:
            if (state->arg_num >= 2)
            {
                argp_error (state,
                        "too many arguments");

                return EINVAL;
            }

            switch (state->arg_num)
            {
                case 0:
                    arguments->dump_path = strdup (arg);
                    break;
                case 1:
                    arguments->output_dir = strdup (arg);
                    break;
            }

            break;

        case ARGP_KEY_END:
            if (state->arg_num < 2)
            {
                argp_error (state,
                        "too few arguments");

                return EINVAL;
            }

            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp =
{
    .options = argp_options,
    .parser = argp_parser,
    .args_doc = "INPUT-DUMP-FILE OUTPUT-DIRECTORY",
    .doc = ARGP_DOC,
};

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
main (int argc, char *argv[])
{
    struct arguments arguments = {};

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    int exit_code = 0;

    lua_State *L = luaL_newstate ();

    if (__builtin_expect (!L, 0))
    {
        fprintf (stderr, "memory allocation error for lua state\n");
        abort ();
    }

    luaL_openlibs (L);

    lua_pushcfunction (L, traceback_msgh);
    lua_pushcfunction (L, bootstrap);

    lua_pushboolean (L, arguments.save_unprocessed);
    lua_pushboolean (L, arguments.no_schema_dirs);
    lua_pushboolean (L, arguments.relaxed_order);
    lua_pushstring (L, arguments.sql_footer);
    lua_pushstring (L, arguments.dump_path);
    lua_pushstring (L, arguments.output_dir);
    lua_pushstring (L, arguments.hooks_path);
    free (arguments.sql_footer);
    free (arguments.dump_path);
    free (arguments.output_dir);
    free (arguments.hooks_path);

    int lua_err = lua_pcall (L, 7, 0, -9);

    if (lua_err)
    {
        fprintf (stderr, "%s\n", lua_tostring (L, -1));
        exit_code = 1;
    }

    lua_close (L);

    return exit_code;
}

// vi:ts=4:sw=4:et
