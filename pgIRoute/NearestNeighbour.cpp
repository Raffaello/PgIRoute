/*******************************************
**** NeirestNeighbour.cpp				****
**** Code File							****
**** Route Interface for Postgres 9.x	****
**** Features:							****
****   -- Nearest Neighbour				****
****									****
**** Author : Raffaello Bertini			****
*******************************************/

#include "NearestNeighbour.hpp"

/*notBoosted Trivial NN in O(n^2) (not use candidate set)
* param:	C n*n matrix, 
*			C_dim length of a row/col (length of tour)
*			
* ret: route_path pointer of an array dinamically allocated by the function
*/
#ifdef __cplusplus
extern "C"
#endif
	int* NearestNeighbour(const multipath_t* C, const int C_dim, float8 *sum)
{
	int register i,ni,k,ii;
	bool* inserted;
	int* route_path;
	//double sum=0.0;
	
	inserted = (bool*) calloc(C_dim,sizeof(bool));
	if(inserted==NULL)
		return NULL;
	route_path = (int*) malloc(sizeof(int)*C_dim);
	if(route_path==NULL)
	{
		free(inserted);
		return NULL;
	}
	
	route_path[0]=0; //default starting node.
	inserted[0]=true;
	*sum=0.0;
	for(i = 0, ni = 0, ii = C_dim-1; i < ii; ni += C_dim)
	{
		float8 best = DBL_MAX;
		int best_i=-1;
		ni = route_path[i]*C_dim;
		for(k=0;k<C_dim;k++)
		{
			if(inserted[k])
				continue;

			if(best>C[ni + k].cost)
			{
				best = C[ni + k].cost;
				best_i=k;
			}
		}
		if(best_i!=-1)
		{
			route_path[++i]=best_i;
			*sum += best;
			inserted[best_i]=true;
		}
		else 
			return NULL;
	}
	//from the last to the first...
	*sum += C[route_path[ii]*C_dim + route_path[0]].cost;

	free(inserted);
	return route_path;
}

/*
 * O(n log n)
 * use candidate set sort_dist.
 */
#ifdef __cplusplus
extern "C"
#endif
	int* NearestNeighbourSorted(const multipath_t* C, const int C_dim, float8 *sum, unsigned int* sort_dist, int node0)
{
	float8 current_sum;
	int* best_route_path;
	int* route_path;
	int i;
	int sizeC;

	
	if(node0==-1)
	{
		sizeC=sizeof(int)*C_dim;
		best_route_path = (int*)malloc(sizeC);
		if(best_route_path==NULL)
			return NULL;
		*sum = DBL_MAX;
		for(i=0; i<C_dim; i++)
		{
			route_path = __NearestNeighbourSorted(C,C_dim,&current_sum,sort_dist,i);
			if(route_path==NULL)
			{
				free(best_route_path);
				return NULL;
			}
			if(*sum > current_sum)
			{
				*sum = current_sum;
				memcpy(best_route_path,route_path,sizeC);
			}
		}
		
		return best_route_path;
	}
	else
		return __NearestNeighbourSorted(C,C_dim,sum,sort_dist,node0);
}

