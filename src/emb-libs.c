#include <lua.h>
#include <lauxlib.h>

#include "pg-dump-splitter.h"

#include "pg_dump_splitter.lua.h"
#include "split_to_chunks.lua.h"
#include "split_to_chunks_pattern_rules.lua.h"
#include "sort_chunks.lua.h"

int
luaopen_pg_dump_splitter (lua_State *L)
{
    int lua_err = luaL_loadbuffer (L,
            EMBEDDED_PG_DUMP_SPLITTER_LUA_DATA,
            EMBEDDED_PG_DUMP_SPLITTER_LUA_SIZE,
            "=pg_dump_splitter");

    if (lua_err)
    {
        return lua_error (L);
    }

    lua_pushvalue (L, 1);
    lua_call (L, 1, 1);

    return 1;
}

int
luaopen_split_to_chunks (lua_State *L)
{
    int lua_err = luaL_loadbuffer (L,
            EMBEDDED_SPLIT_TO_CHUNKS_LUA_DATA,
            EMBEDDED_SPLIT_TO_CHUNKS_LUA_SIZE,
            "=split_to_chunks");

    if (lua_err)
    {
        return lua_error (L);
    }

    lua_pushvalue (L, 1);
    lua_call (L, 1, 1);

    return 1;
}

int
luaopen_split_to_chunks_pattern_rules (lua_State *L)
{
    int lua_err = luaL_loadbuffer (L,
            EMBEDDED_SPLIT_TO_CHUNKS_PATTERN_RULES_LUA_DATA,
            EMBEDDED_SPLIT_TO_CHUNKS_PATTERN_RULES_LUA_SIZE,
            "=split_to_chunks_pattern_rules");

    if (lua_err)
    {
        return lua_error (L);
    }

    lua_pushvalue (L, 1);
    lua_call (L, 1, 1);

    return 1;
}

int
luaopen_sort_chunks (lua_State *L)
{
    int lua_err = luaL_loadbuffer (L,
            EMBEDDED_SORT_CHUNKS_LUA_DATA,
            EMBEDDED_SORT_CHUNKS_LUA_SIZE,
            "=sort_chunks");

    if (lua_err)
    {
        return lua_error (L);
    }

    lua_pushvalue (L, 1);
    lua_call (L, 1, 1);

    return 1;
}

// vi:ts=4:sw=4:et
