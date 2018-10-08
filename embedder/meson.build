# vim: set et ts=2 sw=2:

embedder_py = find_program('embedder.py')

embedder_gen = generator(
  embedder_py,
  arguments : ['@PLAINNAME@', '@INPUT@', '@OUTPUT0@', '@OUTPUT1@'],
  output : ['@PLAINNAME@.c', '@PLAINNAME@.h'],
)
