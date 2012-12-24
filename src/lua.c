#define LUA_COMPAT_MODULE
#include <lua.h>                                /* Always include this when calling Lua */
#include <lauxlib.h>                            /* Always include this when calling Lua */
#include <lualib.h>                             /* Always include this when calling Lua */
#include "vikwaypoint.h"
#include "viktrwlayer.h"

#include <stdlib.h>

#define VIKLUA_WAYPOINT_METATABLE "Viking.waypoint"
#define VIKLUA_TRWLAYER_METATABLE "Viking.trwlayer"
#define VIKLUA_GLIB_HASHITER_METADATA "Viking.glib.hashiter"

/*** WAYPOINT ***/
static void push_viklua_waypoint(lua_State *L, VikWaypoint *wp)
{
  VikWaypoint **lwp = (VikWaypoint **) lua_newuserdata(L, sizeof(VikWaypoint *));
  *lwp = wp;
  luaL_getmetatable(L, VIKLUA_WAYPOINT_METATABLE);
  lua_setmetatable(L, -2);
}

static int viklua_waypoint_comment(lua_State *L)
{
  VikWaypoint **lwp = luaL_checkudata(L, 1, VIKLUA_WAYPOINT_METATABLE);
  luaL_argcheck(L, lwp != NULL, 1, "`waypoint' expected");
  if ((*lwp)->comment)
    lua_pushfstring(L, (*lwp)->comment);
  else
    lua_pushnil(L);
  return 1;
}

static const struct luaL_Reg viklua_waypoint_f[] = {   // static functions
  { NULL, NULL },
};
static const struct luaL_Reg viklua_waypoint_m[] = {   // object methods
  { "comment", viklua_waypoint_comment },
  { NULL, NULL },
};


/*** VIKTRWLAYER ***/
static void push_viklua_trwlayer(lua_State *L, VikTrwLayer *vtl)
{
  VikTrwLayer **ret = (VikTrwLayer **) lua_newuserdata(L, sizeof(VikWaypoint *));
  *ret = vtl;
  luaL_getmetatable(L, VIKLUA_TRWLAYER_METATABLE);
  lua_setmetatable(L, -2);
}

static int viklua_trwlayer_waypoints_iter(lua_State *L)
{
  GHashTableIter *i = (GHashTableIter *)lua_touserdata(L, lua_upvalueindex(1));
  gpointer key;
  gpointer wp;
  if (g_hash_table_iter_next(i, &key, &wp)) {
    lua_pushstring(L, (const char *)key);
    push_viklua_waypoint(L, (VikWaypoint *)wp);
    return 2;
  } else {
    lua_pushnil(L);
    return 1;
  }
}

static int viklua_trwlayer_waypoints(lua_State *L)
{
  VikTrwLayer **pvtl = luaL_checkudata(L, 1, VIKLUA_TRWLAYER_METATABLE);
  luaL_argcheck(L, pvtl != NULL, 1, "`trwlayer' expected");

  GHashTableIter *iter = (GHashTableIter *) lua_newuserdata(L, sizeof(GHashTableIter));
  luaL_getmetatable(L, VIKLUA_GLIB_HASHITER_METADATA);
  lua_setmetatable(L, -2);

  g_hash_table_iter_init(iter, vik_trw_layer_get_waypoints(*pvtl));

  lua_pushcclosure(L, viklua_trwlayer_waypoints_iter, 1);
  return 1;
}


static const struct luaL_Reg viklua_trwlayer_f[] = {   // static functions
  { NULL, NULL },
};
static const struct luaL_Reg viklua_trwlayer_m[] = {   // object methods
  { "waypoints", viklua_trwlayer_waypoints },
  { NULL, NULL },
};

/***********************************************************************/

static int lua_setup (lua_State *L) {
  luaL_newmetatable(L, VIKLUA_WAYPOINT_METATABLE);

  lua_pushstring(L, "__index");
  lua_pushvalue(L, -2);  /* pushes the metatable */
  lua_settable(L, -3);  /* metatable.__index = metatable */

  luaL_openlib(L, NULL, viklua_waypoint_m, 0);

  luaL_openlib(L, "waypoint", viklua_waypoint_f, 0);


  luaL_newmetatable(L, VIKLUA_TRWLAYER_METATABLE);

  lua_pushstring(L, "__index");
  lua_pushvalue(L, -2);  /* pushes the metatable */
  lua_settable(L, -3);  /* metatable.__index = metatable */

  luaL_openlib(L, NULL, viklua_trwlayer_m, 0);

  luaL_openlib(L, "trwlayer", viklua_trwlayer_f, 0);


  luaL_newmetatable(L, VIKLUA_GLIB_HASHITER_METADATA);
  return 1;
}


static void bail(lua_State *L, char *msg){
	fprintf(stderr, "\nFATAL ERROR:\n  %s: %s\n\n",
		msg, lua_tostring(L, -1));
	exit(1);
}


// TODO: only have one L accoss many instances of calling
void a_lua(VikTrwLayer *vtl)
{
    lua_State *L = luaL_newstate();                        /* Create Lua state variable */
    luaL_openlibs(L);                                      /* Load Lua libraries */
    lua_setup(L);


    if (luaL_loadfile(L, "/home/evan/t/dev/lua/newvik.lua"))               /* Load but don't run the Lua script */
        bail(L, "luaL_loadfile() failed");                 /* Error out if file can't be read */

    if (lua_pcall(L, 0, 0, 0)) bail(L, "lua_pcall() failed"); // priming run to load function names i guess

    lua_getglobal(L, "run");
    push_viklua_trwlayer(L, vtl); // push a waypoint

    printf("In C, calling Lua\n");

    if (lua_pcall(L, 1, 1, 0))                  /* Run the loaded Lua script */
        bail(L, "lua_pcall() failed");          /* Error out if Lua file has an error */

    printf("Back in C again\n");

    lua_close(L);                               /* Clean up, free the Lua state var */
}


