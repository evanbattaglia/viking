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



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include <glib/gi18n.h>

#include "viking.h"
#include "icons/icons.h"

static void imagebox_layer_marshall( VikImageboxLayer *vil, guint8 **data, gint *len );
static VikImageboxLayer *imagebox_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp );
static gboolean imagebox_layer_set_param ( VikImageboxLayer *vil, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation );
static VikLayerParamData imagebox_layer_get_param ( VikImageboxLayer *vil, guint16 id, gboolean is_file_operation );
static void imagebox_layer_update_gc ( VikImageboxLayer *vil, VikViewport *vp );
static void imagebox_layer_post_read ( VikLayer *vl, VikViewport *vp, gboolean from_file );

static VikLayerParamScale param_scales[] = {
  { -90, 90, 0.01, 6 },
  { -180, 180, 0.01, 6 },
  { 1, 4096, 1, 0 },
  { 1, 8192, 1, 0 },
};

static VikLayerParam imagebox_layer_params[] = {
  { "center_lat", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Center Latitude:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 0 },
  { "center_lon", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Center Longitude:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 1 },
  { "zoom_factor", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Zoom Factor"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 2 },
  { "pixels_width", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Image Width (pixels)"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 3 },
  { "pixels_height", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Image Height (pixels)"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 3 },
};

enum { PARAM_CENTER_LAT = 0, PARAM_CENTER_LON, PARAM_ZOOM_FACTOR, PARAM_PIXELS_WIDTH, PARAM_PIXELS_HEIGHT, NUM_PARAMS };

VikLayerInterface vik_imagebox_layer_interface = {
  "Image box",
  &vikcoordlayer_pixbuf,

  NULL,
  0,

  imagebox_layer_params,
  NUM_PARAMS,
  NULL,
  0,

  VIK_MENU_ITEM_ALL,

  (VikLayerFuncCreate)                  vik_imagebox_layer_create,
  (VikLayerFuncRealize)                 NULL,
  (VikLayerFuncPostRead)                imagebox_layer_post_read,
  (VikLayerFuncFree)                    vik_imagebox_layer_free,

  (VikLayerFuncProperties)              NULL,
  (VikLayerFuncDraw)                    vik_imagebox_layer_draw,
  (VikLayerFuncChangeCoordMode)         NULL,

  (VikLayerFuncSetMenuItemsSelection)   NULL,
  (VikLayerFuncGetMenuItemsSelection)   NULL,

  (VikLayerFuncAddMenuItems)            NULL,
  (VikLayerFuncSublayerAddMenuItems)    NULL,

  (VikLayerFuncSublayerRenameRequest)   NULL,
  (VikLayerFuncSublayerToggleVisible)   NULL,
  (VikLayerFuncSublayerTooltip)         NULL,
  (VikLayerFuncLayerTooltip)            NULL,
  (VikLayerFuncLayerSelected)           NULL,

  (VikLayerFuncMarshall)		imagebox_layer_marshall,
  (VikLayerFuncUnmarshall)		imagebox_layer_unmarshall,

  (VikLayerFuncSetParam)                imagebox_layer_set_param,
  (VikLayerFuncGetParam)                imagebox_layer_get_param,

  (VikLayerFuncReadFileData)            NULL,
  (VikLayerFuncWriteFileData)           NULL,

  (VikLayerFuncDeleteItem)              NULL,
  (VikLayerFuncCutItem)                 NULL,
  (VikLayerFuncCopyItem)                NULL,
  (VikLayerFuncPasteItem)               NULL,
  (VikLayerFuncFreeCopiedItem)          NULL,
  (VikLayerFuncDragDropRequest)		NULL,

  (VikLayerFuncSelectClick)             NULL,
  (VikLayerFuncSelectMove)              NULL,
  (VikLayerFuncSelectRelease)           NULL,
  (VikLayerFuncSelectedViewportMenu)    NULL,
};

struct _VikImageboxLayer {
  VikLayer vl;
  GdkGC *gc;
  struct LatLon center;
  guint16 zoom_factor, pixels_width, pixels_height;
};

GType vik_imagebox_layer_get_type ()
{
  static GType vil_type = 0;

  if (!vil_type)
  {
    static const GTypeInfo vil_info =
    {
      sizeof (VikImageboxLayerClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikImageboxLayer),
      0,
      NULL /* instance init */
    };
    vil_type = g_type_register_static ( VIK_LAYER_TYPE, "VikImageboxLayer", &vil_info, 0 );
  }

  return vil_type;
}

static void imagebox_layer_marshall( VikImageboxLayer *vil, guint8 **data, gint *len )
{
  vik_layer_marshall_params ( VIK_LAYER(vil), data, len );
}

static VikImageboxLayer *imagebox_layer_unmarshall( guint8 *data, gint len, VikViewport *vvp )
{
  VikImageboxLayer *rv = vik_imagebox_layer_new ();
  vik_layer_unmarshall_params ( VIK_LAYER(rv), data, len, vvp );
  return rv;
}

gboolean imagebox_layer_set_param ( VikImageboxLayer *vil, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation )
{
  switch ( id )
  {
    case PARAM_CENTER_LAT: vil->center.lat = data.d; break;
    case PARAM_CENTER_LON: vil->center.lon = data.d; break;
    case PARAM_ZOOM_FACTOR: vil->zoom_factor = data.u; break;
    case PARAM_PIXELS_WIDTH: vil->pixels_width = data.u; break;
    case PARAM_PIXELS_HEIGHT: vil->pixels_height = data.u; break;
  }
  return TRUE;
}

static VikLayerParamData imagebox_layer_get_param ( VikImageboxLayer *vil, guint16 id, gboolean is_file_operation )
{
  VikLayerParamData rv;
  switch ( id )
  {
    case PARAM_CENTER_LAT: rv.d = vil->center.lat; break;
    case PARAM_CENTER_LON: rv.d = vil->center.lon; break;
    case PARAM_ZOOM_FACTOR: rv.u = vil->zoom_factor; break;
    case PARAM_PIXELS_WIDTH: rv.u = vil->pixels_width; break;
    case PARAM_PIXELS_HEIGHT: rv.u = vil->pixels_height; break;
  }
  return rv;
}

static void imagebox_layer_post_read ( VikLayer *vl, VikViewport *vp, gboolean from_file )
{
  VikImageboxLayer *vil = VIK_IMAGEBOX_LAYER(vl);
  if ( vil->gc )
    g_object_unref ( G_OBJECT(vil->gc) );
  vil->gc = vik_viewport_new_gc ( vp, "black", 3 );
}

VikImageboxLayer *vik_imagebox_layer_new ( )
{
  VikImageboxLayer *vil = VIK_IMAGEBOX_LAYER ( g_object_new ( VIK_IMAGEBOX_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(vil), VIK_LAYER_IMAGEBOX );

  vil->gc = NULL;
  vil->center.lat = vil->center.lon = 0.0;
  vil->zoom_factor = 4;
  vil->pixels_width = 1600;
  vil->pixels_height = 900;
  return vil;
}

#define VIK_IMAGEBOX_CLIP_ALLOWANCE 9

void vik_imagebox_layer_draw ( VikImageboxLayer *vil, gpointer data )
{
  if ( !vil->gc )
    return;

  VikViewport *vp = (VikViewport *) data;

  gdouble vp_zoom = vik_viewport_get_zoom(vp);
  if (! vp_zoom)
    return; // TODO: support for uneven zoom factors.
  gdouble actual_width = vil->zoom_factor / vp_zoom * vil->pixels_width;
  gdouble actual_height = vil->zoom_factor / vp_zoom * vil->pixels_height;

  // Find center
  gint x, y;
  VikCoord center_coord;
  vik_coord_load_from_latlon(&center_coord, vik_viewport_get_coord_mode(vp), &(vil->center));
  vik_viewport_coord_to_screen(vp, &center_coord, &x, &y);
 
  // Find corners
  gint x1 = x - actual_width, x2 = x + actual_width, y1 = y - actual_height, y2 = y + actual_height;

  // it DOESNT NEED TO BE DRAWN if (make the viewport box a litte bit bigger to allow for line thickness, etc.)
  gint vp_box_x1 = -VIK_IMAGEBOX_CLIP_ALLOWANCE;
  gint vp_box_y1 = -VIK_IMAGEBOX_CLIP_ALLOWANCE;
  gint vp_box_x2 = vik_viewport_get_width(vp) + VIK_IMAGEBOX_CLIP_ALLOWANCE;
  gint vp_box_y2 = vik_viewport_get_height(vp) + VIK_IMAGEBOX_CLIP_ALLOWANCE;

  // it's completely north of screen, west of screen, east of screen, south of screen.
  if ( x2 < vp_box_x1 || y2 < vp_box_y1 || x1 > vp_box_x2 || y1 > vp_box_y2 )
    return;
  // OR the screen is completely inside it.
  if ( x1 < vp_box_x1 && y1 < vp_box_y1 && x2 > vp_box_x2 && y2 > vp_box_y2 )
    return;

  // otherwise one of the lines goes thru it. clip coordinates (VikViewport has internal clipping of 9px) and draw it
  x1 = MAX(x1, vp_box_x1);
  y1 = MAX(y1, vp_box_y1);
  x2 = MIN(x2, vp_box_x2);
  y2 = MIN(y2, vp_box_y2);
  vik_viewport_draw_rectangle(vp, vil->gc, FALSE, x1, y1, x2-x1, y2-y1);
}

void vik_imagebox_layer_free ( VikImageboxLayer *vil )
{
  if ( vil->gc != NULL )
    g_object_unref ( G_OBJECT(vil->gc) );
}

/* TODO: probably get rid of this function */
static void imagebox_layer_update_gc ( VikImageboxLayer *vil, VikViewport *vp )
{
  if ( vil->gc )
    g_object_unref ( G_OBJECT(vil->gc) );

  vil->gc = vik_viewport_new_gc ( vp, "black", 3 ); /* TODO: DRY "black", 3 if this function is needed */
}

VikImageboxLayer *vik_imagebox_layer_create ( VikViewport *vp )
{
  VikImageboxLayer *vil = vik_imagebox_layer_new ();
  imagebox_layer_update_gc ( vil, vp );
  return vil;
}

