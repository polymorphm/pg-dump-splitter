# vim: set et ts=2 sw=2:

project('pg-dump-splitter', 'c',
        version : 'PRE-1.0')

luac_opt = get_option('luac')

lua_dep = dependency('lua')

if luac_opt != ''
  luac_program = find_program(luac_opt)
else
  luac_program = disabler()
endif

subdir('embedder')
subdir('src')
