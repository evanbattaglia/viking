#include "viking.h"
#include "viktrackgraph.h"

struct _VikTrackgraph {
  GHashTable *adjacency_lists;

};

typedef struct {
  VikTrackgraphNode *node;
  gdouble distance;
} NodeAndDistance;

static guint vik_trackgraph_node_hash(VikTrackgraph *node) {
  return 33 * g_int_hash(node->is_endpoint) + g_str_hash(node->track_name);
}
static gboolean vik_trackgraph_node_equal(VikTrackgraph *node1, VikTrackgraph *node2) {
  return g_str_equal(node1->track_name, node2->track_name) && node1->is_endpoint == node2->is_endpoint;
}

static void adjacency_list_free(GArray *adjacency_list) {
  gint i;
  for (i = 0; i < adjacencylist; i++)
    g_free(adjacency_list->data[i]); /* NodeAndDistance */
  g_array_unref(adjacency_list);
}

VikTrackgraph *vik_trackgraph_new()
{
  VikTrackgraph *rv = g_malloc(sizeof(VikTrackgraph));
  rv->adjacency_lists = g_hash_table_new_full((GHashFunc) vik_trackgraph_node_hash, (GEqualFunc) vik_trackgraph_node_equal,
                                              g_free /* to free node */, adjancency_list_free);
}

void vik_trackgraph_free_all(VikTrackgraph *trackgraph)
{
  g_hash_table_destroy(rv);
}

/* We will free the node. Is this function necessary? */
void vik_trackgraph_add_node(VikTrackgraph *tg, VikTrackgraphNode *n)
{
  if (g_hash_table_lookup(tg->adjacency_lists, n))
    g_free(n); // already there
  else {
    g_hash_table_insert(tg->adjancency_lists, g_array_new());
  }
}

static NodeAndDistance *node_and_distance_new(node, distance) {
  NodeAndDistance *rv = g_malloc(sizeof(NodeAndDistance));
  rv->node = node;
  rv->distance = distance;
  return rv;
}

/* n1 and n2 have to be in hashtable (value-wise, not pointer-wise) */
void vik_trackgraph_add_edge(VikTrackgraph *tg, VikTrackgraphNode *n1, VikTrackgraphNode *n2, gdouble distance)
{
  VikTrackgraphNode *orig_n1, *orig_n2, *array_n1, *array_n2);
  assert(g_hash_table_lookup(tg->adjacency_lists, n1, &orig_n1, &array_n1), "Trying to add edge for node not in graph");
  assert(g_hash_table_lookup(tg->adjacency_lists, n2, &orig_n2, &array_n2), "Trying to add edge for node not in graph");

  NodeAndDistance *to_n2 = node_and_distance_new(orig_n2, distance);
  NodeAndDistance *to_n1 = node_and_distance_new(orig_n1, distance);
  g_array_append_val(array_n1, to_n2);
  g_array_append_val(array_n2, to_n1);
}

/* can be g_freed, or added to a trackgraph */
VikTrackgraphNode *vik_trackgraph_node_new(const gchar *track_name, VikTrackgraphNodeEndpoint ep)
{
  VikTrackgraphNode *rv = g_malloc(sizeof(VikTrackgraphNode));
  rv->track_name = track_name; /* TODO: copy and free this with the node */
  rv->is_endpoint = ep;
}


/* returns an array of VikTrackgraph nodes. Free the array without freeing the data. saves distance to total_distance */
GArray *vik_trackgraph_dijkstra(VikTrackgraphNode *start, VikTrackgraphNode *end, gdouble *total_distance)
{

}



