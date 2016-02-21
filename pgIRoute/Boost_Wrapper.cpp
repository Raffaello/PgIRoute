/******************************************************
*** Updated for MSVC2010 Compiling					***	
*** Author:	Raffaello Bertini						***
*******************************************************/

#include "Boost_Wrapper.hpp"

#ifdef __cplusplus
extern "C"
#endif
float8 Boost_Dijkstra(edge_t *_edges, unsigned int _count, int _start_vertex, int4 _end_vertex,
		   bool _directed, bool _has_reverse_cost,
		   path_element_t **_path, int *_path_count, char **_err_msg, int v_min_id)
{
	Boost_Wrapper w(_edges, _count,/* _start_vertex, _end_vertex,*/ _directed, _has_reverse_cost, v_min_id, _err_msg);
	
	return w.Dijkstra(_start_vertex, _end_vertex, _path, _path_count);
}

#ifdef __cplusplus
extern "C"
#endif
int Boost_GraphOverlay(edge_t *_edges, unsigned int _count, int* _sources, int _tot_sources,
		   bool _directed, bool _has_reverse_cost,
		   char **_err_msg, multipath_t** C, int v_min_id)
{
	int ret;
	
	Boost_Wrapper w(_edges, _count, _directed, _has_reverse_cost, v_min_id, _err_msg);
	
	ret = w.GraphOverlay(_sources, _tot_sources,C);
	switch (ret)
	{
	case -1: *_err_msg = (char*) "MultiDijkstra: Out of Memory!";
		break;
	case -2: *_err_msg = (char*) "MultiDijkstra: nodes error";
		break;
	case -3: *_err_msg = (char*) "MultiDijkstra: postporcess";
		break;
	case -4: *_err_msg = (char*) "MultiDijkstra: vertex_id";
		break;
	default:
		break;
	}
	
	return 0;
}

