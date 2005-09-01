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

#include <string.h>

enum {
  VLP_UPDATE_SIGNAL,
  VLP_LAST_SIGNAL
};

static void layers_panel_class_init ( VikLayersPanelClass *klass );
static void layers_panel_init ( VikLayersPanel *vlp );
static void layers_item_edited (VikLayersPanel *vlp, GtkTreeIter *iter, const gchar *new_text);
static void layers_item_toggled (VikLayersPanel *vlp, GtkTreeIter *iter);

static guint layers_panel_signals[VLP_LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class;

struct _VikLayersPanel {
  GtkVBox vbox;

  VikAggregateLayer *toplayer;
  GtkTreeIter toplayer_iter;

  VikTreeview *vt;
  VikViewport *vvp; /* reference */

  GtkItemFactory *popup_factory;
};

static GtkItemFactoryEntry base_entries[] = {
 { "/_Delete", NULL, (GtkItemFactoryCallback) vik_layers_panel_delete_selected, -1, "<StockItem>", GTK_STOCK_DELETE },
 { "/New Layer", NULL, NULL, -1, "<Branch>" },
};

#define NUM_BASE_ENTRIES 2

static void layers_item_toggled (VikLayersPanel *vlp, GtkTreeIter *iter);
static void layers_item_edited (VikLayersPanel *vlp, GtkTreeIter *iter, const gchar *new_text);
static void layers_popup_cb (VikLayersPanel *vlp);
static void layers_popup ( VikLayersPanel *vlp, GtkTreeIter *iter, gint mouse_button );
static gboolean layers_button_press_cb (VikLayersPanel *vlp, GdkEventButton *event);
static void layers_move_item ( VikLayersPanel *vlp, gboolean up );
static void layers_move_item_up ( VikLayersPanel *vlp );
static void layers_move_item_down ( VikLayersPanel *vlp );
static void layers_panel_finalize ( GObject *gob );

GType vik_layers_panel_get_type()
{
  static GType vlp_type = 0;

  if (!vlp_type)
  {
    static const GTypeInfo vlp_info = 
    {
      sizeof (VikLayersPanelClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) layers_panel_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof (VikLayersPanel),
      0,
      (GInstanceInitFunc) layers_panel_init,
    };
    vlp_type = g_type_register_static ( GTK_TYPE_VBOX, "VikLayersPanel", &vlp_info, 0 );
  }

  return vlp_type;
}

static void layers_panel_class_init ( VikLayersPanelClass *klass )
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = layers_panel_finalize;

  parent_class = g_type_class_peek_parent (klass);

  layers_panel_signals[VLP_UPDATE_SIGNAL] = g_signal_new ( "update", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (VikLayersPanelClass, update), NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

VikLayersPanel *vik_layers_panel_new ()
{
  return VIK_LAYERS_PANEL ( g_object_new ( VIK_LAYERS_PANEL_TYPE, NULL ) );
}

void vik_layers_panel_set_viewport ( VikLayersPanel *vlp, VikViewport *vvp )
{
  vlp->vvp = vvp;
  /* TODO: also update GCs (?) */
}

VikViewport *vik_layers_panel_get_viewport ( VikLayersPanel *vlp )
{
  return vlp->vvp;
}

static void layers_panel_init ( VikLayersPanel *vlp )
{
  GtkWidget *hbox;
  GtkWidget *upbutton, *upimage, *downbutton, *downimage;
  GtkWidget *scrolledwindow;
  GtkItemFactoryEntry entry;
  guint i, tmp;

  vlp->vvp = NULL;

  hbox = gtk_hbox_new ( TRUE, 2 );
  vlp->vt = vik_treeview_new ( );

  vlp->toplayer = vik_aggregate_layer_new ();
  vik_layer_rename ( VIK_LAYER(vlp->toplayer), "Top Layer");
  g_signal_connect_swapped ( G_OBJECT(vlp->toplayer), "update", G_CALLBACK(vik_layers_panel_emit_update), vlp );

  vik_treeview_add_layer ( vlp->vt, NULL, &(vlp->toplayer_iter), VIK_LAYER(vlp->toplayer)->name, NULL, vlp->toplayer, VIK_LAYER_AGGREGATE, VIK_LAYER_AGGREGATE );
  vik_layer_realize ( VIK_LAYER(vlp->toplayer), vlp->vt, &(vlp->toplayer_iter) );

  g_signal_connect_swapped ( vlp->vt, "popup_menu", G_CALLBACK(layers_popup_cb), vlp);
  g_signal_connect_swapped ( vlp->vt, "button_press_event", G_CALLBACK(layers_button_press_cb), vlp);
  g_signal_connect_swapped ( vlp->vt, "item_toggled", G_CALLBACK(layers_item_toggled), vlp);
  g_signal_connect_swapped ( vlp->vt, "item_edited", G_CALLBACK(layers_item_edited), vlp);

  upimage = gtk_image_new_from_stock ( GTK_STOCK_GO_UP, GTK_ICON_SIZE_BUTTON );
  upbutton = gtk_button_new ( );
  gtk_container_add ( GTK_CONTAINER(upbutton), upimage );
  gtk_box_pack_start ( GTK_BOX(hbox), upbutton, TRUE, TRUE, 0 );
  g_signal_connect_swapped ( G_OBJECT(upbutton), "clicked", G_CALLBACK(layers_move_item_up), vlp );
  downimage = gtk_image_new_from_stock ( GTK_STOCK_GO_DOWN, GTK_ICON_SIZE_BUTTON );
  downbutton = gtk_button_new ( );
  gtk_container_add ( GTK_CONTAINER(downbutton), downimage );
  gtk_box_pack_start ( GTK_BOX(hbox), downbutton, TRUE, TRUE, 0 );
  g_signal_connect_swapped ( G_OBJECT(downbutton), "clicked", G_CALLBACK(layers_move_item_down), vlp );

  scrolledwindow = gtk_scrolled_window_new ( NULL, NULL );
  gtk_scrolled_window_set_policy ( GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
  gtk_container_add ( GTK_CONTAINER(scrolledwindow), GTK_WIDGET(vlp->vt) );
  
  gtk_box_pack_start ( GTK_BOX(vlp), scrolledwindow, TRUE, TRUE, 0 );
  gtk_box_pack_start ( GTK_BOX(vlp), hbox, FALSE, FALSE, 0 );

  vlp->popup_factory = gtk_item_factory_new ( GTK_TYPE_MENU, "<main>", NULL );
  gtk_item_factory_create_items ( vlp->popup_factory, NUM_BASE_ENTRIES, base_entries, vlp );
  for ( i = 0; i < VIK_LAYER_NUM_TYPES; i++ )
  {
    /* TODO: FIXME: if name has a '/' in it it will get all messed up. why not have an itemfactory field with
                    name, icon, shortcut, etc.? */
    entry.path = g_strdup_printf("%s/New %s Layer", base_entries[NUM_BASE_ENTRIES-1].path, vik_layer_get_interface(i)->name );
    entry.accelerator = NULL;
    entry.callback = (GtkItemFactoryCallback) vik_layers_panel_new_layer;
    entry.callback_action = i;
    if ( vik_layer_get_interface(i)->icon )
    {
      entry.item_type = "<ImageItem>";
      entry.extra_data = gdk_pixdata_serialize ( vik_layer_get_interface(i)->icon, &tmp );
    }
    else
      entry.item_type = "<Item>";

    gtk_item_factory_create_item ( vlp->popup_factory, &entry, vlp, 1 );
    g_free ( (gpointer) entry.extra_data );
    g_free ( entry.path );
  }
}

void vik_layers_panel_emit_update ( VikLayersPanel *vlp )
{
  g_signal_emit ( G_OBJECT(vlp), layers_panel_signals[VLP_UPDATE_SIGNAL], 0 );
}

static void layers_item_toggled (VikLayersPanel *vlp, GtkTreeIter *iter)
{
  gboolean visible;
  gpointer p;
  gint type;

  /* get type and data */
  type = vik_treeview_item_get_type ( vlp->vt, iter );
  p = vik_treeview_item_get_pointer ( vlp->vt, iter );

  switch ( type )
  {
    case VIK_TREEVIEW_TYPE_LAYER: visible = (VIK_LAYER(p)->visible ^= 1); break;
    case VIK_TREEVIEW_TYPE_SUBLAYER: visible = vik_layer_sublayer_toggle_visible ( VIK_LAYER(vik_treeview_item_get_parent(vlp->vt, iter)), vik_treeview_item_get_data(vlp->vt, iter), p); break;
    default: return;
  }

  vik_treeview_item_set_visible ( vlp->vt, iter, visible );

  vik_layers_panel_emit_update ( vlp );
}

static void layers_item_edited (VikLayersPanel *vlp, GtkTreeIter *iter, const gchar *new_text)
{
  if ( vik_treeview_item_get_type ( vlp->vt, iter ) == VIK_TREEVIEW_TYPE_LAYER )
  {
    VikLayer *l;

    /* get iter and layer */
    l = VIK_LAYER ( vik_treeview_item_get_pointer ( vlp->vt, iter ) );

    if ( strcmp ( l->name, new_text ) != 0 )
    {
      vik_layer_rename ( l, new_text );
      vik_treeview_item_set_name ( vlp->vt, iter, l->name );
    }
  }
  else
  {
    const gchar *name = vik_layer_sublayer_rename_request ( vik_treeview_item_get_parent ( vlp->vt, iter ), new_text, vlp, vik_treeview_item_get_data ( vlp->vt, iter ), vik_treeview_item_get_pointer ( vlp->vt, iter ), iter );
    if ( name )
      vik_treeview_item_set_name ( vlp->vt, iter, name);
  }
}

static gboolean layers_button_press_cb ( VikLayersPanel *vlp, GdkEventButton *event )
{
  if (event->button == 3)
  {
    GtkTreeIter iter;
    if ( vik_treeview_get_iter_at_pos ( vlp->vt, &iter, event->x, event->y ) )
    {
      layers_popup ( vlp, &iter, 3 );
      vik_treeview_item_select ( vlp->vt, &iter );
    }
    else
      layers_popup ( vlp, NULL, 3 );
    return TRUE;
  }
  return FALSE;
}

static void layers_popup ( VikLayersPanel *vlp, GtkTreeIter *iter, gint mouse_button )
{
  GtkMenu *menu = NULL;


  if ( iter )
  {
    if ( vik_treeview_item_get_type ( vlp->vt, iter ) == VIK_TREEVIEW_TYPE_LAYER )
    {
      VikLayer *layer = VIK_LAYER(vik_treeview_item_get_pointer ( vlp->vt, iter ));

      if ( layer->type == VIK_LAYER_AGGREGATE )
        menu = GTK_MENU(gtk_item_factory_get_widget ( vlp->popup_factory, "<main>" ));
      else
      {
        GtkWidget *del, *prop;

        menu = GTK_MENU ( gtk_menu_new () );

        prop = gtk_image_menu_item_new_from_stock ( GTK_STOCK_PROPERTIES, NULL );
        g_signal_connect_swapped ( G_OBJECT(prop), "activate", G_CALLBACK(vik_layers_panel_properties), vlp );
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), prop);
        gtk_widget_show ( prop );

        del = gtk_image_menu_item_new_from_stock ( GTK_STOCK_DELETE, NULL );
        g_signal_connect_swapped ( G_OBJECT(del), "activate", G_CALLBACK(vik_layers_panel_delete_selected), vlp );
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), del);
        gtk_widget_show ( del );

        vik_layer_add_menu_items ( layer, menu, vlp );
      } 
    }
    else
    {
      menu = GTK_MENU ( gtk_menu_new () );
      if ( ! vik_layer_sublayer_add_menu_items ( vik_treeview_item_get_parent ( vlp->vt, iter ), menu, vlp, vik_treeview_item_get_data ( vlp->vt, iter ), vik_treeview_item_get_pointer ( vlp->vt, iter ), iter ) )
      {
        gtk_widget_destroy ( GTK_WIDGET(menu) );
        return;
      }
      /* TODO: copy, paste, specific things for different types */
    }
  }
  else
    menu = GTK_MENU(gtk_item_factory_get_widget ( vlp->popup_factory, base_entries[NUM_BASE_ENTRIES-1].path ));
  gtk_menu_popup ( menu, NULL, NULL, NULL, NULL, mouse_button, gtk_get_current_event_time() );
}

static void layers_popup_cb ( VikLayersPanel *vlp )
{
  GtkTreeIter iter;
  layers_popup ( vlp, vik_treeview_get_selected_iter ( vlp->vt, &iter ) ? &iter : NULL, 0 );
}

gboolean vik_layers_panel_new_layer ( VikLayersPanel *vlp, gint type )
{
  VikLayer *l;
  g_assert ( vlp->vvp );
  l = vik_layer_create ( type, vlp->vvp, VIK_GTK_WINDOW_FROM_WIDGET(vlp), TRUE );
  if ( l )
  {
    vik_layers_panel_add_layer ( vlp, l );
    vik_layers_panel_emit_update ( vlp );
    return TRUE;
  }
  return FALSE;
}

void vik_layers_panel_add_layer ( VikLayersPanel *vlp, VikLayer *l )
{
  GtkTreeIter iter;
  GtkTreeIter *replace_iter = NULL;

  /* could be something different so we have to do this */
  vik_layer_change_coord_mode ( l, vik_viewport_get_coord_mode(vlp->vvp) );

  if ( ! vik_treeview_get_selected_iter ( vlp->vt, &iter ) )
    vik_aggregate_layer_add_layer ( vlp->toplayer, l );
  else
  {
    VikAggregateLayer *addtoagg;
    if (vik_treeview_item_get_type ( vlp->vt, &iter ) == VIK_TREEVIEW_TYPE_LAYER )
    {
      if ( ! IS_VIK_AGGREGATE_LAYER(vik_treeview_item_get_pointer ( vlp->vt, &iter )) ) {
        addtoagg = VIK_AGGREGATE_LAYER(vik_treeview_item_get_parent ( vlp->vt, &iter ));
        replace_iter = &iter;
      }
      else
        addtoagg = VIK_AGGREGATE_LAYER(vik_treeview_item_get_pointer ( vlp->vt, &iter ));
    }
    else
    {
      /* a sublayer is selected, first get its parent (layer), then find the layer's parent (aggr. layer) */
      VikLayer *vl = VIK_LAYER(vik_treeview_item_get_parent ( vlp->vt, &iter ));
      replace_iter = &(vl->iter);
      g_assert ( vl->realized );
      addtoagg = VIK_AGGREGATE_LAYER(vik_treeview_item_get_parent ( vlp->vt, &(vl->iter) ) );
    }
    if ( replace_iter )
      vik_aggregate_layer_insert_layer ( addtoagg, l, replace_iter );
    else
      vik_aggregate_layer_add_layer ( addtoagg, l );
  }
}

static void layers_move_item ( VikLayersPanel *vlp, gboolean up )
{
  GtkTreeIter iter;
  VikAggregateLayer *parent;

  /* TODO: deactivate the buttons and stuff */
  if ( ! vik_treeview_get_selected_iter ( vlp->vt, &iter ) )
    return;

  vik_treeview_select_iter ( vlp->vt, &iter ); /* cancel any layer-name editing going on... */

  if ( vik_treeview_item_get_type ( vlp->vt, &iter ) == VIK_TREEVIEW_TYPE_LAYER )
  {
    parent = VIK_AGGREGATE_LAYER(vik_treeview_item_get_parent ( vlp->vt, &iter ));
    if ( parent ) /* not toplevel */
    {
      vik_aggregate_layer_move_layer ( parent, &iter, up );
      vik_layers_panel_emit_update ( vlp );
    }
  }
}

gboolean vik_layers_panel_properties ( VikLayersPanel *vlp )
{
  GtkTreeIter iter;
  g_assert ( vlp->vvp );

  if ( vik_treeview_get_selected_iter ( vlp->vt, &iter ) && vik_treeview_item_get_type ( vlp->vt, &iter ) == VIK_TREEVIEW_TYPE_LAYER )
  {
    if ( vik_treeview_item_get_data ( vlp->vt, &iter ) == VIK_LAYER_AGGREGATE )
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_WIDGET(vlp), "Aggregate Layers have no settable properties." );
    vik_layer_properties ( VIK_LAYER( vik_treeview_item_get_pointer ( vlp->vt, &iter ) ), VIK_GTK_WINDOW_FROM_WIDGET(vlp->vt) );
    return TRUE;
  }
  else
    return FALSE;
}

void vik_layers_panel_draw_all ( VikLayersPanel *vlp )
{
  if ( vlp->vvp && VIK_LAYER(vlp->toplayer)->visible )
    vik_aggregate_layer_draw ( vlp->toplayer, vlp->vvp );
}

void vik_layers_panel_draw_all_using_viewport ( VikLayersPanel *vlp, VikViewport *vvp )
{
  if ( vlp->vvp && VIK_LAYER(vlp->toplayer)->visible )
    vik_aggregate_layer_draw ( vlp->toplayer, vvp );
}

void vik_layers_panel_delete_selected ( VikLayersPanel *vlp )
{
  gint type;
  GtkTreeIter iter;
  
  g_return_if_fail ( vik_treeview_get_selected_iter ( vlp->vt, &iter ) );

  type = vik_treeview_item_get_type ( vlp->vt, &iter );

  if ( type == VIK_TREEVIEW_TYPE_LAYER )
  {
    VikAggregateLayer *parent = vik_treeview_item_get_parent ( vlp->vt, &iter );
    if ( parent )
    {
      if ( vik_aggregate_layer_delete ( parent, &iter ) )
        vik_layers_panel_emit_update ( vlp );
    }
    else
      a_dialog_info_msg ( VIK_GTK_WINDOW_FROM_WIDGET(vlp), "You cannot delete the Top Layer." );
  }
}

VikLayer *vik_layers_panel_get_selected ( VikLayersPanel *vlp )
{
  GtkTreeIter iter, parent;
  gint type;

  if ( ! vik_treeview_get_selected_iter ( vlp->vt, &iter ) )
    return NULL;

  type = vik_treeview_item_get_type ( vlp->vt, &iter );

  while ( type != VIK_TREEVIEW_TYPE_LAYER )
  {
    if ( ! vik_treeview_item_get_parent_iter ( vlp->vt, &iter, &parent ) )
      return NULL;
    iter = parent;
    type = vik_treeview_item_get_type ( vlp->vt, &iter );
  }

  return VIK_LAYER( vik_treeview_item_get_pointer ( vlp->vt, &iter ) );
}

static void layers_move_item_up ( VikLayersPanel *vlp )
{
  layers_move_item ( vlp, TRUE );
}

static void layers_move_item_down ( VikLayersPanel *vlp )
{
  layers_move_item ( vlp, FALSE );
}

gboolean vik_layers_panel_tool ( VikLayersPanel *vlp, guint16 layer_type, VikToolInterfaceFunc tool_func, GdkEventButton *event, VikViewport *vvp )
{
  VikLayer *vl = vik_layers_panel_get_selected ( vlp );
  if ( vl && vl->type == layer_type )
  {
    tool_func ( vl, event, vvp );
    return TRUE;
  }
  else if ( VIK_LAYER(vlp->toplayer)->visible &&
      vik_aggregate_layer_tool ( vlp->toplayer, layer_type, tool_func, event, vvp ) != 1 ) /* either accepted or rejected, but a layer was found */
    return TRUE;
  return FALSE;
}

VikLayer *vik_layers_panel_get_layer_of_type ( VikLayersPanel *vlp, gint type )
{
  VikLayer *rv = vik_layers_panel_get_selected ( vlp );
  if ( rv == NULL || rv->type != type )
    if ( VIK_LAYER(vlp->toplayer)->visible )
      return vik_aggregate_layer_get_top_visible_layer_of_type ( vlp->toplayer, type );
    else
      return NULL;
  else
    return rv;
}

VikAggregateLayer *vik_layers_panel_get_top_layer ( VikLayersPanel *vlp )
{
  return vlp->toplayer;
}

void vik_layers_panel_clear ( VikLayersPanel *vlp )
{
  if ( (! vik_aggregate_layer_is_empty(vlp->toplayer)) && a_dialog_overwrite ( VIK_GTK_WINDOW_FROM_WIDGET(vlp), "Are you sure you wish to delete all layers?", NULL ) )
    vik_aggregate_layer_clear ( vlp->toplayer ); /* simply deletes all layers */
}

void vik_layers_panel_change_coord_mode ( VikLayersPanel *vlp, VikCoordMode mode )
{
  vik_layer_change_coord_mode ( VIK_LAYER(vlp->toplayer), mode );
}

static void layers_panel_finalize ( GObject *gob )
{
  VikLayersPanel *vlp = VIK_LAYERS_PANEL ( gob );
  g_object_unref ( VIK_LAYER(vlp->toplayer) );
  g_object_unref ( G_OBJECT(vlp->popup_factory) );
  G_OBJECT_CLASS(parent_class)->finalize(gob);
}
