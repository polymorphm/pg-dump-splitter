// vim: set et ts=4 sw=4:

// free
#include <stdlib.h>

// strdup
#include <string.h>

// fprintf, stderr
#include <stdio.h>

#include <argp.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "pg-dump-splitter-config.h"
#include "git-rev.h"
#include "pg_dump_splitter.lua.h"

static const char *const argp_doc = "Splits Postgresql's dump file "
        "for easily using source code comparing tools "
        "to its data. You are able to use this utility "
        "as a part of big automate script.";

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
    char *dump_path;
    char *output_dir;
    char *hooks_path;
};

static struct argp_option argp_options[] =
{
    {
        .name = "hooks",
        .key = 'k',
        .arg = "LUA-FILE",
        .doc = "Path to a lua file, the script which defines hooks"
    },
    {},
};

static error_t
argp_parser (int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;

    switch (key)
    {
        case 'k':
            if (arguments->hooks_path) {
                argp_error (state,
                        "attempt to redefine argument for option \"hooks\"");

                return EINVAL;
            }

            arguments->hooks_path = strdup (arg);

            break;

        case ARGP_KEY_ARG:
            if (state->arg_num >= 2) {
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
            if (state->arg_num < 2) {
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
    .doc = argp_doc,
};

int
luaopen_pg_dump_splitter (lua_State *L)
{
    const char *modname = luaL_checkstring (L, 1);

    int lua_err = luaL_loadbuffer (L,
            EMBEDDED_PG_DUMP_SPLITTER_LUA_DATA,
            EMBEDDED_PG_DUMP_SPLITTER_LUA_SIZE,
            modname);

    if (lua_err) {
        return lua_error (L);
    }

    lua_pushvalue (L, 1);
    lua_call (L, 1, 1);

    return 1;
}

static int
bootstrap (lua_State *L)
{
    // TODO other calls of luaL_requiref (...) here, then lua_pop (L, 1);

    luaL_requiref (
            L, "pg_dump_splitter", luaopen_pg_dump_splitter, 0);

    lua_getfield (L, -1, "pg_dump_splitter");

    lua_pushvalue (L, 1);
    lua_pushvalue (L, 2);
    lua_pushvalue (L, 3);

    lua_call (L, 3, 0);

    return 0;
}

int
main (int argc, char *argv[])
{
    struct arguments arguments = {};

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    int exit_code = 0;

    lua_State *L = luaL_newstate ();
    luaL_openlibs (L);

    lua_pushcfunction (L, bootstrap);

    lua_pushstring (L, arguments.dump_path);
    free (arguments.dump_path);
    arguments.dump_path = 0;

    lua_pushstring (L, arguments.output_dir);
    free (arguments.output_dir);
    arguments.output_dir = 0;

    lua_pushstring (L, arguments.hooks_path);
    free (arguments.hooks_path);
    arguments.hooks_path = 0;

    int lua_err = lua_pcall (L, 3, 0, 0);

    if (lua_err) {
        fprintf (stderr, "%s\n", lua_tostring(L, -1));
        exit_code = 1;
    }

    lua_close (L);

    return exit_code;
}
