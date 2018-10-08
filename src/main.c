// vim: set et ts=4 sw=4:

// free
#include <stdlib.h>

// strdup
#include <string.h>

// fprintf
#include <stdio.h>

#include <argp.h>
#include "pg-dump-splitter-config.h"
#include "git-rev.h"

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
main (int argc, char *argv[])
{
    struct arguments arguments = {};

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    // DEBUG ONLY
    printf ("d: %s\n", arguments.dump_path);
    printf ("o: %s\n", arguments.output_dir);
    printf ("k: %s\n", arguments.hooks_path);

    free (arguments.dump_path);
    free (arguments.output_dir);
    free (arguments.hooks_path);
}
