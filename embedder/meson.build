# vim: set et ts=2 sw=2:

embedder_py = find_program('embedder.py')

embedder_gen = generator(
  embedder_py,
  arguments : ['@PLAINNAME@', '@INPUT@', '@OUTPUT0@', '@OUTPUT1@'],
  output : ['@PLAINNAME@.c', '@PLAINNAME@.h'],
)

if luac_program.found()
  embedder_luac_sh = find_program('embedder-luac.sh')

  embedder_luac_gen = generator(
    embedder_luac_sh,
    arguments : [embedder_py.path(), luac_program.path(),
                 '@PLAINNAME@', '@INPUT@', '@OUTPUT0@', '@OUTPUT1@'],
    output : ['@PLAINNAME@.c', '@PLAINNAME@.h'],
  )

  embedder_lua_gen = embedder_luac_gen
else
  embedder_lua_gen = embedder_gen
endif
