/*******************************************
**** pgIRoute.h							****
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

#define IROUTE_VERSION 0.1
// The number of tuples to fetch from the SPI cursor at each iteration
#define TUPLIMIT 30000

#define CACHE_RESULTS 1  //save the results in the db using aux function
//#define CACHE_DETAILED_RESULTS 1 //save dijkstra paths 

#define ROUTE_NAME_LEN 1024

#define ROUTEPATH_LOG 1  //show the rout_path as a message

#define DISABLED_CODE 1 //for disabling at compile-time some not used code snippets.

//#define NOTICE_MORE_INFO 1

//Table name for caching results.
/*Table Schema:
* source::int4, target::int4, cost::float8, geometry::multilinestring (not used)
*/
//#define DIJKSTRA_TABLE_RESULTS "dijkstra_results"

#include <postgres.h>
#include <executor/spi.h>
#include <funcapi.h>
#include <catalog/pg_type.h>
#include <fmgr.h>
#include "pgIRoute_type.h"

//#undef DEBUG
//#define DEBUG 1

/*
#ifdef DEBUG
#define DBG(format, arg...)                     \
	elog(NOTICE, format , ## arg)
#else
//#define DBG(format, arg) do { ; } while (0)
#define DBG
#endif
*/
#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

/**************************
*** Prototype Function	***	
*** pgIroute.c			***
***************************/
static inline char* text2char(text *in);
static inline int Connect(char* funcname);
static inline int finish( int ret);
static inline int fetch_point_columns(SPITupleTable *tuptable, Point_columns_t  *point_columns, bool has_capacity);
static inline void fetch_point(HeapTuple *tuple, TupleDesc *tupdesc, Point_columns_t *point_columns, Point_t *point, bool has_capacity);
static inline char* ExtractTable(char* sql);
static inline int64 GetTotalTuples(char* table);
static inline int Get_Points_list(char *subsql, Point_t** _points, bool has_capacity);
static inline int fetch_edge_columns(SPITupleTable *tuptable, edge_columns_t *edge_columns, bool has_reverse_cost);
static inline void fetch_edge(HeapTuple *tuple, TupleDesc *tupdesc, edge_columns_t *edge_columns, edge_t *target_edge);

static inline int PrepareEdges(char* sql, bool has_reverse_cost, edge_t** _edges, int* _v_min_id);
static inline int compute_Dijkstra(char* sql, int start_vertex, 
								 int end_vertex, bool directed, 
								 bool has_reverse_cost, 
								 float8 sx, float8 sy, float8 tx, float8 ty,
								 path_element_t **path, int *path_count);
static inline void FreeC(multipath_t** C, int tot_sources);
static inline int compute_MultiDijkstra(char *sql, char* subsql,	bool directed, bool has_reverse_cost, bool wayback, bool overlayroute,
										path_element_t **path, int *path_count, int node0);
/*Fix this one in a better big function */
static inline int compute_VRP(char *sql, char* subsql,	bool directed, bool has_reverse_cost, bool wayback, bool overlayroute,
										path_element_t **path, int *path_count, char* subsql_depot);
										

#ifdef CACHE_RESULTS
static inline int commit_GraphOverlay_result(float8 totcost, Point_t p1, Point_t p2);
#ifdef CACHE_DETAILED_RESULTS 
static inline int commit_GraphOverlayDetails_result(const int4 source, const int4 target, path_element_t *path, int edge_count);
#endif
#endif

static inline int commit_WayPointRoute_result_step(const Point_t source, const Point_t target, const int4 route_path_value, char* name, int vrp_id);
//static inline void commit_WayPointRoute_result(const Point_t *points, const int tot_sources, const int4 *route_path, bool wayback, char* alg, int vrp_id, int vrp_length);
static inline char* SourcesSort(const Point_t* points, const int tot_sources, char* alg);
static inline void commit_WayPointRoute_result(const Point_t *points, const int tot_sources, const int4 *route_path, bool wayback, char* name, int vrp_id, int vrp_length);


#ifdef DISABLE_CODE
static inline float8 RouteCost(path_element_t *path, int path_count);
static inline int commit_dijkstra_results(int start_vertex, int end_vertex, float8 totcost);
static inline int CheckPrecomputedRoute(int start_vertex, int end_vertex, dijkstra_result_t *res);
#endif

#ifdef __cplusplus
extern "C"
#endif
	int inline CopyRouteToPath(const multipath_t *C, const int tot_size, path_element_t **path, int* path_count, const int* RoutePath, bool wayback);

#ifdef __cplusplus
extern "C"
#endif
	int inline CopyOverlayToPath(const multipath_t *C, const int tot_size, path_element_t **path, int* path_count, /*const int* sources,*/ const int* RoutePath, bool wayback);

int inline CopyOverlayVRPToPath(const multipath_t *C, const int tot_size, path_element_t **path, int* path_count, const int** RoutesPath,const int tot_routes, const int* RoutesLength, bool wayback);
int inline CopyRouteVRPToPath(const multipath_t *C, const int tot_size, path_element_t **path, int* path_count, const int** RoutesPath, const int tot_routes, const int* RoutesLength, bool wayback);

static inline void commit_VRPRoute_result(const int n_routes, char *name, float8* route_cost, int* capacities_used);
static inline int commit_VRPRoute_result_step(const int _vrp_id, float8 tour_cost, char* name, int cap_used);

/******************************************
*** Extern Prototype Wrapper Function	***	
*** Boost_Wrapper.cpp					***
******************************************/
#ifdef __cplusplus
extern "C"
#endif
	extern float8 Boost_Dijkstra(edge_t *_edges, unsigned int _count, int _start_vertex, int _end_vertex,
		   bool _directed, bool _has_reverse_cost,
		   path_element_t **_path, int *_path_count, char **_err_msg, int v_min_id);
/*
 * C= graph overlay. (n*n) elements/nodes.
 */
#ifdef __cplusplus
extern "C"
#endif
	extern int Boost_GraphOverlay(edge_t *_edges, unsigned int _count, int* _sources, int _tot_sources,
		   bool _directed, bool _has_reverse_cost,
		   char **_err_msg, multipath_t** C, int v_min_id);


/******************************************
*** Extern Prototype Wrapper Function	***	
*** QuickSort.hpp						***
*******************************************/
#ifdef __cplusplus
extern "C"
#endif
	extern void QuickSortInt(int *A, int p, int r);

#ifdef __cplusplus
extern "C"
#endif
	extern unsigned int* BuildSortedIndexGraphOverlay(multipath_t *A, unsigned int tot_sources);

/******************************************
***	Extern Prototype Wrapper Function	***
*** NearestNeighbour.cpp				***
*******************************************/

#ifdef __cplusplus
extern "C"
#endif
	extern int* NearestNeighbour(const multipath_t* C, const int C_dim, float8 *sum);

#ifdef __cplusplus
extern "C"
#endif
	extern int* NearestNeighbourSorted(const multipath_t* C, const int C_dim, float8 *sum, unsigned int* sort_dist, int node0);


///******************************************
//***	Extern Prototype Wrapper Function	***
//*** 3opt.cpp							***
//*******************************************/
//#ifdef __cplusplus
//extern "C"
//#endif
//	extern float8 Loc3opt(multipath_t *C, int C_dim, int* route_path);

/*******************************************
***	Extern Prototype Wrapper Function	***
*** 3opt.cpp							***
*******************************************/
#ifdef __cplusplus
extern "C"
#endif
	extern double pg3opt(int** route_path, const int route_len, multipath_t* C);

/*******************************************
***	Extern Prototype Wrapper Function	***
*** vrp.cpp								***
*******************************************/
#ifdef __cplusplus
extern "C"
#endif
	//return number of route
	extern int** vrp(multipath_t* _C, /*unsigned int* _C_index,*/ const int _tot_sources, const int _depot, int* _tot_routes, int** _route_length, float8** _route_cost, int* capacity, int** capacity_used, int* depot_cap_used);