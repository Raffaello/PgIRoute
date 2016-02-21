/*******************************************
**** Boost_Wrapper.hpp					****
**** Header File						****
**** Route Interface for Postgres 9.x	****
**** Features:							****
****   -- Quering Data					****
****   -- Building Graph				****
****   -- Fetch DAta					****
****   -- Management					****
****									****
**** Author : Raffaello Bertini			****
*******************************************/
#pragma once

// Maximal number of nodes in the path (to avoid infinite loops)
#define MAX_NODES 100000000
#define NOWAY 1000000 // oneway cost for reverse (>=) (no traffic allowed)

//#include "pgIRoute.h"
#include "pgIRoute_type.h"
#include <boost/config.hpp>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
//#include <boost/heap/fibonacci_heap.hpp>


using namespace std;
using namespace boost;

struct Edge_t 
{
	int id;
	float8 cost;
};

class Boost_Wrapper
{
private:
	// FIXME: use a template for the directedS parameters
	typedef adjacency_list < listS, vecS, directedS, no_property, Edge_t> graph_t;
	typedef graph_traits < graph_t >::vertex_descriptor vertex_descriptor;
	typedef graph_traits < graph_t >::edge_descriptor edge_descriptor;
	typedef std::pair<int, int> Edge;
	
	char			**err_msg;

	unsigned int	num_nodes;
	unsigned int	_count;
	graph_t			graph;
	edge_t			*edges;

	bool directed;
	bool has_reverse_cost;
	int v_min_id;

	vertex_descriptor _source;
	vertex_descriptor _target;

	std::vector<vertex_descriptor> predecessors;
	std::vector<float8> distances;
	
	/**************************************************************
	 * _i = linear index (ex: [i=2][j=3] (3x3)---> _i = 2*3+3 = 9 *
	 **************************************************************/
	void inline FreeAdjacencyMatrix(const int _i, multipath_t **C)
	{
		int i=_i;
		while(--i>=0)
			free((*C)[i].path);
		free((*C));
	}

	/*
	* Dijkstra with heap testing, check, try compile and so on....
	* Not working yet, code need to be rewrited!
	*/
	/*	
	int node_distance(std::string const& s1, std::string const& s2)
	{
		int count = 0;
		for (std::string::size_type i = 0; i < s1.size(); ++i)
			if (s1[i] != s2[i])
				++count;

		return count * count;
	}

	struct node_ptr
	{
		int index;    // index of the string
		int distance; // current distance

		explicit node_ptr(int idx): index(idx), distance(INT_MAX) {}
		void operator= (int dist) { distance = dist; }

		bool operator< (node_ptr const& np) const { return distance > np.distance; }
	};


	template <typename T, int sz> inline int size(T (&)[sz]) { return sz; }
	template <typename T, int sz> inline T* begin(T (&array)[sz]) { return array; }
	template <typename T, int sz> inline T* end(T (&array)[sz]) { return array + sz; }

	template <template <typename T> class Heap, typename RandomAccessIt>
	void dijkstra_heap(RandomAccessIt begin, RandomAccessIt end, int start, int destination)
	{
		Heap<node_ptr>                                heap;
		std::vector<typename Heap<node_ptr>::pointer> references;
		std::vector<int>                              distance;
		std::vector<int>                              predecessor;

		for (ptrdiff_t i = 0; i < end - begin; ++i)
		{
			references.push_back(heap.push(node_ptr(i)));
			distance.push_back(INT_MAX);
			predecessor.push_back(INT_MAX);
		}

		heap.change(references[start], 0);
		distance[start] = 0;
		predecessor[start] = 0;

		while (!heap.empty())
		{
			int dist;

			node_ptr np = heap.top();
			heap.pop();

			if (np.index == destination)
				break;

			for (ptrdiff_t i = 0; i < end - begin; ++i)
			{
				dist = node_distance(begin[i], begin[np.index]) + np.distance;
				if (dist < distance[i])
				{
					heap.change(references[i], dist);
					distance[i] = dist;
					predecessor[i] = np.index;
				}
			}
		}

		for(int i = destination; i != start; i = predecessor[i])
			std::cout << begin[i] << " - ";
		std::cout << "";
	}
	*/
protected:
	bool inline CheckNodes()
	{
		if (_source < 0 ) 
		{
			*err_msg = (char *) "Starting vertex not found";
			return false;
		}
		
		if (_target < 0 )
		{
			*err_msg = (char *) "Ending vertex not found";
			return false;
		}

		return true;
	}

	void inline PreProcessing()
	{
		for (std::size_t j = 0; j < _count; ++j)
		{
			edge_descriptor e;
			bool inserted;
			//boost::tie(e, inserted) = add_edge(edges[j].source, edges[j].target, graph);
			boost::tie(e, inserted) = add_edge(edges[j].source - v_min_id, edges[j].target - v_min_id, graph );

			graph[e].cost = edges[j].cost;
			graph[e].id = edges[j].id;

			//if (!directed || (directed && has_reverse_cost))
			if (!directed || has_reverse_cost) //the same but simplified
			{
				//boost::tie(e, inserted) = add_edge(edges[j].target, edges[j].source, graph);
				//graph[e].id = edges[j].id;

				if (has_reverse_cost) 
				{
					if((edges[j].reverse_cost<NOWAY))
					{
						//boost::tie(e, inserted) = add_edge(edges[j].target, edges[j].source, graph);
						boost::tie(e, inserted) = add_edge(edges[j].target - v_min_id, edges[j].source - v_min_id, graph);
						graph[e].id = edges[j].id;
						graph[e].cost = edges[j].reverse_cost;
					}
				}
				else
				{
					//boost::tie(e, inserted) = add_edge(edges[j].target, edges[j].source, graph);
					boost::tie(e, inserted) = add_edge(edges[j].target - v_min_id, edges[j].source - v_min_id, graph);
					graph[e].id = edges[j].id;
					graph[e].cost = edges[j].cost;
				}
			}
		}
		predecessors.resize(num_vertices(graph));
		
		distances.resize(num_vertices(graph));

		//return 0;	
	}

	float8 inline PostProcessing(path_element_t** path, int* path_count)
	{
		int i,j;
		vector<int> path_vect;
		int max = MAX_NODES;
		path_vect.push_back(_target);
		float8 totcost=0.0;

		while (_target != _source) 
		{
			if (_target == predecessors[_target]) 
			{
				*err_msg = (char *) "No path found";
				return -1;
			}
			_target = predecessors[_target];

			path_vect.push_back(_target);
			if (!max--) 
			{
				*err_msg = (char *) "Overflow";
				return -1;
			}
		}
		
		*path = (path_element_t *) malloc(sizeof(path_element_t) * (path_vect.size() + 1));
		if(*path==NULL)
				return -1;

		i = path_vect.size();
		*path_count = i-1;
		for(i--, j = 0; i > 0; i--, j++)
		{
			graph_traits < graph_t >::vertex_descriptor v_src;
			graph_traits < graph_t >::vertex_descriptor v_targ;
			graph_traits < graph_t >::edge_descriptor e;
			graph_traits < graph_t >::out_edge_iterator out_i, out_end;

			(*path)[j].vertex_id = path_vect.at(i) + v_min_id;

			(*path)[j].edge_id = -1;
			totcost += ( (*path)[j].cost = distances[_target] );
			
			v_src = path_vect.at(i);
			v_targ = path_vect.at(i - 1);

			for (boost::tie(out_i, out_end) = out_edges(v_src, graph); 
				out_i != out_end; ++out_i)
			{
				graph_traits < graph_t >::vertex_descriptor v, targ;
				e = *out_i;
				v = source(e, graph);
				targ = target(e, graph);

				if (targ == v_targ)
				{
					//edge_id not need to be restored.
					(*path)[j].edge_id = graph[*out_i].id;// + v_min_id;
					totcost += ( (*path)[j].cost = graph[*out_i].cost );
					break;
				}
			}
		}

		return totcost;
	}

public:
	Boost_Wrapper(edge_t *_edges, unsigned int __count,	bool _directed, bool _has_reverse_cost, int _v_min_id, char **_err_msg)
	{
		_count            = __count;
		directed         = _directed;
		has_reverse_cost = _has_reverse_cost;
		v_min_id		= _v_min_id;
		//CHECKTHIS
		err_msg = _err_msg;

		// FIXME: compute this value
		num_nodes = ((directed && has_reverse_cost ? 2 : 1) * _count) + 100;
		graph_t _graph(num_nodes);
		std::vector<vertex_descriptor> predecessors(num_vertices(graph));
		std::vector<float8> distances(num_vertices(graph));

		graph = _graph;
		edges = _edges;
		

#if defined(BOOST_MSVC) && BOOST_MSVC <= 1300
		// VC++ has trouble with the named parameters mechanism
		property_map<graph_t, edge_weight_t>::type weightmap = get(edge_weight, graph);
#endif
	}

	float8 Dijkstra(const int __source, const int __dest, path_element_t **_path_, int *_path_count)
	{
		PreProcessing();
		_source = vertex(__source, graph);
		
		/* optional */
		//if(CheckNodes() == false)
			//return -1;
		
		// calling Boost function
		dijkstra_shortest_paths(graph, _source,
								predecessor_map(&predecessors[0]).weight_map(get(&Edge_t::cost, graph)).distance_map(&distances[0]));

		_target = vertex(__dest, graph);
		
		return PostProcessing(_path_, _path_count);
	}

	int GraphOverlay(const int* __sources, const int _tot_sources, multipath_t** _C)
	{
		int register i,j,ni;
		float8 c;
		path_element_t* p;
		int pc;
		
		PreProcessing();
		

		(*_C) = (multipath_t*) malloc(sizeof(multipath_t) * _tot_sources * _tot_sources) ;
		if((*_C) == NULL)
			return -1;
		
		for(i=0,ni=0; i<_tot_sources; i++, ni+=_tot_sources)
		{
			_source = vertex(__sources[i] - v_min_id, graph);
			
			dijkstra_shortest_paths(graph, 
									_source,
									predecessor_map(&predecessors[0]).weight_map(get(&Edge_t::cost, graph)).distance_map(&distances[0]));
			
			for(j=0;j<_tot_sources;j++)
			{
				if(i==j)
				{
					(*_C)[ni+j].cost=0.0;
					(*_C)[ni+j].path_count=0;
					(*_C)[ni+j].path=NULL;
					continue;
				}

				//if(!CheckNodes())
				//{
				//	//FreeDistanceMatrix(i,_C);
				//	return -2;
				//}

				_target = vertex(__sources[j] - v_min_id, graph);
				//Post-Processing...
				c = PostProcessing(&p, &pc);
				if(c < 0)
					return -3;

				(*_C)[ni+j].path = p;
				(*_C)[ni+j].path_count = pc;
				(*_C)[ni+j].cost=c;
			}
		}

		return 0;
	}

	/*
	int DikstraHeap()
	{
		
		std::string nodes[] = { "heap", "help", "hold", "cold", "bold", "bolt", "boot" };

		//dijkstra_heap<boost::heap::splay_heap>(begin(nodes), end(nodes), 0, size(nodes) - 1);
		//dijkstra_heap<boost::heap::d_heap>(begin(nodes), end(nodes), 0, size(nodes) - 1);
		//dijkstra_heap<boost::heap::fibonacci_heap>(begin(nodes), end(nodes), 0, size(nodes) - 1);
		//dijkstra_heap<boost::heap::lazy_fibonacci_heap>(begin(nodes), end(nodes), 0, size(nodes) - 1);
		//dijkstra_heap<boost::heap::pairing_heap>(begin(nodes), end(nodes), 0, size(nodes) - 1);
		Heap heap;


		return -1;
	}
	*/
};
