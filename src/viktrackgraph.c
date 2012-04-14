#include "viking.h"
#include "viktrackgraph.h"

// TODO: change adjacency lists into lists not arrays
struct _VikTrackgraph {
  GHashTable *adjacency_lists;

};

typedef struct {
  VikTrackgraphNode *node;
  gdouble distance;
} NodeAndDistance;

static guint vik_trackgraph_node_hash(VikTrackgraphNode *node) {
  return 33 * node->is_endpoint + g_str_hash(node->track_name);
}
static gboolean vik_trackgraph_node_equal(VikTrackgraphNode *node1, VikTrackgraphNode *node2) {
  return g_str_equal(node1->track_name, node2->track_name) && node1->is_endpoint == node2->is_endpoint;
}

static void adjacency_list_free(GArray *adjacency_list) {
  gint i;
  for (i = 0; i < adjacency_list->len; i++)
    g_free(g_array_index(adjacency_list, gpointer, i)); /* NodeAndDistance */
  g_array_unref(adjacency_list);
}

VikTrackgraph *vik_trackgraph_new()
{
  VikTrackgraph *rv = g_malloc(sizeof(VikTrackgraph));
  rv->adjacency_lists = g_hash_table_new_full((GHashFunc) vik_trackgraph_node_hash, (GEqualFunc) vik_trackgraph_node_equal,
                                              g_free /* to free node */, (GDestroyNotify) adjacency_list_free);
  return rv;
}

void vik_trackgraph_free_all(VikTrackgraph *tg)
{
  g_hash_table_destroy(tg->adjacency_lists);
  g_free(tg);
}

/* We will free the node. Is this function necessary? */
void vik_trackgraph_add_node(VikTrackgraph *tg, VikTrackgraphNode *n)
{
  if (g_hash_table_lookup(tg->adjacency_lists, n))
    g_free(n); // already there
  else {
    g_hash_table_insert(tg->adjacency_lists, n, g_array_new(FALSE, TRUE, sizeof(gpointer)));
  }
}

static NodeAndDistance *node_and_distance_new(VikTrackgraphNode *node, gdouble distance) {
  NodeAndDistance *rv = g_malloc(sizeof(NodeAndDistance));
  rv->node = node;
  rv->distance = distance;
  return rv;
}

/* n1 and n2 have to be in hashtable (value-wise, not pointer-wise) */
void vik_trackgraph_add_edge(VikTrackgraph *tg, const VikTrackgraphNode *n1, const VikTrackgraphNode *n2, gdouble distance)
{
  VikTrackgraphNode *orig_n1, *orig_n2;
  GArray *array_n1, *array_n2;
  g_assert(g_hash_table_lookup_extended(tg->adjacency_lists, n1, (gpointer *) &orig_n1, (gpointer *) &array_n1));
  g_assert(g_hash_table_lookup_extended(tg->adjacency_lists, n2, (gpointer *) &orig_n2, (gpointer *) &array_n2));

  NodeAndDistance *to_n2 = node_and_distance_new(orig_n2, distance);
  NodeAndDistance *to_n1 = node_and_distance_new(orig_n1, distance);
  g_array_append_val(array_n1, to_n2);
  g_array_append_val(array_n2, to_n1);
}

/* can be g_freed, or added to a trackgraph */
VikTrackgraphNode *vik_trackgraph_node_new(const gchar *track_name, VikTrackgraphNodeEndpoint ep)
{
  VikTrackgraphNode *rv = g_malloc(sizeof(VikTrackgraphNode));
  rv->track_name = track_name; /* TODO: copy and free this with the node? */
  rv->is_endpoint = ep;
  return rv;
}

void vik_trackgraph_node_free(VikTrackgraphNode *n)
{
  g_free(n);
}


// TODO
#define DIJKSTRA_INFINITY 99999999999.0

/* returns an array of VikTrackgraph nodes. Free the array without freeing the data. saves distance to total_distance */

typedef struct {
  gpointer *heap_data;
  gdouble  *heap_weights;
  guint heap_length;
  GHashTable *heap_positions;
} DijkstraHeapHash;

void dijkstra_heap_hash_swap(DijkstraHeapHash *dhh, guint i1, guint i2)
{
  // swap dhh->heap_data
  gpointer tmp = dhh->heap_data[i1];
  dhh->heap_data[i1] = dhh->heap_data[i2];
  dhh->heap_data[i2] = tmp;

  // swap dhh->heap_weights
  gdouble tmp2 = dhh->heap_weights[i1];
  dhh->heap_weights[i1] = dhh->heap_weights[i2];
  dhh->heap_weights[i2] = tmp2;

  // update dhh->heap_positions
  g_hash_table_insert(dhh->heap_positions, dhh->heap_data[i1], GINT_TO_POINTER(i1));
  g_hash_table_insert(dhh->heap_positions, dhh->heap_data[i2], GINT_TO_POINTER(i2));
}



/* Returns ultimate position */
guint dijkstra_heap_hash_sift_down(DijkstraHeapHash *dhh, guint index)
{
  guint left = 2*(index+1) - 1;
  guint right = 2*(index+1);
  guint smallest = index;
  if (left < dhh->heap_length && dhh->heap_weights[left] < dhh->heap_weights[smallest])
      smallest = left;
  if (right < dhh->heap_length && dhh->heap_weights[right] < dhh->heap_weights[smallest])
      smallest = right;
  if (smallest != index) {
    dijkstra_heap_hash_swap(dhh, index, smallest);
    return dijkstra_heap_hash_sift_down(dhh, smallest);
  }
  else
    return index;
}


guint dijkstra_heap_hash_sift_up(DijkstraHeapHash *dhh, guint index)
{
  if (index == 0)
    return index;
  guint parent_index = (index & 0x1) ? ((index + 1)/2 - 1) : (index/2 - 1);
  if ( dhh->heap_weights[parent_index] > dhh->heap_weights[index] ) {
    dijkstra_heap_hash_swap(dhh, index, parent_index);
    return dijkstra_heap_hash_sift_up(dhh, parent_index);
  } else
    return index;
}



DijkstraHeapHash *dijkstra_heap_hash_new(GList *data, gdouble default_weight)
{

  DijkstraHeapHash *dhh = g_malloc(sizeof(DijkstraHeapHash));
  dhh->heap_length = g_list_length(data);
  dhh->heap_data = g_malloc(sizeof(gpointer) * dhh->heap_length);
  dhh->heap_weights = g_malloc(sizeof(gdouble) * dhh->heap_length);
  dhh->heap_positions = g_hash_table_new((GHashFunc) g_direct_hash, (GEqualFunc) g_int_equal);

  GList *iter = data;
  guint i;
  for (i = 0; iter; iter = iter->next, i++) {
    dhh->heap_data[i] = iter->data;
    dhh->heap_weights[i] = default_weight;
    g_hash_table_insert(dhh->heap_positions, dhh->heap_data[i], GINT_TO_POINTER(i));
  }

  return dhh;
}

void dijkstra_heap_hash_pop(DijkstraHeapHash *dhh, gpointer *node, gdouble *weight)
{
  g_assert(dhh->heap_length);

  *node = dhh->heap_data[0];
  *weight = dhh->heap_weights[0];
  g_hash_table_remove(dhh->heap_positions, *node);
  dhh->heap_length--;

  // bring last to top
  dhh->heap_data[0] = dhh->heap_data[dhh->heap_length];
  dhh->heap_weights[0] = dhh->heap_weights[dhh->heap_length];
  dijkstra_heap_hash_sift_down(dhh, 0);
}

gdouble dijkstra_heap_hash_get_weight(DijkstraHeapHash *dhh, gpointer data)
{
  guint index;
  gboolean found_index = g_hash_table_lookup_extended(dhh->heap_positions, data, NULL, (gpointer *) &index);
  g_assert(found_index);

  return dhh->heap_weights[index];
}

gboolean dijkstra_heap_hash_includes(DijkstraHeapHash *dhh, gpointer data)
{
  return g_hash_table_lookup_extended(dhh->heap_positions, data, NULL, NULL);
}

void dijkstra_heap_hash_update(DijkstraHeapHash *dhh, gpointer data, gdouble new_priority)
{
  guint index;
  gboolean found_index = g_hash_table_lookup_extended(dhh->heap_positions, data, NULL, (gpointer *) &index);
  g_assert(found_index);

  dhh->heap_weights[index] = new_priority;
  dijkstra_heap_hash_sift_down(dhh, dijkstra_heap_hash_sift_up(dhh, index));
}

gboolean dijkstra_heap_hash_empty(DijkstraHeapHash *dhh)
{
  return dhh->heap_length == 0;
}

void dijkstra_heap_hash_free(DijkstraHeapHash *dhh)
{
  g_free(dhh->heap_data);
  g_free(dhh->heap_weights);
  g_hash_table_destroy(dhh->heap_positions);
  g_free(dhh);
}


GList *vik_trackgraph_dijkstra(VikTrackgraph *tg, VikTrackgraphNode *start_node, VikTrackgraphNode *end_node, gdouble *total_distance)
{
  // get canonical nodes used in the graph
  VikTrackgraphNode *start;
  VikTrackgraphNode *end;
  g_assert(g_hash_table_lookup_extended(tg->adjacency_lists, start_node, (gpointer *) &start, NULL));
  g_assert(g_hash_table_lookup_extended(tg->adjacency_lists, end_node, (gpointer *) &end, NULL));


  // distance from from source to each node
  GHashTable *previous = g_hash_table_new(g_direct_hash, g_direct_equal);
  // default weights are infinity, except

  GList *nodes = g_hash_table_get_keys(tg->adjacency_lists);
  DijkstraHeapHash *heap_hash = dijkstra_heap_hash_new(nodes, DIJKSTRA_INFINITY);
  g_list_free(nodes);
  dijkstra_heap_hash_update(heap_hash, start, 0);

  while (! dijkstra_heap_hash_empty(heap_hash)) {
    gdouble current_node_distance;
    VikTrackgraphNode *current_node;
    dijkstra_heap_hash_pop(heap_hash, (gpointer *) &current_node, &current_node_distance);
    
    if (current_node_distance == DIJKSTRA_INFINITY) {
      g_hash_table_destroy(previous);
      dijkstra_heap_hash_free(heap_hash);
      return NULL; // unreachable
    }
    if (end == current_node) {
      *total_distance = current_node_distance;
      GList *rv = g_list_prepend(NULL, end);
      VikTrackgraphNode *iter = end;
      while (iter != start)
        rv = g_list_prepend(rv, iter = g_hash_table_lookup(previous, iter));

      g_hash_table_destroy(previous);
      dijkstra_heap_hash_free(heap_hash);
      return rv;
    }

    GArray *neighbors = g_hash_table_lookup(tg->adjacency_lists, current_node);
    gint i;
    for (i = 0; i < neighbors->len; i++) {
      NodeAndDistance *neighbor_and_distance = g_array_index(neighbors, NodeAndDistance *, i);
      VikTrackgraphNode *neighbor = neighbor_and_distance->node;
      if (dijkstra_heap_hash_includes(heap_hash, neighbor_and_distance->node))
      {  // don't retry completed nodes
        gdouble dist = dijkstra_heap_hash_get_weight(heap_hash, neighbor_and_distance->node);
        gdouble new_dist = current_node_distance + neighbor_and_distance->distance;
        if (new_dist < dist) {
          g_hash_table_insert(previous, neighbor, current_node);
          dijkstra_heap_hash_update(heap_hash, neighbor_and_distance->node, new_dist);
        }
      }
    }
  }
  g_assert(FALSE); // should never get here
  return NULL;
}


