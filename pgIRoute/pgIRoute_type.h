/*******************************************
**** pgIRoute_type.hpp					****
**** Code File							****
**** Route Interface for Postgres 9.x	****
**** Features:							****
****   -- type data						****
****									****
**** Author : Raffaello Bertini			****
*******************************************/
#pragma once

#include <c.h>

typedef struct //edge 
{
	int id;
	int source;
	int target;
	float8 cost;
	float8 reverse_cost;
} edge_t;

typedef struct //edge_columns 
{
  int id;
  int source;
  int target;
  int cost;
  int reverse_cost;
} edge_columns_t;

typedef struct //path_element 
{
	int vertex_id;
	int edge_id;
	float8 cost;
} path_element_t;

typedef struct //dijkstra_result_column
{
	int source;
	int target;
	int totcost;
} dijkstra_result_column_t;

typedef struct //dijkstra_result
{
	int start_vertex;
	int end_vertex;
	float8 totcost;
} dijkstra_result_t;

/*
 * Struct used for overlay graph, with dijkstra result.
 */
typedef struct //multipath
{
	float8 cost;
	path_element_t* path;
	int path_count;
} multipath_t;

typedef struct //Point_columuns
{
	int source_id;
	int x;
	int y;
	int capacity;
} Point_columns_t;

typedef struct //Point
{
	int4 source_id;
	float8 x;
	float8 y;
	int capacity;
} Point_t;

/*
 * Type of algorithm
 * values range:
 * Initial tour [0..15] 4 bit
 * Loc Search [0..15]   4 bit 
 * tot size 
 * 
 */
//typedef enum algorithm_e {Seq,NN,Seq3opt,NN3opt};
#define ALG_INIT_SEQ 1
#define ALG_INIT_RND 0000 (1)
#define ALG_INIT_NN 2
#define ALG_INIT_CI 3  //cheapest insertion
#define ALG_LOC_NONE 0
#define ALG_LOC_3OPT 1

/*typedef union //algorithm_u
{
	int init:4;
	int loc:4;
}algorithm_u;*/

static inline int Algorithm(/*algorithm_u algo,*/int init, int loc, char* str, int init_node0)
{
	char buf[32];

	switch(/*algo.*/init)
	{
	case ALG_INIT_SEQ :
		strcpy_s(str,256,"Seq");
		break;
	case ALG_INIT_NN:
		strcpy_s(str,256,"NN");
		break;
	case ALG_INIT_CI:
		strcpy_s(str,256,"CI");
		break;
	default:
		return -1;
	}

	_itoa_s(init_node0,buf,32,10);
	strcat_s(str,256,buf);

	switch(/*algo.*/loc)
	{
	case ALG_LOC_NONE:
		break;
	case ALG_LOC_3OPT:
		strcat_s(str,256,"3opt");
		break;
	default:
		return -1;
	}

	return 0;
}

