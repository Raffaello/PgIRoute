/*******************************************
**** VRP.cpp							****
**** Code File							****
**** Route Interface for Postgres 9.x	****
**** Features:							****
****   -- Quick Sort					****
****									****
**** Author : Raffaello Bertini			****
*******************************************/
#include "vrp.hpp"

#ifdef __cplusplus
extern "C"
#endif
	int** vrp(multipath_t* _C, /*unsigned int* _C_index,*/ const int _tot_sources, const int _depot, int* _tot_routes, int** _route_length, float8** _route_cost, int* capacity, int** capacity_used, int* depot_cap_used)
{
	if(_depot<0)
		return NULL;

	int res;

	VRP vrp(_C,/*_C_index,*/_tot_sources,_depot, capacity);
	if((res=vrp.solve())<=-1)
	{
		*_tot_routes=res;
		return NULL;
	}

	//si può fare il 3opt finite le routes...
	
	return  vrp.GetRoutesPath(_tot_routes, _route_length, _route_cost, capacity_used, depot_cap_used);
}