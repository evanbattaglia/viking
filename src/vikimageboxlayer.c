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

static void imagebox_layer_draw_compass_rose(VikImageboxLayer *vil, VikViewport *vp);

static VikLayerParamScale param_scales[] = {
  { -90, 90, 0.01, 6 },
  { -180, 180, 0.01, 6 },
  { 1, 4096, 1, 0 },
  { 1, 8192, 1, 0 },
  { -4096, 4096, 1, 0 },
  { -4096, 4096, 1, 0 },
};

static VikLayerParam imagebox_layer_params[] = {
  { "filename", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("File to save to:"), VIK_LAYER_WIDGET_FILESAVEENTRY },
  { "center_lat", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Center Latitude:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 0 },
  { "center_lon", VIK_LAYER_PARAM_DOUBLE, VIK_LAYER_GROUP_NONE, N_("Center Longitude:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 1 },
  { "zoom_factor", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Zoom Factor"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 2 },
  { "pixels_width", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Image Width (pixels)"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 3 },
  { "pixels_height", VIK_LAYER_PARAM_UINT, VIK_LAYER_GROUP_NONE, N_("Image Height (pixels)"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 3 },
  { "compass_rose_filename", VIK_LAYER_PARAM_STRING, VIK_LAYER_GROUP_NONE, N_("Compass rose filename (optional):"), VIK_LAYER_WIDGET_FILEENTRY },
  { "compass_rose_offset_x", VIK_LAYER_PARAM_INT, VIK_LAYER_GROUP_NONE, N_("Compass rose center X offset:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 4 },
  { "compass_rose_offset_y", VIK_LAYER_PARAM_INT, VIK_LAYER_GROUP_NONE, N_("Compass rose center Y offset:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 4 },
  { "compass_rose_coordtext_offset_x", VIK_LAYER_PARAM_INT, VIK_LAYER_GROUP_NONE, N_("C.R. Coord Text X offset:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 5 },
  { "compass_rose_coordtext_offset_y", VIK_LAYER_PARAM_INT, VIK_LAYER_GROUP_NONE, N_("C.R. Coord Text Y offset:"), VIK_LAYER_WIDGET_SPINBUTTON, param_scales + 5 },
};

enum { PARAM_FILENAME = 0, PARAM_CENTER_LAT, PARAM_CENTER_LON, PARAM_ZOOM_FACTOR, PARAM_PIXELS_WIDTH, PARAM_PIXELS_HEIGHT, PARAM_COMPASS_ROSE_FILENAME, PARAM_COMPASS_ROSE_X_OFFSET, PARAM_COMPASS_ROSE_Y_OFFSET, PARAM_COMPASS_ROSE_COORDTEXT_X_OFFSET, PARAM_COMPASS_ROSE_COORDTEXT_Y_OFFSET, NUM_PARAMS };


/* tools */
static gpointer imagebox_layer_move_create ( VikWindow *vw, VikViewport *vvp);
static gboolean imagebox_layer_move_press ( VikImageboxLayer *vil, GdkEventButton *event, VikViewport *vvp );
static gpointer imagebox_layer_compass_rose_move_create ( VikWindow *vw, VikViewport *vvp);
static gboolean imagebox_layer_compass_rose_move_press ( VikImageboxLayer *vil, GdkEventButton *event, VikViewport *vvp );

static VikToolInterface imagebox_tools[] = {
  { N_("Recenter Imagebox"), (VikToolConstructorFunc) imagebox_layer_move_create, NULL, NULL, NULL,
    (VikToolMouseFunc) imagebox_layer_move_press, NULL, NULL,
    (VikToolKeyFunc) NULL, GDK_CURSOR_IS_PIXMAP, &cursor_geomove_pixbuf }, /* TODO: get rid of cursor_geomove_pixbuf and create an image from this. I copied this from georeflayer. */
  { N_("Recenter Compass Rose"), (VikToolConstructorFunc) imagebox_layer_compass_rose_move_create, NULL, NULL, NULL,
    (VikToolMouseFunc) imagebox_layer_compass_rose_move_press, NULL, NULL,
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
  gchar *compass_rose_filename;
  gint16 compass_rose_x_offset, compass_rose_y_offset;
  gint16 compass_rose_coordtext_x_offset, compass_rose_coordtext_y_offset;
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
    case PARAM_FILENAME:
      if (vil->filename)
        g_free(vil->filename);
      vil->filename = g_strdup(data.s);
      break;
    case PARAM_CENTER_LAT: vil->center.lat = data.d; break;
    case PARAM_CENTER_LON: vil->center.lon = data.d; break;
    case PARAM_ZOOM_FACTOR: vil->zoom_factor = data.u; break;
    case PARAM_PIXELS_WIDTH: vil->pixels_width = data.u; break;
    case PARAM_PIXELS_HEIGHT: vil->pixels_height = data.u; break;
    case PARAM_COMPASS_ROSE_FILENAME: 
      if (vil->compass_rose_filename)
        g_free(vil->compass_rose_filename);
      vil->compass_rose_filename = g_strdup(data.s);
      break;
    case PARAM_COMPASS_ROSE_X_OFFSET: vil->compass_rose_x_offset = data.i; break;
    case PARAM_COMPASS_ROSE_Y_OFFSET: vil->compass_rose_y_offset = data.i; break;
    case PARAM_COMPASS_ROSE_COORDTEXT_X_OFFSET: vil->compass_rose_coordtext_x_offset = data.i; break;
    case PARAM_COMPASS_ROSE_COORDTEXT_Y_OFFSET: vil->compass_rose_coordtext_y_offset = data.i; break;
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
    case PARAM_COMPASS_ROSE_FILENAME: rv.s = vil->compass_rose_filename ? vil->compass_rose_filename : ""; break;
    case PARAM_COMPASS_ROSE_X_OFFSET: rv.i = vil->compass_rose_x_offset; break;
    case PARAM_COMPASS_ROSE_Y_OFFSET: rv.i = vil->compass_rose_y_offset; break;
    case PARAM_COMPASS_ROSE_COORDTEXT_X_OFFSET:
                                      rv.i = vil->compass_rose_coordtext_x_offset;
                                      break;
    case PARAM_COMPASS_ROSE_COORDTEXT_Y_OFFSET: rv.i = vil->compass_rose_coordtext_y_offset; break;
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
  vil->compass_rose_filename = NULL;
  vil->gc = NULL;
  vil->center.lat = vil->center.lon = 0.0;
  vil->zoom_factor = 4;
  vil->pixels_width = 1600;
  vil->pixels_height = 900;
  vil->compass_rose_x_offset = vil->compass_rose_y_offset = 0;
  vil->compass_rose_coordtext_x_offset = -80;
  vil->compass_rose_coordtext_y_offset = -140;
  return vil;
}

static gboolean imagebox_layer_has_compass_rose(VikImageboxLayer *vil)
{
    return ( vil->compass_rose_filename &&
      ABS(vil->compass_rose_x_offset) <= vil->pixels_width &&
      ABS(vil->compass_rose_y_offset) <= vil->pixels_height );
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

  // IT DOESNT NEED OT BE DRAW IF it's completely north of screen, west of screen, east of screen, south of screen.
  if ( x2 < vp_box_x1 || y2 < vp_box_y1 || x1 > vp_box_x2 || y1 > vp_box_y2 )
    return; // compass rose doesn't need to be drawn either in this case


  // IT DOESNT NEED TO BE DRAWN if the image box is completely inside viewport. Compass rose might still need to be drawn.
  if ( !( x1 < vp_box_x1 && y1 < vp_box_y1 && x2 > vp_box_x2 && y2 > vp_box_y2) ) {
    // otherwise one of the lines goes thru it. clip coordinates (VikViewport has internal clipping of 9px) and draw it
    gint clipped_x1 = MAX(x1, vp_box_x1);
    gint clipped_y1 = MAX(y1, vp_box_y1);
    gint clipped_x2 = MIN(x2, vp_box_x2);
    gint clipped_y2 = MIN(y2, vp_box_y2);
    vik_viewport_draw_rectangle(vp, vil->gc, FALSE, clipped_x1, clipped_y1, clipped_x2-clipped_x1, clipped_y2-clipped_y1);
  }

  // draw compass rose cross hair
  if (imagebox_layer_has_compass_rose(vil)) {
    gint cr_x = (x1 + x2) / 2 + vil->compass_rose_x_offset * vil->zoom_factor / vp_zoom;
    gint cr_y = (y1 + y2) / 2 + vil->compass_rose_y_offset * vil->zoom_factor / vp_zoom;
    
    if (cr_x > vp_box_x1 && cr_x < vp_box_x2 && cr_y > vp_box_y1 && cr_y < vp_box_y2) {
      vik_viewport_draw_line(vp, vil->gc, cr_x - 5, cr_y, cr_x + 5, cr_y );
      vik_viewport_draw_line(vp, vil->gc, cr_x, cr_y - 5, cr_x, cr_y + 5);
    }

  }
}

void vik_imagebox_layer_free ( VikImageboxLayer *vil )
{
  if ( vil->filename )
    g_free ( vil->filename );
  vil->filename = NULL;

  if ( vil->compass_rose_filename )
    g_free ( vil->compass_rose_filename );
  vil->compass_rose_filename = NULL;

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

static void imagebox_layer_go(gpointer vil_vlp[2])
{
  VikImageboxLayer *vil = VIK_IMAGEBOX_LAYER ( vil_vlp[0] );
  if (! vil->filename || ! *(vil->filename)) {
    a_dialog_error_msg ( VIK_GTK_WINDOW_FROM_LAYER(vil), "No filename to save to!");
    return;
  }

  VikLayersPanel *vlp = VIK_LAYERS_PANEL(vil_vlp[1]);
  VikViewport *vp = vik_layers_panel_get_viewport(vlp);
  VikCoord new_center;

  vik_coord_load_from_latlon( &new_center, vik_viewport_get_coord_mode(vp), &(vil->center));
  vik_viewport_set_center_coord(vp, &new_center);
  vik_viewport_set_zoom ( vp, vil->zoom_factor );

  vik_layer_emit_update ( VIK_LAYER(vil), TRUE );
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

  imagebox_layer_draw_compass_rose(vil, vp);

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

void vik_imagebox_layer_generate_map( VikImageboxLayer *vil, gpointer vlp)
{
  gpointer vil_vlp[2];
  vil_vlp[0] = vil;
  vil_vlp[1] = vlp;
  imagebox_layer_generate_map ( vil_vlp );
}

static void imagebox_layer_draw_compass_rose(VikImageboxLayer *vil, VikViewport *vp)
{
  if (imagebox_layer_has_compass_rose(vil)) {
    // load file or ignore & give warning
    GError *cr_error = NULL;
    GdkPixbuf *compass_rose = gdk_pixbuf_new_from_file ( vil->compass_rose_filename, &cr_error );
    if (cr_error) {
      g_warning ( _("Couldn't open compass rose image file: %s"), cr_error->message );
      g_error_free ( cr_error );
      return;
    }

    // draw onto vp at width / 2 + x_offset - filewidth / 2, height / 2 + y_offset - fileheight / 2
    gint cr_width = gdk_pixbuf_get_width(compass_rose);
    gint cr_height = gdk_pixbuf_get_height(compass_rose);
    gint cr_center_x = vil->pixels_width / 2 + vil->compass_rose_x_offset;
    gint cr_center_y = vil->pixels_height / 2 + vil->compass_rose_y_offset;
    gint dest_x = cr_center_x - cr_width / 2;
    gint dest_y = cr_center_y - cr_height / 2;
    vik_viewport_draw_pixbuf( vp, compass_rose, 0, 0, dest_x, dest_y, cr_width, cr_height );
    g_object_unref( G_OBJECT(compass_rose) );

    /* Coordinate string */
    VikCoord compass_rose_center;
    struct LatLon compass_rose_ll;
    vik_viewport_screen_to_coord(vp, cr_center_x, cr_center_y, &compass_rose_center);
    vik_coord_to_latlon(&compass_rose_center, &compass_rose_ll);
    gchar *coordinate_string = g_strdup_printf("%s%.4f\n%s%.4f",
        compass_rose_ll.lat > 0 ? "N" : "S",
        ABS(compass_rose_ll.lat),
        compass_rose_ll.lon > 0 ? "E" : "W",
        ABS(compass_rose_ll.lon)
    );

    PangoLayout *pl = gtk_widget_create_pango_layout(GTK_WIDGET(vp), NULL);
    PangoFontDescription *pfd = pango_font_description_from_string ("Sans 20");
    pango_layout_set_font_description (pl, pfd);
    pango_font_description_free (pfd);
    pango_layout_set_text(pl, coordinate_string, -1);

    vik_viewport_draw_layout(vp, vil->gc,
        cr_center_x + vil->compass_rose_coordtext_x_offset,
        cr_center_y + vil->compass_rose_coordtext_y_offset,
        pl
    );

    g_object_unref(pl);
    g_free(coordinate_string);
  }
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

  item = gtk_image_menu_item_new_with_mnemonic ( _("Go to map center/zoom") );
  gtk_image_menu_item_set_image ( (GtkImageMenuItem*)item, gtk_image_new_from_stock (GTK_STOCK_ZOOM_FIT, GTK_ICON_SIZE_MENU) );
  g_signal_connect_swapped ( G_OBJECT(item), "activate", G_CALLBACK(imagebox_layer_go), pass_along );
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

static gpointer imagebox_layer_compass_rose_move_create ( VikWindow *vw, VikViewport *vvp)
{
  return vvp;
}

static gboolean imagebox_layer_compass_rose_move_press ( VikImageboxLayer *vil, GdkEventButton *event, VikViewport *vvp )
{
  if (!vil || vil->vl.type != VIK_LAYER_IMAGEBOX || !vil->compass_rose_filename || !*vil->compass_rose_filename)
    return FALSE;

  VikCoord imagebox_center;
  gint x, y;
  vik_coord_load_from_latlon(&imagebox_center, vik_viewport_get_coord_mode(vvp), &(vil->center));
  vik_viewport_coord_to_screen(vvp, &imagebox_center, &x, &y);
  vil->compass_rose_x_offset = (event->x - x) * vik_viewport_get_xmpp(vvp) / vil->zoom_factor;
  vil->compass_rose_y_offset = (event->y - y) * vik_viewport_get_ympp(vvp) / vil->zoom_factor;
  vik_layer_emit_update ( VIK_LAYER(vil), FALSE );
  return TRUE; /* I didn't move anything on this layer! */
}

