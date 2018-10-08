# vim: set et ts=2 sw=2:

conf_data = configuration_data()
conf_data.set_quoted('PG_DUMP_SPLITTER_NAME', meson.project_name())
conf_data.set_quoted('PG_DUMP_SPLITTER_VERSION', meson.project_version())
configure_file(output : 'pg-dump-splitter-config.h',
               configuration : conf_data)

git_rev_c = vcs_tag(command : ['git', 'describe', '--always', '--long', '--dirty'],
                  input : 'git-rev.c.in',
                  output : 'git-rev.c')

lua_gen_src = embedder_gen.process(
  'pg-dump-splitter.lua',
)

sources = [
  'main.c',
  git_rev_c,
  lua_gen_src,
]

executable('pg-dump-splitter', sources,
           install : true)
