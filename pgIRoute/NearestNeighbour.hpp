/*******************************************
**** NeirestNeighbour.hpp				****
**** Code File							****
**** Route Interface for Postgres 9.x	****
**** Features:							****
****   -- Nearest Neighbour				****
****									****
**** Author : Raffaello Bertini			****
*******************************************/
#pragma once
#include <limits>
#include "pgIRoute_type.h"
using ::malloc;
using ::calloc;
using ::free;

int* __NearestNeighbourSorted(const multipath_t* C, const int C_dim, float8 *sum, unsigned int* sort_dist, int node0)
{
	int register i,ni,k,nn;
	bool* inserted;
	int* route_path;

	inserted = (bool*) calloc(C_dim,sizeof(bool));
	if(inserted==NULL)
		return NULL;
	route_path = (int*) malloc(sizeof(int)*C_dim);
	if(route_path==NULL)
	{
		free(inserted);
		return NULL;
	}
	
	route_path[0]=node0; 
	inserted[node0]=true;
	nn=1;
	*sum=0.0;
	while(nn<C_dim)
	{
		i = route_path[nn - 1];
		ni = i*C_dim;

		for(k=1;k<C_dim;k++)
		{
			if(!inserted[sort_dist[ni +k]])
			{
				route_path[nn] = sort_dist[ni + k];
				inserted[route_path[nn]]=true;
				*sum += C[ni + route_path[nn]].cost;
				nn++;
				break;
			}
		}
	}
	
	//from the last to the first...
	*sum += C[route_path[C_dim-1]*C_dim + node0].cost;

	free(inserted);
	return route_path;
}
