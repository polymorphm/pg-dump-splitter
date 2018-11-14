pg-dump-splitter
================

``pg-dump-splitter`` is a utility to split Postgresql's dump file for easily
using source code comparing tools to its data. You are able to use this utility
as a part of big automate script.

Examples of Using
-----------------

Dumping database::

   $ pg_dump -sfdump.sql -- 'user=postgres dbname=postgres'

An example of simple using pg-dump-splitter::

   $ pg_dump_splitter -- dump.sql db_objects

An example with turning of excluding some parts and suppressing unprocessed
errors::

   $ pg_dump_splitter --save-unprocessed -kexcluding-obj-hook.lua -- dump.sql db_objects

``db_objects`` will contain grouped parts of ``dump.sql``. To write the
file ``excluding-obj-hook.lua`` see ``EXAMPLE.excluding-obj-hook.lua`` in
source code's directory.

Building: A Short Story
-----------------------

All things are by default, apart of optimization level for GCC.
All dependencies are from distrib's repository::

   $ meson --buildtype=release --prefix=/opt/pg-dump-splitter builddir

   $ cd builddir

   $ ninja

   $ sudo ninja install

Building: A Long Story
----------------------

Building with manually downloaded lua-library.
And turning on precompilation of lua-scripts::

   $ pushd ..

   $ curl https://www.lua.org/ftp/lua-5.3.5.tar.gz -O lua-5.3.5.tar.gz

   $ tar -xaflua-5.3.5.tar.gz

   $ cd lua-5.3.5

   $ make INSTALL_TOP="`pwd`/../lua" linux install

   $ mkdir -p ../pkgconfig

   $ make INSTALL_TOP="`pwd`/../lua" pc >../pkgconfig/lua.pc

   $ cat <<'__END__' >>../pkgconfig/lua.pc
   Name: Lua
   Description: An Extensible Extension Language
   Version: ${version}
   Requires:
   Libs: -L${libdir} -llua -lm -ldl
   Cflags: -I${includedir}
   __END__

   $ export PKG_CONFIG_LIBDIR="`pwd`/../pkgconfig"

   $ popd

   $ meson \
         --buildtype=release \
         --warnlevel=2 \
         --prefix=/opt/pg-dump-splitter \
         -Dluac="`pwd`/../lua/bin/luac" \
         builddir

   $ cd builddir

   $ ninja

   $ sudo ninja install

   $ unset PKG_CONFIG_LIBDIR

How to Add a New Pattern Rule to the Utility?
---------------------------------------------

If some rule turn out to be missing, it isn't hard to add it to the utility.
In typical case you need only to put a few lines to two places of the source
code.

An example of adding ``CREATE TRIGGER`` rule::

   diff --git a/src/split_to_chunks_pattern_rules.lua b/src/split_to_chunks_pattern_rules.lua
   index 4c0892f..4cd3999 100644
   --- a/src/split_to_chunks_pattern_rules.lua
   +++ b/src/split_to_chunks_pattern_rules.lua
   @@ -534,6 +534,26 @@ function export.make_pattern_rules(handlers)
          {en},
        },

   +    {
   +      'create_trigger',
   +      {kw, 'create'},
   +      {kw, 'trigger'},
   +      {ident, 'obj_name'},
   +      {any},
   +      {kw, 'on'},
   +      {
   +        fork,
   +        {
   +          {ident, 'rel_schema'},
   +          {ss, '.'},
   +        },
   +        {},
   +      },
   +      {ident, 'rel_name'},
   +      {any},
   +      {en},
   +    },
   +
        {
          'create_cast',
          {kw, 'create'},
   diff --git a/src/sort_chunks.lua b/src/sort_chunks.lua
   index f269cc8..7301461 100644
   --- a/src/sort_chunks.lua
   +++ b/src/sort_chunks.lua
   @@ -108,6 +108,7 @@ function export.make_sort_rules(options)
        {'alter_sequence', reg, 'SEQUENCE'},

        {'create_index', rel, 'TABLE'},
   +    {'create_trigger', rel, 'TABLE'},

        {'comment_schema', schema, 'SCHEMA'},
        {'comment_extension', reg, 'EXTENSION'},

Fixing of the first place is needed to defining a pattern structure. and
fixing the second place is needed to defining the way of saving data to fs
tree.

But if recompilation of the utility isn't possible, you still can use
handlers registered in ``hooks.lua`` to making decorators to the functions
above that make them more rich by patterns you needed. Building the utility
from modified sources isn't required in this case.
