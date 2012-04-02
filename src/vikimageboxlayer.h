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

#ifndef _VIKING_IMAGEBOXLAYER_H
#define _VIKING_IMAGEBOXLAYER_H

#include "viklayer.h"

#define VIK_IMAGEBOX_LAYER_TYPE            (vik_imagebox_layer_get_type ())
#define VIK_IMAGEBOX_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIK_IMAGEBOX_LAYER_TYPE, VikImageBoxLayer))
#define VIK_IMAGEBOX_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), VIK_IMAGEBOX_LAYER_TYPE, VikImageBoxLayerClass))
#define IS_VIK_IMAGEBOX_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIK_IMAGEBOX_LAYER_TYPE))
#define IS_VIK_IMAGEBOX_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIK_IMAGEBOX_LAYER_TYPE))

typedef struct _VikImageBoxLayerClass VikImageBoxLayerClass;
struct _VikImageBoxLayerClass
{
  VikLayerClass object_class;
};

GType vik_imagebox_layer_get_type ();

typedef struct _VikImageboxLayer VikImageboxLayer;



VikImageboxLayer *vik_imagebox_layer_new ( );
void vik_imagebox_layer_draw ( VikImageboxLayer *vil, gpointer data );
void vik_imagebox_layer_free ( VikImageboxLayer *vil );

VikImageboxLayer *vik_imagebox_layer_create ( VikViewport *vp );
gboolean vik_imagebox_layer_properties ( VikImageLayer *vcl, gpointer vp );


#endif
