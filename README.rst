pg-dump-splitter
================

``pg-dump-splitter`` is a utility to split Postgresql's dump file for easily
using source code comparing tools to its data. You are able to use this utility
as a part of big automate script.

Compiling
---------

::

   $ meson --buildtype=release --prefix=/opt/pg-dump-splitter builddir

   $ cd builddir

   $ meson configure

   # you could also setup warning level like this
   #
   #$ meson configure --warnlevel=2
   #
   # or change other build parameters

   $ ninja

   $ sudo ninja install
