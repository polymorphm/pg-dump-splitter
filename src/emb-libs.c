// vim: set et ts=4 sw=4:

#include <lua.h>
#include <lauxlib.h>

#include "pg_dump_splitter.lua.h"
#include "emb-libs.h"

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
