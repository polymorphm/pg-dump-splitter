project('pg-dump-splitter', 'c',
        version : 'PRE-1.0')

link_argp_opt = get_option('link-argp')
luac_opt = get_option('luac')
make_lib_opt = get_option('make-lib')
use_winapi_opt = get_option('use-winapi')

lua_dep = dependency('lua')

if link_argp_opt
  sys_dep = meson.get_compiler('c').find_library('argp')
else
  sys_dep = []
endif

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

# vi:ts=2:sw=2:et
