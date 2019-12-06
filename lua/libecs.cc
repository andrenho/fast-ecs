#include <lua.hpp>

#include <cstring>

#include <map>
#include <string>
#include <vector>

extern "C" {
    int luaopen_ecs(lua_State* L);
}


// {{{ library setup

static const struct luaL_Reg ecslib[] = {
    { "new", libecs_new },
    { NULL, NULL },
};

int luaopen_ecs(lua_State* L)
{
    luaL_newlib(L, ecslib);
    return 1;
}
