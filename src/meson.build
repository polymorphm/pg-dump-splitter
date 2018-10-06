# vim: set et ts=2 sw=2:

conf_data = configuration_data()
conf_data.set_quoted('PG_DUMP_SPLITTER_VERSION', meson.project_version())
configure_file(output : 'pg-dump-splitter-config.h',
               configuration : conf_data)

git_rev = vcs_tag(command : ['git', 'describe', '--always', '--long', '--dirty'],
                  input : 'git-rev.c.in',
                  output : 'git-rev.c')

sources = [
  'main.c',
  git_rev,
]

executable('pg-dump-splitter', sources,
           install : true)
