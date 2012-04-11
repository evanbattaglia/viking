/*
 * viking -- GPS Data and Topo Analyzer, Explorer, and Manager
 *
 * Copyright (C) 2003-2012, Evan Battaglia <gtoevan@gmx.net> and others (see AUTHORS file)
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
#ifndef _VIKING_TRACKGRAPH_H
#define _VIKING_TRACKGRAPH_H

typedef struct _VikTrackgraph VikTrackgraph;
VikTrackgraph *vik_trackgraph_new();
void vik_trackgraph_free_all(VikTrackgraph *trackgraph); /* Frees graph and all nodes and edges */
void vik_trackgraph_add_node(VikTrackgraph *tg, VikTrackgraphNode *n);
void vik_trackgraph_add_edge(VikTrackgraph *tg, VikTrackgraphNode *n1, VikTrackgraphNode *n2, gdouble distance);

typedef enum { VIK_TRACKGRAPH_NODE_START = 0, VIK_TRACKGRAPH_NODE_END } VikTrackgraphNodeEndpoint;
typedef struct {
  gchar *track_name,
  VikTrackgraphNodeEndpoint is_endpoint;
} VikTrackgraphNode;

VikTrackgraphNode *vik_trackgraph_node_new(const gchar *track_name, VikTrackgraphNodeEndpoint ep); /* can be freed, or added to a trackgraph */

/* returns an array of VikTrackgraph nodes. Free the array without freeing the data. saves distance to total_distance */
GArray *vik_trackgraph_dijkstra(VikTrackgraphNode *start, VikTrackgraphNode *end, gdouble *total_distance)

#endif
