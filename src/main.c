// vim: set et ts=4 sw=4:

#include <stdio.h>
#include <argp.h>
#include "pg-dump-splitter-config.h"

static const char *const argp_doc = "Splits Postgresql's dump file "
        "for easily using source code comparing tools "
        "to its data. You are able to use this utility "
        "as a part of big automate script.";

extern const char *PG_DUMP_SPLITTER_GIT_REV;

static void
argp_print_version (FILE *stream,
        struct argp_state *state __attribute__ ((unused)))
{
    fprintf (stream, "%s (%s)\n",
            PG_DUMP_SPLITTER_VERSION, PG_DUMP_SPLITTER_GIT_REV);
}

void (*argp_program_version_hook) (FILE *, struct argp_state *) = argp_print_version;

static error_t
argp_parser (int key, char *arg, struct argp_state *state)
{
    return 0;
}

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
    error_t parse_error = argp_parse (&argp, argc, argv, 0, 0, 0);

    if (parse_error)
    {
        return parse_error;
    }



}
