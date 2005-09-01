/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2005, Evan Battaglia <gtoevan@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "viking.h"
#include "vikcoordlayer_pixmap.h"

static VikCoordLayer *coord_layer_copy ( VikCoordLayer *vcl, gpointer vp );
static gboolean coord_layer_set_param ( VikCoordLayer *vcl, guint16 id, VikLayerParamData data, VikViewport *vp );
static VikLayerParamData coord_layer_get_param ( VikCoordLayer *vcl, guint16 id );
static void coord_layer_update_gc ( VikCoordLayer *vcl, VikViewport *vp, const gchar *color );
static void coord_layer_post_read ( VikCoordLayer *vcl, VikViewport *vp );

VikLayerParamScale param_scales[] = {
  { 0.05, 60.0, 0.25, 10 },
  { 1, 10, 1, 0 },
};

VikLayerParam coord_layer_params[] = {
  { "color", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, "Color:", VIK_LAYER_WIDGET_ENTRY },
  { "min_inc", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, "Minutes Width:", VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 0 },
  { "line_thickness", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, "Line Thickness:", VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 1 },
};


enum { PARAM_COLOR = 0, PARAM_MIN_INC, PARAM_LINE_THICKNESS, NUM_PARAMS };

VikLayerInterface vik_coord_layer_interface = {
  "Coord",
  &coordlayer_pixbuf,

  NULL,
  0,

  coord_layer_params,
  NUM_PARAMS,
  NULL,
  0,

  (VikLayerFuncCreate)                  vik_coord_layer_create,
  (VikLayerFuncRealize)                 NULL,
  (VikLayerFuncPostRead)                coord_layer_post_read,
  (VikLayerFuncFree)                    vik_coord_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    vik_coord_layer_draw,
  (VikLayerFuncChangeCoordMode)         NULL,

  (VikLayerFuncAddMenuItems)            NULL,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,

  (VikLayerFuncCopy)                    coord_layer_copy,

  (VikLayerFuncSetParam)                coord_layer_set_param,
  (VikLayerFuncGetParam)                coord_layer_get_param,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
};

struct _VikCoordLayer {
  VikLayer vl;
  GdkGC *gc;
  gdouble deg_inc;
  guint8 line_thickness;
  gchar *color;
};

GType vik_coord_layer_get_type ()
{
  static GType vcl_type = 0;

  if (!vcl_type)
  {
    static const GTypeInfo vcl_info =
    {
      sizeof (VikCoordLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikCoordLayer),
      0,
      NULL /* instance init */
    };
    vcl_type = g_type_register_static ( VIK_LAYER_TYPE, "VikCoordLayer", &vcl_info, 0 );
  }

  return vcl_type;
}

static VikCoordLayer *coord_layer_copy ( VikCoordLayer *vcl, gpointer vp )
{
  VikCoordLayer *rv = vik_coord_layer_new ( );

  rv->color = g_strdup ( vcl->color );
  rv->deg_inc = vcl->deg_inc;
  rv->line_thickness = vcl->line_thickness;
  rv->gc = vcl->gc;
  g_object_ref ( rv->gc );
  return rv;
}

gboolean coord_layer_set_param ( VikCoordLayer *vcl, guint16 id, VikLayerParamData data, VikViewport *vp )
{
  switch ( id )
  {
    case PARAM_COLOR: if ( vcl->color ) g_free ( vcl->color ); vcl->color = g_strdup ( data.s ); break;
    case PARAM_MIN_INC: vcl->deg_inc = data.d / 60.0; break;
    case PARAM_LINE_THICKNESS: if ( data.u >= 1 && data.u <= 15 ) vcl->line_thickness = data.u; break;
  }
  return TRUE;
}

static VikLayerParamData coord_layer_get_param ( VikCoordLayer *vcl, guint16 id )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_COLOR: rv.s = vcl->color ? vcl->color : ""; break;
    case PARAM_MIN_INC: rv.d = vcl->deg_inc * 60.0; break;
    case PARAM_LINE_THICKNESS: rv.i = vcl->line_thickness; break;
  }
  return rv;
}

static void coord_layer_post_read ( VikCoordLayer *vcl, VikViewport *vp )
{
  if ( vcl->gc )
    g_object_unref ( G_OBJECT(vcl->gc) );

  vcl->gc = vik_viewport_new_gc ( vp, vcl->color, vcl->line_thickness );
}

VikCoordLayer *vik_coord_layer_new ( )
{
  VikCoordLayer *vcl = VIK_COORD_LAYER ( g_object_new ( VIK_COORD_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(vcl), VIK_LAYER_COORD );

  vcl->gc = NULL;
  vcl->deg_inc = 1.0/60.0;
  vcl->line_thickness = 3;
  vcl->color = NULL;
  return vcl;
}

void vik_coord_layer_draw ( VikCoordLayer *vcl, gpointer data )
{
  VikViewport *vp = (VikViewport *) data;
  if ( vik_viewport_get_coord_mode(vp) != VIK_COORD_UTM )
    return;
  if ( vcl->gc != NULL)
  {
    const struct UTM *center = (const struct UTM *)vik_viewport_get_center ( vp );
    gdouble xmpp = vik_viewport_get_xmpp ( vp ), ympp = vik_viewport_get_ympp ( vp );
    guint16 width = vik_viewport_get_width ( vp ), height = vik_viewport_get_height ( vp );
    struct LatLon ll, ll2, min, max;
    double lon;
    int x1, x2;
    struct UTM utm;

    utm = *center;
    utm.northing = center->northing - ( ympp * height / 2 );

    a_coords_utm_to_latlon ( &utm, &ll );

    utm.northing = center->northing + ( ympp * height / 2 );

    a_coords_utm_to_latlon ( &utm, &ll2 );

    {
      /* find corner coords in lat/lon.
        start at whichever is less: top or bottom left lon. goto whichever more: top or bottom right lon
      */
      struct LatLon topleft, topright, bottomleft, bottomright;
      struct UTM temp_utm;
      temp_utm = *center;
      temp_utm.easting -= (width/2)*xmpp;
      temp_utm.northing += (height/2)*ympp;
      a_coords_utm_to_latlon ( &temp_utm, &topleft );
      temp_utm.easting += (width*xmpp);
      a_coords_utm_to_latlon ( &temp_utm, &topright );
      temp_utm.northing -= (height*ympp);
      a_coords_utm_to_latlon ( &temp_utm, &bottomright );
      temp_utm.easting -= (width*xmpp);
      a_coords_utm_to_latlon ( &temp_utm, &bottomleft );
      min.lon = (topleft.lon < bottomleft.lon) ? topleft.lon : bottomleft.lon;
      max.lon = (topright.lon > bottomright.lon) ? topright.lon : bottomright.lon;
      min.lat = (bottomleft.lat < bottomright.lat) ? bottomleft.lat : bottomright.lat;
      max.lat = (topleft.lat > topright.lat) ? topleft.lat : topright.lat;
    }

    lon = ((double) ((long) ((min.lon)/ vcl->deg_inc))) * vcl->deg_inc;
    ll.lon = ll2.lon = lon;

    for (; ll.lon <= max.lon; ll.lon+=vcl->deg_inc, ll2.lon+=vcl->deg_inc )
    {
      a_coords_latlon_to_utm ( &ll, &utm );
      x1 = ( (utm.easting - center->easting) / xmpp ) + (width / 2);
      a_coords_latlon_to_utm ( &ll2, &utm );
      x2 = ( (utm.easting - center->easting) / xmpp ) + (width / 2);
      vik_viewport_draw_line (vp, vcl->gc, x1, height, x2, 0);
    }

    utm = *center;
    utm.easting = center->easting - ( xmpp * width / 2 );

    a_coords_utm_to_latlon ( &utm, &ll );

    utm.easting = center->easting + ( xmpp * width / 2 );

    a_coords_utm_to_latlon ( &utm, &ll2 );

    /* really lat, just reusing a variable */
    lon = ((double) ((long) ((min.lat)/ vcl->deg_inc))) * vcl->deg_inc;
    ll.lat = ll2.lat = lon;

    for (; ll.lat <= max.lat ; ll.lat+=vcl->deg_inc, ll2.lat+=vcl->deg_inc )
    {
      a_coords_latlon_to_utm ( &ll, &utm );
      x1 = (height / 2) - ( (utm.northing - center->northing) / ympp );
      a_coords_latlon_to_utm ( &ll2, &utm );
      x2 = (height / 2) - ( (utm.northing - center->northing) / ympp );
      vik_viewport_draw_line (vp, vcl->gc, width, x2, 0, x1);
    }
  }
}

void vik_coord_layer_free ( VikCoordLayer *vcl )
{
  if ( vcl->gc != NULL )
    g_object_unref ( G_OBJECT(vcl->gc) );

  if ( vcl->color != NULL )
    g_free ( vcl->color );
}

static void coord_layer_update_gc ( VikCoordLayer *vcl, VikViewport *vp, const gchar *color )
{
  if ( vcl->color )
    g_free ( vcl->color );

  vcl->color = g_strdup ( color );

  if ( vcl->gc )
    g_object_unref ( G_OBJECT(vcl->gc) );

  vcl->gc = vik_viewport_new_gc ( vp, vcl->color, vcl->line_thickness );
}

VikCoordLayer *vik_coord_layer_create ( VikViewport *vp )
{
  VikCoordLayer *vcl = vik_coord_layer_new ();
  coord_layer_update_gc ( vcl, vp, "red" );
  return vcl;
}
