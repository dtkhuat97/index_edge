/**
 * @file cgraph.h
 * @author FR
 * @brief header file of the library
 */

#ifndef LIBCGRAPH_H
#define LIBCGRAPH_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32) && !defined(CGRAPH_STATIC) && defined(CGRAPH_INTERNAL)
#  define CGRAPH_API __declspec(dllexport)
#elif defined(_WIN32) && !defined(CGRAPH_STATIC)
#  define CGRAPH_API __declspec(dllimport)
#elif defined(__GNUC__)
#  define CGRAPH_API __attribute__((visibility("default")))
#else
#  define CGRAPH_API
#endif

// Determines if libcgraph was build with support for RRR
#cmakedefine RRR 1

/**
 * Type used as the handler for the API calls for compressing and writing graphs.
 * The exact type of the handler is not defined in the header,
 * because it should only be known internally by the cgraph functions.
 */
typedef struct CGraphW_ CGraphW;

/**
 * Contains several parameters to influence the compression.
 */
typedef struct {
	// Maximum rank
	int max_rank;

	// Replace monograms
	bool monograms;

	// Factor for bitsequences
	int factor;

	// Sampling for searching for nodes
	int sampling;

	// Using the run-length encoding
	bool rle;

	// Add the extra NT table
	bool nt_table;
#ifdef RRR
	// Using bitsequences of type RRR
	bool rrr;
#endif
} CGraphCParams;

/**
 * Type used as the handler for the API calls for reading compressed graphs.
 * The exact type of the handler is not defined in the header,
 * because it should only be known internally by the cgraph functions.
 */
typedef struct CGraphR_ CGraphR;

#define CGRAPH_LABELS_ALL ((CGraphEdgeLabel) -1)
#define CGRAPH_NODES_ALL ((CGraphNode) -1)
/**
 * Type used as parameters for the functions to read a compressed graph.
 */
typedef int64_t CGraphNode, CGraphEdgeLabel;
typedef int64_t CGraphRank;

/**
 * Type used for the iterator of node ids.
 */
typedef struct CGraphNodeIterator_ CGraphNodeIterator;

/**
 * Type used for the iterator of the adjacent edges.
 */
typedef struct CGraphEdgeIterator_ CGraphEdgeIterator;

/**
 * Type used to return an edge at a node.
 */
typedef struct {
	/* The rank of the edge. */
    CGraphRank rank;

	/* The edge label of the edge. */
	CGraphEdgeLabel label;

    /* The nodes of the edge */
    CGraphNode* nodes;
} CGraphEdge;

/**
 * Creates a handler to compress an existing graph.
 * If the handler could not be created, `NULL` is returned.
 * 
 * @return Handler of the graph compressor.
 */
CGRAPH_API
CGraphW* cgraphw_init();

/**
 * Frees all resources of this handler.
 * 
 * @param g Handler of the graph compressor.
 */
CGRAPH_API
void cgraphw_destroy(CGraphW* g);

/**
 * Adds a new edge to the graph.
 * The two nodes and the edge label must be a 0-byte terminated string.
 * So strings with value NULL are not allowed.
 * 
 * @param g Handler of the graph compressor.
 * @param n1 Source node of the edge.
 * @param n2 Destination node of the edge.
 * @param label Label of the edge.
 * @return 0, if no errors occurred, otherwise -1.
 */
CGRAPH_API
int cgraphw_add_edge(CGraphW* g, const CGraphRank rank, const char* label, const char** nodes);

/**
 * Adds a new node to the graph.
 * The node must be a 0-byte terminated string.
 * So strings with value NULL are not allowed.
 * 
 * If an adjacency edge exists for each node, calling this function is not needed.
 * 
 * @param g Handler of the graph compressor.
 * @param n Node value.
 * @return 0, if no errors occurred, otherwise -1.
 */
CGRAPH_API
int cgraphw_add_node(CGraphW* g, const char* n);

/**
 * Sets the compression parameters.
 * Must be done before compressing or writing the graph.
 * 
 * @param g Handler of the graph compressor.
 * @param p Parameters for the compression.
 */
CGRAPH_API
void cgraphw_set_params(CGraphW* g, const CGraphCParams* p);

/**
 * Compresses the internal graph structure with RePair.
 * After this function is called, no more edges can be added to the graph.
 * 
 * @param g Handler of the graph compressor.
 * @return 0, if no errors occurred, otherwise -1.
 */
CGRAPH_API
int cgraphw_compress(CGraphW* g);

/**
 * Writes the compressed graph to a file.
 * This requires that the function `cgraphw_compress` has been called beforehand.
 * If not, the function fails.
 * If a file exists at the given path, it will be overwritten.
 * 
 * @param g Handler of the graph compressor.
 * @param path Destination of the compressed graph.
 * @return 0, if no errors occurred, otherwise -1.
 */
CGRAPH_API
int cgraphw_write(CGraphW* g, const char* path, bool verbose);

/**
 * Creates a handler for a file of a compressed graph.
 * If the file was not read correctly, e.g.
 * because the file contains wrong data, `NULL` is returned.
 * 
 * @param path Path of the graph file; can be absolute or relative.
 * @return Handler used for the calls to libcgraph.
 */
CGRAPH_API
CGraphR* cgraphr_init(const char* path);

/**
 * Frees the resources of this handler and does the unmapping of the files from the memory.
 * 
 * @param g Handler of the graph reader.
 */
CGRAPH_API
void cgraphr_destroy(CGraphR* g);

/**
 * Returns the number of nodes in the graph.
 * The number is returned as the type `size_t`.
 * To search nodes with the function `cgraphr_search_node`, the nodes need to fit
 * into the memory. So the number of nodes must be representable with `size_t`.
 * 
 * @param g Handler of the graph reader.
 * @return Number of nodes in the graph.
 */
CGRAPH_API
size_t cgraphr_node_count(CGraphR* g);

/**
 * Returns the number of different edge labels in the graph.
 * 
 * @param g Handler of the graph reader.
 * @return Number of edge labels in the graph.
 */
CGRAPH_API
size_t cgraphr_edge_label_count(CGraphR* g);

/**
 * Performs the extract operation for a node on the dictionary of nodes.
 * If the node does not exist, `NULL` is returned.
 * The length which is returned by the parameter `l` is the same as the result of `strlen` on the
 * returned value of this function. So this parameter is optional.
 * The returned data must be freed afterwards with `free()`.
 * 
 * @param g Handler of the graph reader.
 * @param n ID of the node.
 * @param l Output parameter for the length of the returned string.
 * @return Text of the node or `NULL` if it does not exist.
 */
CGRAPH_API
char* cgraphr_extract_node(CGraphR* g, CGraphNode n, size_t* l);

/**
 * Performs the extract operation for a edge label on the dictionary of edge labels.
 * If the edge label does not exist, `NULL` is returned.
 * The length which is returned by the parameter `l` is the same as the result of `strlen` on the
 * returned value of this function. So this parameter is optional.
 * The returned data must be freed afterwards with `free()`.
 * 
 * @param g Handler of the graph reader.
 * @param e ID of the edge label.
 * @param l Output parameter for the length of the returned string.
 * @return Text of the edge label or `NULL` if it does not exist.
 */
CGRAPH_API
char* cgraphr_extract_edge_label(CGraphR* g, CGraphEdgeLabel e, size_t* l);

/**
 * Performs the locate operation on the dictionary of nodes.
 * If the node with this value is not found, -1 is returned.
 * Otherwise the ID of the node is returned.
 * -1 is also returned, if p is NULL.
 * 
 * @param g Handler of the graph reader.
 * @param p Text of the node.
 * @return ID of the node or -1 if not found.
 */
CGRAPH_API
CGraphNode cgraphr_locate_node(CGraphR* g, const char* p);

/**
 * Performs the locate operation on the dictionary of edge labels.
 * If the edge label with this value is not found, -1 is returned.
 * Otherwise the ID of the edge label is returned.
 * -1 is also returned, if p is NULL.
 * 
 * @param g Handler of the graph reader.
 * @param p Text of the edge label.
 * @return ID of the edge label or -1 if not found.
 */
CGRAPH_API
CGraphEdgeLabel cgraphr_locate_edge_label(CGraphR* g, const char* p);

/**
 * Searches the dictionary for nodes whose node label start with the given prefix.
 * If errors happen, `NULL` is returned.
 * Otherwise a iterator is returned, to iterate over the nodes.
 * The iterator is used with the function `cgraphr_node_next`.
 * 
 * @param g Handler of the graph reader.
 * @param p Search text; must not be `NULL` or empty.
 * @return Iterator for the nodes.
 */
CGRAPH_API
CGraphNodeIterator* cgraphr_locate_node_prefix(CGraphR* g, const char* p);

/**
 * Determines the next element of the node iterator.
 * If an element exist; `true` is returned and the node is passed via the second parameterr.
 * Else, this function returns `false`.
 * If the nodes are iterated, until no further element was found,
 * all memory is freed with the last call to this function.
 * 
 * @param it Iterator for the nodes.
 * @param n Parameter to return the node.
 * @return `true` if a node exists; else `false`.
 */
CGRAPH_API
bool cgraphr_node_next(CGraphNodeIterator* it, CGraphNode* n);

/**
 * This function ends the iteration of the node iterator.
 * If the node iterator is iterated, until the last element was found, this function must not be called.
 * But if not, this function has to be called to free up memory.
 * 
 * @param it Iterator for the nodes.
 */
CGRAPH_API
void cgraphr_node_finish(CGraphNodeIterator* it);

/**
 * Searches the dictionary for nodes whose node label contain the given text.
 * If errors happen, `NULL` is returned.
 * Otherwise a iterator is returned, to iterate over the nodes.
 * The iterator is used with the function `cgraphr_node_next`.
 * 
 * @param g Handler of the graph reader.
 * @param p Search text; must not be `NULL` or empty.
 * @return Iterator for the nodes.
 */
CGRAPH_API
CGraphNodeIterator* cgraphr_search_node(CGraphR* g, const char* p);

/**
 * Determines outgoing edges of a node `node`.
 * To determine specific edges with a given edge label, this can be done with the parameter `label`.
 * If all outgoing edges should be returned, `CGRAPH_LABELS_ALL` should be passed for the parameter `label`.
 * The list of edges is returned via a iterator.
 * The values of the iterator can be iterated with the function `cgraphr_edge_next`.
 * 
 * @param g Handler of the graph reader.
 * @param n ID of an node.
 * @return Iterator for the edges.
 */
CGRAPH_API
CGraphEdgeIterator* cgraphr_edges(CGraphR* g, CGraphRank rank, CGraphEdgeLabel label, const CGraphNode* nodes);

/**
 * Determines the next element of the edge iterator.
 * If an element exist; `true` is returned and the edge is passed via the second parameter.
 * Else, this function returns `false`.
 * If the edges are iterated, until no further element was found,
 * all memory is freed with the last call to this function.
 * 
 * @param it Iterator for the edges.
 * @param n Parameter to return the edge.
 * @return `true` if an edge exists; else `false`.
 */
CGRAPH_API
bool cgraphr_edges_next(CGraphEdgeIterator* it, CGraphEdge* e);

/**
 * This function ends the iteration of the edge iterator.
 * If the node iterator is iterated, until the last element was found, this function must not be called.
 * But if not, this function has to be called to free up memory.
 * 
 * @param it Iterator for the edges.
 */
CGRAPH_API
void cgraphr_edges_finish(CGraphEdgeIterator* it);

/**
 * Checks if the given edge exists in the graph.
 * 
 * @param g Handler of the graph reader.
 * @param node1 Source node of the edge.
 * @param node2 Destination node of the edge.
 * @param label Edge label.
 * @return `true` of the edge exists; else `false`.
 */
CGRAPH_API
bool cgraphr_edge_exists(CGraphR* g, CGraphRank rank, CGraphEdgeLabel label, const CGraphNode* nodes);

/**
 * Determines the edges with source node `node1` und destination node `node2`.
 * The edges are returned via an iterator.
 * The values of the iterator can be iterated with the function `cgraphr_edge_next`.
 * 
 * @param g Handler of the graph reader.
 * @param node1 Source node of the edges.
 * @param node2 Destination node of the edges.
 * @return Iterator for the edges.
 */
CGRAPH_API
CGraphEdgeIterator* cgraphr_edges_connecting(CGraphR* g, CGraphRank rank, const CGraphNode* nodes);

/**
 * Checks if there is an edge from node `node1` to node `node2`.
 * This is equivalent with checking, if the function `cgraphr_edges_connecting` yield an edge.
 * 
 * @param g Handler of the graph reader.
 * @param node1 Source node.
 * @param node2 Destination node.
 * @return `true` if an edge exists from `node1` to `node2`; else `false`.
 */
CGRAPH_API
bool cgraphr_nodes_connected(CGraphR* g, CGraphRank rank, const CGraphNode* nodes);

/**
* Checks if there is an edge from node `node1` to node `node2`.
* This is equivalent with checking, if the function `cgraphr_edges_connecting` yield an edge.
*
* @param g Handler of the graph reader.
* @param node1 Source node.
* @param node2 Destination node.
* @return `true` if an edge exists from `node1` to `node2`; else `false`.
*/
CGRAPH_API
CGraphEdgeIterator* cgraphr_edges_by_predicate(CGraphR* g, CGraphEdgeLabel label);

#endif
