#define LUA_COMPAT_MODULE
#include <lua.h>                                /* Always include this when calling Lua */
#include <lauxlib.h>                            /* Always include this when calling Lua */
#include <lualib.h>                             /* Always include this when calling Lua */
#include "vikwaypoint.h"
#include "viktrwlayer.h"
#include "globals.h"

#include <stdlib.h>

#define VIKLUA_WAYPOINT_METATABLE "Viking.waypoint"
#define VIKLUA_TRWLAYER_METATABLE "Viking.trwlayer"
#define VIKLUA_GLIB_HASHITER_METADATA "Viking.glib.hashiter"

#define VIKLUA_GLIB_LIST_METADATA "Viking.glib.list"
#define VIKLUA_TRACK_METATABLE "Viking.track"
#define VIKLUA_TRACKPOINT_METATABLE "Viking.trackpoint"
#define VIKLUA_COORD_METATABLE "Viking.coord"


static const gchar *lua_file;
static const gchar **lua_args;

/****** VIKCOORD ****/
static void push_viklua_coord(lua_State *L, VikCoord *c)
{
  VikCoord **pc = (VikCoord **) lua_newuserdata(L, sizeof(VikCoord *));
  *pc = c;
  luaL_getmetatable(L, VIKLUA_COORD_METATABLE);
  lua_setmetatable(L, -2);
}

static int viklua_coord_diff(lua_State *L)
{
  VikCoord **c1 = luaL_checkudata(L, 1, VIKLUA_COORD_METATABLE);
  luaL_argcheck(L, c1 != NULL, 1, "`coord' expected");
  VikCoord **c2 = luaL_checkudata(L, 2, VIKLUA_COORD_METATABLE);
  luaL_argcheck(L, c2 != NULL, 2, "`coord' expected");

  lua_pushnumber(L, vik_coord_diff(*c1, *c2));
  return 1;
}
static int viklua_coord_set_lat(lua_State *L)
{
  VikCoord **c = luaL_checkudata(L, 1, VIKLUA_COORD_METATABLE);
  double lat = luaL_checknumber(L, 2);
  luaL_argcheck(L, c != NULL, 1, "`coord' expected");
  struct LatLon ll;
  vik_coord_to_latlon(*c, &ll);
  ll.lat = lat;
  vik_coord_load_from_latlon(*c, (*c)->mode, &ll);
  return 0;
}
static int viklua_coord_set_lon(lua_State *L)
{
  VikCoord **c = luaL_checkudata(L, 1, VIKLUA_COORD_METATABLE);
  double lon = luaL_checknumber(L, 2);
  luaL_argcheck(L, c != NULL, 1, "`coord' expected");
  struct LatLon ll;
  vik_coord_to_latlon(*c, &ll);
  ll.lon = lon;
  vik_coord_load_from_latlon(*c, (*c)->mode, &ll);
  return 0;
}
static int viklua_coord_lat(lua_State *L)
{
  VikCoord **c = luaL_checkudata(L, 1, VIKLUA_COORD_METATABLE);
  luaL_argcheck(L, c != NULL, 1, "`coord' expected");
  struct LatLon ll;
  vik_coord_to_latlon(*c, &ll);
  lua_pushnumber(L, ll.lat);
  return 1;
}
static int viklua_coord_lon(lua_State *L)
{
  VikCoord **c = luaL_checkudata(L, 1, VIKLUA_COORD_METATABLE);
  luaL_argcheck(L, c != NULL, 1, "`coord' expected");
  struct LatLon ll;
  vik_coord_to_latlon(*c, &ll);
  lua_pushnumber(L, ll.lon);
  return 1;
}
static const struct luaL_Reg viklua_coord_f[] = {   // static functions
  { "diff", viklua_coord_diff },
  { NULL, NULL },
};
static const struct luaL_Reg viklua_coord_m[] = {   // object methods
  { "lat", viklua_coord_lat },
  { "lon", viklua_coord_lon },
  { "set_lat", viklua_coord_set_lat },
  { "set_lon", viklua_coord_set_lon },
  { NULL, NULL },
};

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

static int viklua_waypoint_coord(lua_State *L)
{
  VikWaypoint **lwp = luaL_checkudata(L, 1, VIKLUA_WAYPOINT_METATABLE);
  luaL_argcheck(L, lwp != NULL, 1, "`waypoint' expected");
  push_viklua_coord(L, &((*lwp)->coord));
  return 1;
}

static const struct luaL_Reg viklua_waypoint_f[] = {   // static functions
  { NULL, NULL },
};
static const struct luaL_Reg viklua_waypoint_m[] = {   // object methods
  { "comment", viklua_waypoint_comment },
  { "coord", viklua_waypoint_coord },
  { NULL, NULL },
};


static void push_viklua_trackpoint(lua_State *L, VikTrackpoint *tp)
{
  VikTrackpoint **ltp = (VikTrackpoint **) lua_newuserdata(L, sizeof(VikTrackpoint *));
  *ltp = tp;
  luaL_getmetatable(L, VIKLUA_TRACKPOINT_METATABLE);
  lua_setmetatable(L, -2);
}
static int viklua_trackpoint_coord(lua_State *L)
{
  VikTrackpoint **ltp = luaL_checkudata(L, 1, VIKLUA_TRACKPOINT_METATABLE);
  luaL_argcheck(L, ltp != NULL, 1, "`trackpoint' expected");
  push_viklua_coord(L, &((*ltp)->coord));
  return 1;
}
static int viklua_trackpoint_timestamp(lua_State *L)
{
  VikTrackpoint **ltp = luaL_checkudata(L, 1, VIKLUA_TRACKPOINT_METATABLE);
  luaL_argcheck(L, ltp != NULL, 1, "`trackpoint' expected");
  if ((*ltp)->has_timestamp)
    lua_pushnumber(L, (*ltp)->timestamp);
  else
    lua_pushnil(L);
  return 1;
}
static int viklua_trackpoint_altitude(lua_State *L)
{
  VikTrackpoint **ltp = luaL_checkudata(L, 1, VIKLUA_TRACKPOINT_METATABLE);
  luaL_argcheck(L, ltp != NULL, 1, "`trackpoint' expected");
  if ((*ltp)->altitude == VIK_DEFAULT_ALTITUDE)
    lua_pushnil(L);
  else
    lua_pushnumber(L, (*ltp)->altitude);
  return 1;
}
static const struct luaL_Reg viklua_trackpoint_f[] = {   // static functions
  { NULL, NULL },
};
static const struct luaL_Reg viklua_trackpoint_m[] = {   // object methods
  { "coord", viklua_trackpoint_coord },
  { "timestamp", viklua_trackpoint_timestamp },
  { "altitude", viklua_trackpoint_altitude },
  { NULL, NULL },
};


static void push_viklua_track(lua_State *L, VikTrack *track)
{
  VikTrack **lt = (VikTrack **) lua_newuserdata(L, sizeof(VikTrack *));
  *lt = track;
  luaL_getmetatable(L, VIKLUA_TRACK_METATABLE);
  lua_setmetatable(L, -2);
}
static int viklua_track_trackpoints_iter(lua_State *L)
{
  GList **iter = (GList **) lua_touserdata(L, lua_upvalueindex(1));
  if (*iter) {
    push_viklua_trackpoint(L, (VikTrackpoint *) (*iter)->data);
    *iter = (*iter)->next;
  } else
    lua_pushnil(L);
  return 1;
}
static int viklua_track_trackpoints(lua_State *L)
{
  VikTrack **pvt = luaL_checkudata(L, 1, VIKLUA_TRACK_METATABLE);
  luaL_argcheck(L, pvt != NULL, 1, "`track' expected");

  GList **iter = (GList **) lua_newuserdata(L, sizeof(GList *));
  luaL_getmetatable(L, VIKLUA_GLIB_LIST_METADATA);
  lua_setmetatable(L, -2);
  *iter = (*pvt)->trackpoints;

  lua_pushcclosure(L, viklua_track_trackpoints_iter, 1);
  return 1;
}
static const struct luaL_Reg viklua_track_f[] = {   // static functions
  { NULL, NULL },
};
static const struct luaL_Reg viklua_track_m[] = {   // object methods
  { "trackpoints", viklua_track_trackpoints},
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

// TODO: DRY this up...
static int viklua_trwlayer_tracks_iter(lua_State *L)
{
  GHashTableIter *i = (GHashTableIter *)lua_touserdata(L, lua_upvalueindex(1));
  gpointer key;
  gpointer wp;
  if (g_hash_table_iter_next(i, &key, &wp)) {
    lua_pushstring(L, (const char *)key);
    push_viklua_track(L, (VikTrack *)wp);
    return 2;
  } else {
    lua_pushnil(L);
    return 1;
  }
}
static int viklua_trwlayer_tracks(lua_State *L)
{
  VikTrwLayer **pvtl = luaL_checkudata(L, 1, VIKLUA_TRWLAYER_METATABLE);
  luaL_argcheck(L, pvtl != NULL, 1, "`trwlayer' expected");

  GHashTableIter *iter = (GHashTableIter *) lua_newuserdata(L, sizeof(GHashTableIter));
  luaL_getmetatable(L, VIKLUA_GLIB_HASHITER_METADATA);
  lua_setmetatable(L, -2);

  g_hash_table_iter_init(iter, vik_trw_layer_get_tracks(*pvtl));

  lua_pushcclosure(L, viklua_trwlayer_tracks_iter, 1);
  return 1;
}

static const struct luaL_Reg viklua_trwlayer_f[] = {   // static functions
  { NULL, NULL },
};
static const struct luaL_Reg viklua_trwlayer_m[] = {   // object methods
  { "waypoints", viklua_trwlayer_waypoints },
  { "tracks", viklua_trwlayer_tracks },
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
  luaL_newmetatable(L, VIKLUA_GLIB_LIST_METADATA);


  luaL_newmetatable(L, VIKLUA_TRACK_METATABLE);
  lua_pushstring(L, "__index");
  lua_pushvalue(L, -2);  /* pushes the metatable */
  lua_settable(L, -3);  /* metatable.__index = metatable */
  luaL_openlib(L, NULL, viklua_track_m, 0);
  luaL_openlib(L, "track", viklua_track_f, 0);

  luaL_newmetatable(L, VIKLUA_TRACKPOINT_METATABLE);
  lua_pushstring(L, "__index");
  lua_pushvalue(L, -2);  /* pushes the metatable */
  lua_settable(L, -3);  /* metatable.__index = metatable */
  luaL_openlib(L, NULL, viklua_trackpoint_m, 0);
  luaL_openlib(L, "trackpoint", viklua_trackpoint_f, 0);

  luaL_newmetatable(L, VIKLUA_COORD_METATABLE);
  lua_pushstring(L, "__index");
  lua_pushvalue(L, -2);  /* pushes the metatable */
  lua_settable(L, -3);  /* metatable.__index = metatable */
  luaL_openlib(L, NULL, viklua_coord_m, 0);
  luaL_openlib(L, "coord", viklua_coord_f, 0);

  return 1;
}


static void bail(lua_State *L, char *msg){
	fprintf(stderr, "\nFATAL ERROR:\n  %s: %s\n\n",
		msg, lua_tostring(L, -1));
	exit(1);
}

void a_lua_set_file(const gchar *file, const gchar **args)
{
  lua_file = file;
  lua_args = args;
}

// TODO: only have one L accoss many instances of calling
void a_lua(VikTrwLayer *vtl)
{
  if (!lua_file)
    return;

  lua_State *L = luaL_newstate();                        /* Create Lua state variable */
  luaL_openlibs(L);                                      /* Load Lua libraries */
  lua_setup(L);


  if (luaL_loadfile(L, lua_file))               /* Load but don't run the Lua script */
    bail(L, "luaL_loadfile() failed");                 /* Error out if file can't be read */

  if (lua_pcall(L, 0, 0, 0)) bail(L, "lua_pcall() failed"); // priming run to load function names i guess

  lua_getglobal(L, "run");
  push_viklua_trwlayer(L, vtl); // push a waypoint

  gint n_extra_args = 0;
  const gchar **args;
  for (args = lua_args; args && *args; args++, n_extra_args++)
    lua_pushstring(L, *args);

  printf("In C, calling Lua\n");

  if (lua_pcall(L, 1+n_extra_args, 1, 0))         /* Run the loaded Lua script */
    bail(L, "lua_pcall() failed");          /* Error out if Lua file has an error */

  printf("Back in C again\n");

  lua_close(L);                               /* Clean up, free the Lua state var */
}


