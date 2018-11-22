conf_data = configuration_data()
conf_data.set_quoted('PG_DUMP_SPLITTER_NAME', meson.project_name())
conf_data.set_quoted('PG_DUMP_SPLITTER_VERSION', meson.project_version())
configure_file(output : 'pg-dump-splitter-config.h',
               configuration : conf_data)

git_rev_c = vcs_tag(command : ['git', 'describe', '--always', '--long', '--dirty'],
                  input : 'git-rev.c.in',
                  output : 'git-rev.c')

lua_emb_src = embedder_lua_gen.process(
  'pg_dump_splitter.lua',
  'split_to_chunks.lua',
  'split_to_chunks_pattern_rules.lua',
  'sort_chunks.lua',
)

sources = [
  'main.c',
  'bootstrap.c',
  'emb-libs.c',
  'os-ext.c',
  'lex.c',
  lua_emb_src,
  git_rev_c,
]

executable('pg-dump-splitter', sources,
           include_directories : inc,
           dependencies : [lua_dep, sys_dep],
           install : true)

if make_lib_opt
  shared_sources = [
    'emb-libs.c',
    'os-ext.c',
    'lex.c',
    lua_emb_src,
  ]

  libs = shared_library('pg-dump-splitter', shared_sources,
                 include_directories : inc,
                 dependencies : lua_dep,
                 install : true)
endif

# vi:ts=2:sw=2:et
