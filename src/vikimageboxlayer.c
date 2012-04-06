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
static void imagebox_layer_add_menu_items ( VikImageboxLayer *vil, GtkMenu *menu, gpointer vlp );

static VikLayerParamScale param_scales[] = {
  { -90, 90, 0.01, 6 },
  { -180, 180, 0.01, 6 },
  { 1, 4096, 1, 0 },
  { 1, 8192, 1, 0 },
};

static VikLayerParam imagebox_layer_params[] = {
  { "filename", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("File to save to:"), VIK_LAYER_WIDGET_FILESAVEENTRY },
  { "center_lat", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Center Latitude:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 0 },
  { "center_lon", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Center Longitude:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 1 },
  { "zoom_factor", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Zoom Factor"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 2 },
  { "pixels_width", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Image Width (pixels)"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 3 },
  { "pixels_height", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Image Height (pixels)"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 3 },
};

enum { PARAM_FILENAME = 0, PARAM_CENTER_LAT, PARAM_CENTER_LON, PARAM_ZOOM_FACTOR, PARAM_PIXELS_WIDTH, PARAM_PIXELS_HEIGHT, NUM_PARAMS };


/* tools */
static gpointer imagebox_layer_move_create ( VikWindow *vw, VikViewport *vvp);
static gboolean imagebox_layer_move_press ( VikImageboxLayer *vil, GdkEventButton *event, VikViewport *vvp );

static VikToolInterface imagebox_tools[] = {
  { N_("Recenter Imagebox"), (VikToolConstructorFunc) imagebox_layer_move_create, NULL, NULL, NULL,
    (VikToolMouseFunc) imagebox_layer_move_press, NULL, NULL,
    (VikToolKeyFunc) NULL, GDK_CURSOR_IS_PIXMAP, &cursor_geomove_pixbuf }, /* TODO: get rid of cursor_geomove_pixbuf and create an image from this. I copied this from georeflayer. */
};

VikLayerInterface vik_imagebox_layer_interface = {
  "Image box",
  &vikcoordlayer_pixbuf,

  imagebox_tools,
  sizeof(imagebox_tools) / sizeof(VikToolInterface),

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

  (VikLayerFuncAddMenuItems)            imagebox_layer_add_menu_items,
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
  gchar *filename;
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
  imagebox_layer_update_gc ( rv, vvp);
  return rv;
}

gboolean imagebox_layer_set_param ( VikImageboxLayer *vil, guint16 id, VikLayerParamData data, VikViewport *vp, gboolean is_file_operation )
{
  switch ( id )
  {
    case PARAM_FILENAME: vil->filename = g_strdup(data.s); break;
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
    case PARAM_FILENAME: rv.s = vil->filename ? vil->filename : ""; break; 
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
  imagebox_layer_update_gc(vil, vp);
}

VikImageboxLayer *vik_imagebox_layer_new ( )
{
  VikImageboxLayer *vil = VIK_IMAGEBOX_LAYER ( g_object_new ( VIK_IMAGEBOX_LAYER_TYPE, NULL ) );
  vik_layer_init ( VIK_LAYER(vil), VIK_LAYER_IMAGEBOX );

  vil->filename = NULL;
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
  gint x1 = x - (actual_width/2), x2 = x + (actual_width/2), y1 = y - (actual_height/2), y2 = y + (actual_height/2);

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
  if ( vil->filename )
    g_free ( vil->filename );
  vil->filename = NULL;
  if ( vil->gc != NULL )
    g_object_unref ( G_OBJECT(vil->gc) );
}

static void imagebox_layer_update_gc ( VikImageboxLayer *vil, VikViewport *vp )
{
  if ( vil->gc )
    g_object_unref ( G_OBJECT(vil->gc) );

  vil->gc = vik_viewport_new_gc ( vp, "black", 3 );
}

VikImageboxLayer *vik_imagebox_layer_create ( VikViewport *vp )
{
  VikImageboxLayer *vil = vik_imagebox_layer_new ();
  imagebox_layer_update_gc ( vil, vp );
  return vil;
}

static void imagebox_layer_generate_map ( gpointer vil_vlp[2] )
{
  VikImageboxLayer *vil = VIK_IMAGEBOX_LAYER ( vil_vlp[0] );
  if (! vil->filename || ! *(vil->filename)) {
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vil), "No filename to save to!");
    return;
  }

  VikLayersPanel *vlp = VIK_LAYERS_PANEL(vil_vlp[1]);
  VikViewport *vp = vik_layers_panel_get_viewport(vlp);

  // Lots copied from vikwindow.c:save_image_file. It would e nice to DRY this up.
  /* more efficient way: stuff draws directly to pixbuf (fork viewport) */
  GdkPixbuf *pixbuf_to_save;
  gdouble old_xmpp, old_ympp;
  GError *error = NULL;
  /* backup old zoom & set new */
  VikCoord orig_center= *vik_viewport_get_center(vp);
  old_xmpp = vik_viewport_get_xmpp ( vp );
  old_ympp = vik_viewport_get_ympp ( vp );
  VikCoord new_center;
  vik_coord_load_from_latlon( &new_center, vik_viewport_get_coord_mode(vp), &(vil->center));
  vik_viewport_set_center_coord(vp, &new_center);
  vik_viewport_set_zoom ( vp, vil->zoom_factor );

  /* reset width and height: */
  vik_viewport_configure_manually ( vp, vil->pixels_width, vil->pixels_height );

  /* draw all layers */
  vik_viewport_clear ( vp );
  vik_layers_panel_draw_all ( vlp );
  vik_viewport_draw_scale ( vp );

  /* save buffer as file. */
  pixbuf_to_save = gdk_pixbuf_get_from_drawable ( NULL, GDK_DRAWABLE(vik_viewport_get_pixmap ( vp )), NULL, 0, 0, 0, 0, vil->pixels_width, vil->pixels_height);

  gdk_pixbuf_save ( pixbuf_to_save, vil->filename, "png", &error, NULL );
  if (error)
  {
    gchar *msg = g_strdup_printf("Unable to write to file %s: %s", vil->filename, error->message );
    a_dialog_error_msg(VIK_GTK_WINDOW_FROM_LAYER(vil), msg);
    g_free(msg);
    g_error_free (error);
  }
  g_object_unref ( G_OBJECT(pixbuf_to_save) );

  /* pretend like nothing happened ;) */
  vik_viewport_set_xmpp ( vp, old_xmpp );
  vik_viewport_set_ympp ( vp, old_ympp );
  vik_viewport_set_center_coord(vp, &orig_center);
  vik_viewport_configure ( vp );

  // redraw everything since we've messed with the viewport.
  vik_layer_emit_update_although_invisible( VIK_LAYER(vil) );
}

static void imagebox_layer_add_menu_items ( VikImageboxLayer *vil, GtkMenu *menu, gpointer vlp )
{
  static gpointer pass_along[2];
  GtkWidget *item;
  pass_along[0] = vil;
  pass_along[1] = vlp;

  item = gtk_menu_item_new();
  gtk_menu_shell_append ( GTK_MENU_SHELL(menu), item );
  gtk_widget_show ( item );

  item = gtk_image_menu_item_new_with_mnemonic ( _("_Generate Map") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(imagebox_layer_generate_map), pass_along );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show ( item );
}


static gpointer imagebox_layer_move_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static gboolean imagebox_layer_move_press ( VikImageboxLayer *vil, GdkEventButton *event, VikViewport *vvp )
{
  if (!vil || vil->vl.type != VIK_LAYER_IMAGEBOX)
    return FALSE;

  VikCoord new_center;
  vik_viewport_screen_to_coord(vvp, event->x, event->y, &new_center);
  vik_coord_to_latlon(&new_center, &(vil->center));
  vik_layer_emit_update ( VIK_LAYER(vil), FALSE );
  return TRUE; /* I didn't move anything on this layer! */
}
