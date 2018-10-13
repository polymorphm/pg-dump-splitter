# vim: set et ts=2 sw=2:

project('pg-dump-splitter', 'c',
        version : 'PRE-1.0')

luac_opt = get_option('luac')
make_lib_opt = get_option('make-lib')

lua_dep = dependency('lua')

if luac_opt != ''
  luac_program = find_program(luac_opt)
else
  luac_program = disabler()
endif

inc = include_directories('include')

subdir('include')
subdir('embedder')
subdir('src')

if make_lib_opt
  pkg = import('pkgconfig')

  pkg.generate(
    name : 'pg-dump-splitter',
    libraries : libs,
    description : 'utility to split Postgresql\'s dump file',
  )
endif
