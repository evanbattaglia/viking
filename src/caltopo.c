/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2012, Evan Battaglia <gtoevan@gmx.net>
 * Copyright (C) 2008, Guilhem Bonnefille <guilhem.bonnefille@gmail.com>
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

#include "caltopo.h"
#include "vikmapslayer.h"
#include "vikslippymapsource.h"

/* initialisation */
void caltopo_init ()
{
  VikMapSource *caltopo_type = VIK_MAP_SOURCE(g_object_new(VIK_TYPE_SLIPPY_MAP_SOURCE,
							      "id", 29,
							      "label", "CalTopo USGS 7.5\" Topos",
							      "hostname", "s3-us-west-1.amazonaws.com",
							      "url", "/caltopo/topo/%d/%d/%d.png?v=1",
							      "copyright", "CalTopo Terms of Use",
							      "license", "CalTopo Terms of Use",
							      "license-url", "http://caltopo.com/",
							      NULL));
  /* Matt Jacobs from CalTopo told me (Evan Battaglia): "You guys should go
   * ahead and use the tiles, and if bandwidth becomes a concern we can sort
   * it out then." :) 2012-04-28
   */

  maps_layer_register_map_source (caltopo_type);
}

