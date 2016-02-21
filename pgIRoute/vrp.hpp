/*******************************************
**** VRP.hpp							****
**** Code File							****
**** Route Interface for Postgres 9.x	****
**** Features:							****
****   -- Quick Sort					****
****									****
**** Author : Raffaello Bertini			****
*******************************************/

#pragma once
//#define TRUCK_SERVING_MAX_CUSTOMERS 10
#define TRUCK_MAX_CAPACITY 300

#include "pgIRoute_type.h"
#include <list>
//#include <iostream>
using namespace std;
//Compute Heuristic Trivial VRP
			//...
			//TODO
			//1. prendere il cliente più lontano da node0 ed inserirlo nel tour
			//2. inserire a costo minore i nodi presenti fino a servirne 10
			//3. iniziare una nuova route dal punto 1 finchè non sono stati serviti tutti i clienti.
			
			//1.
			//C_index[node0 * tot_sources + tot_sources-1]; 
			
			//2.
			//Chepeast Insertion

			//3. New Route until no more customer

//single depot trivial VRP
class VRP
{
private:
	multipath_t* C;
	int* capacity;
	//unsigned int* C_index;
	int tot_nodes;
	int depot;
	int depot_id;
	list<list<int>> routes; //list of routes
	list<int> route;  //single computed route
	list<double>route_cost; //cost single route
	bool *C_used;

	int nodes_remaining;

	list<int>::iterator best_it; //route best insert pos

	list<int> route_capacity_used;

	//if use the candidate list is O(1) instead of O(n)
	inline int GetFarestNodeAvailable(double* cost)
	{
		//return C_index[node0*tot_sources + tot_sources-1];
		//e per i tour successivi?
		int register i;
		float8 best = 0.0;
		int best_id=-1;

		for(i = 0; i < tot_nodes; i++)
		{
			if((!C_used[i])&&
				(C[depot_id + i].cost > best)&&
				(capacity[i]<TRUCK_MAX_CAPACITY))
			{
				best_id=i;
				best=C[depot_id + i].cost;
			}

		}
		*cost=best;
		return best_id;
	}

	inline float8 InsertNode(int* cap_rem)
	{
		int register i;
		int node_id;
		float8 cost, best_cost;
		
		list<int>::iterator best_route_it;

		//route_tmp = route; //backup route
		
		best_cost=DBL_MAX;
		//insert cheapest node

		for(i=0; i < tot_nodes; i++)
		{
			if((C_used[i]) ||
				(*cap_rem < capacity[i]))
				continue;
			
			cost = ComputeBestNodeInsert(i);
			if(cost == DBL_MAX)
				return DBL_MAX;

			if	(cost < best_cost)
			{
				best_cost = cost;
				best_route_it = best_it;
				node_id=i;
			}
		}
		
		if(best_cost<DBL_MAX)
		{
			route.insert(best_route_it,node_id);
			C_used[node_id]=true;
			if(capacity[node_id]>capacity[depot])
			{
				*cap_rem -= capacity[depot];
				capacity[depot] = 0;
			}
			else
			{
				capacity[depot] -= capacity[node_id];
				*cap_rem -= capacity[node_id];
			}
			return best_cost;
		}
		else
			return DBL_MAX;
	}

	inline float8 ChepeastInsertion(int* cap)
	{
		//int rem_n = TRUCK_SERVING_MAX_CUSTOMERS - route.size(); //one is the depot
		int cap_rem = TRUCK_MAX_CAPACITY - *cap;
		double sum=0.0;
		double res;
		//if(nodes_remaining < rem_n)
		//	rem_n=nodes_remaining;

		//while(rem_n>1)
		while(cap_rem < TRUCK_MAX_CAPACITY)
		{
			if((res = InsertNode(&cap_rem))==DBL_MAX) //Error
				//return DBL_MAX;
				break;
			sum += res;
			
			//rem_n--;
		}

		//return InsertNode();
		
		//return capacity used
		*cap = TRUCK_MAX_CAPACITY - cap_rem;
		return sum;

	}

	inline float8 ComputeRouteCost()
	{
		list<int>::iterator it;
		list<int>::iterator it_prev;
		float8 route_cost=0.0;

		it_prev = it = route.begin();
		it++;
		
		for(; it!=route.end(); it++)
		{
			route_cost += C[*it_prev * tot_nodes + *it].cost;
			it_prev= it;
		}
		it = route.begin();
		route_cost += C[*it_prev * tot_nodes + *it].cost;

		return route_cost;
	}

	inline float8 ComputeBestNodeInsert(int node_id)
	{
		list<int>::iterator it;
		float8 best=DBL_MAX;
		float8 cost;
		int best_id=-1;

		best_it=route.end();

		it=route.begin(); //id=0 = depot
		for(it++; it != route.end(); it++) 
		{
			route.insert(it, node_id);
			cost = ComputeRouteCost();
			if(cost < best)
			{
				best_it = it;
				best = cost;
			}
			route.remove(node_id);
		}
		
		//route.insert(it, node_id);
		route.push_back(node_id);
		if((cost = ComputeRouteCost()) < best)
		{
			best_it = it;
			best = cost;
		}
		//route.remove(node_id);
		route.pop_back();
		//if(best<DBL_MAX)
		//	route.insert(best_it,node_id);
		return best;
	}

protected:
public:
	VRP(multipath_t* _C,/* unsigned int* _C_index,*/ const int _tot_sources, const int _depot, int* _capacity)
	{
		C = _C;
		//C_index = _C_index;
		tot_nodes = _tot_sources;
		depot = _depot;
		
		depot_id= depot*_tot_sources;

		nodes_remaining = _tot_sources;

		capacity = _capacity;
	}

	int solve()
	{
		double sum;
		int cap;
		C_used = (bool*) malloc(tot_nodes*sizeof(bool));
		for(int i=0; i < tot_nodes; i++)
			C_used[i]=false;
		C_used[depot]=true;
		nodes_remaining--; //1 is for the depot
	
		while((nodes_remaining>0) && (capacity[depot]>0))
		{
			//sum=0.0;
			//cap=0;
			//init route (2 nodes)
			route.clear();
			route.push_back(depot);
			int node_id = GetFarestNodeAvailable(&sum);
			if(node_id==-1)
				return -1;
			
			cap = capacity[node_id];
			capacity[depot] -= cap;

			route.push_back(node_id);
			C_used[node_id]=true;
			nodes_remaining--;

			if( (sum += ChepeastInsertion(&cap)) == DBL_MAX) //error (no more)
				return -2;
			
			routes.push_back(route);
			route_cost.push_back(sum);
			route_capacity_used.push_back(cap);
			nodes_remaining -= route.size()-2; //-2 = depot & farest customer.
		}
		
		free(C_used);
		route.clear();
		return 0;

	}

	int** GetRoutesPath(int* tot_routes, int** route_length, float8** _route_cost, int** cap_used, int* depot_cap_used)
	{
		*route_length = (int*) malloc(sizeof(int) * routes.size());
		if(*route_length == NULL)
			return NULL;
		int** RoutesPath = (int**) malloc(sizeof(int*)* routes.size());
		if(RoutesPath==NULL)
			return NULL;
		*_route_cost= (float8*) malloc(sizeof(float8)* routes.size());
		if(*_route_cost==NULL)
			return NULL;
		*cap_used = (int*) malloc(sizeof(int) * routes.size());
		if(*cap_used==NULL)
			return NULL;

		list<list<int>>::iterator it;
		list<double>::iterator itd;
		list<int>::iterator itc;
		int i,j;

		for(i=0, it=routes.begin(), itd=route_cost.begin(), itc=route_capacity_used.begin(); it!=routes.end(); it++, i++, itd++, itc++)
		{
			RoutesPath[i] = (int*) malloc(sizeof(int*) * it->size());
			
			if(RoutesPath[i] == NULL) //(postgres free mem.)
			{
				free(route_length);
				while(--i>0)
					free(RoutesPath[i]);
				free(RoutesPath[0]);
				free(RoutesPath);
				return NULL;
			}
			
			(*route_length)[i] = it->size();
			(*_route_cost)[i]  = *itd;
			(*cap_used)[i] = *itc;
			
			list<int>::iterator it2;
			for(j=0, it2 = it->begin(); it2 != it->end(); it2++, j++)
				RoutesPath[i][j] = *it2;
			
			//it->clear(); //needs to destroy object, if no postgres crash.
			//it->~list();
		}
		*tot_routes = routes.size();
		routes.clear();
		route_cost.clear();
		route_capacity_used.clear();
		//routes.~list();
		*depot_cap_used = capacity[depot];
		return RoutesPath;
	}

	~VRP()
	{
		//free(C_used); //crash postgres if free mem here... (also postgres auto free mem)
	}
};