#include <lua.h>
#include <lauxlib.h>

#include "pg-dump-splitter.h"

void
open_pg_dump_splitter (lua_State *L)
{
    luaL_requiref (L, "os_ext", luaopen_os_ext, 0);
    luaL_requiref (L, "lex", luaopen_lex, 0);
    luaL_requiref (L, "sort_chunks", luaopen_sort_chunks, 0);
    luaL_requiref (L, "split_to_chunks_pattern_rules",
            luaopen_split_to_chunks_pattern_rules, 0);
    luaL_requiref (L, "split_to_chunks", luaopen_split_to_chunks, 0);

    lua_pop (L, 5);

    luaL_requiref (L, "pg_dump_splitter", luaopen_pg_dump_splitter, 0);
}

// vi:ts=4:sw=4:et
