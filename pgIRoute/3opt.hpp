/*******************************************
**** 3opt.hpp							****
**** Code File							****
**** Route Interface for Postgres 9.x	****
**** Features:							****
****   -- 3-opt							****
****									****
**** Author : Raffaello Bertini			****
*******************************************/

#pragma once
//#include <boost/config.hpp>
//#include <boost/intrusive/list.hpp>
#include "pgIRoute_type.h"

#include <list>
using namespace std;


class c3opt
{
private:
	typedef unsigned short int word;
	typedef unsigned char byte;


	typedef list<int> listint_t;
	typedef listint_t::_Nodeptr node_ptr_t;

	typedef struct   //memorizzo i nodi da scambiare col 3opt
	{
		word a,b; //arc1
		word c,d; //arc2
		word e,f; //arc3
		byte swap;  
	} TSWAP;

	float8 EPSILON;

	//float8 *distance;
	const multipath_t *C;
	int C_dim;
	int* route_path;

	float8 gain;
	TSWAP t;
	TSWAP best_swap;

	listint_t list_tour;
	node_ptr_t l1,l1b ,l2,l2b ,l3,l3b;
	node_ptr_t b1,b2,b3;

	inline byte Best3optSwap()
	{
		//tutte le volte ricalcolo come con gain... 2 somme in + ogni volta che chiama la funzione, ma almeno + preciso.
		int ta,tc,te,tb,td;

		ta = t.a * C_dim;
		tb = t.b * C_dim;
		tc = t.c * C_dim;
		td = t.d * C_dim;
		te = t.e * C_dim;
		
		float8 m = C[ta + t.b].cost + C[tc + t.d].cost + C[te + t.f].cost;
		float8 min = m;
		t.swap = 0;

		//t2 
		gain = C[ta + t.c].cost + C[tb + t.e].cost + C[td + t.f].cost;
		if (gain < min)
		{
			t.swap = 1;
			min = gain;
		}
		//t3
		gain = C[ta + t.d].cost + C[te + t.c].cost + C[tb + t.f].cost;
		if (gain < min)
		{
			t.swap = 2;
			min = gain;
		}
		//t8 (se t.c==t.b no swap)
		gain = C[ta + t.c].cost + C[tb + t.d].cost + C[te + t.f].cost;
		if (gain < min)
		{
			min = gain;
			t.swap = 7;
		}
		//t4
		gain = C[ta + t.e].cost + C[td + t.b].cost + C[tc + t.f].cost;
		if (gain < min)
		{
			t.swap = 3;
			min = gain;
		}
		//t5 
		gain = C[ta + t.d].cost + C[te + t.b].cost + C[tc + t.f].cost;
		if (gain < min)
		{
			min = gain;
			t.swap = 4;
		}
		//t6 (se t.e==t.d no swap)
		gain = C[ta + t.b].cost + C[t.c + t.e].cost + C[td + t.f].cost;
		if (gain < min)
		{
			min = gain;
			t.swap = 5;
		}
		//t7 (se t.e==t.b no swap ma impossibile per i loop dei 3 for)
		gain = C[ta + t.e].cost + C[td + t.c].cost + C[tb + t.f].cost;
		if (gain < min)
		{
			min = gain;
			t.swap = 6;
		}

		gain = min - m;
		return t.swap;
	}

	inline void BuildFromTour()
	{
		int i;

		for(i = 0; i < C_dim; i++)
			list_tour.push_back(route_path[i]);

	}

	inline void BuildToTour()
	{
		int i;
		listint_t::const_iterator it;
		
		
		for (i=0, it  =list_tour.cbegin(); i < C_dim; i++, it++)
			route_path[i] = it._Ptr->_Myval;

	}
	
	inline void Reverse(node_ptr_t from,node_ptr_t to)
	{
		node_ptr_t l1, l2, l3, l4, l5, l6;

		l1 = from;
		l2 = to;
		l3 = from->_Next;
		l4 = to->_Prev;

		while (((l3->_Prev != l4) && (l4->_Next != l3)) && (l1 != l2))
		{
			//assign
			l1->_Next = l4;
			l2->_Prev = l3;
			l5 = l4->_Prev;
			l4->_Prev = l1;
			l6 = l3->_Next;
			l3->_Next = l2;
			//shift
			l1 = l4;
			l2 = l3;
			l4 = l5;
			l3 = l6;
		}
	}

	inline void swapcase()
	{

		int etmp;
		node_ptr_t b, d, f, a, c, e;

		a = b1;
		c = b2;
		e = b3;
		b = a->_Next;
		d = c->_Next;
		f = e->_Next;

		switch (best_swap.swap)
		{
		case 1: //t2
			Reverse(a, d);
			//listint_t listtmp(a,d);
			//listtmp.reverse();
			best_swap.b = best_swap.c;
			best_swap.c = b->_Myval;
			Reverse(d->_Prev, f);
			
			best_swap.d = best_swap.e;
			break;
		case 2: //t3
			Reverse(a, d);
			a->_Next = d; d->_Prev = a;
			e->_Next = c; c->_Prev = e;
			b->_Next = f; f->_Prev = b;
			best_swap.b = best_swap.d;
			best_swap.d = best_swap.c;
			best_swap.c = best_swap.e;
			break;
		case 3: // t4
			Reverse(c, f);
			a->_Next = e; e->_Prev = a;
			d->_Next = b; b->_Prev = d;
			c->_Next = f; f->_Prev = c;
			etmp = best_swap.b;
			best_swap.b = best_swap.e;
			best_swap.c = best_swap.d;
			best_swap.d = etmp;
			break;
		case 4: // t5
			a->_Next = d; d->_Prev = a;
			e->_Next = b; b->_Prev = e;
			c->_Next = f; f->_Prev = c;
			etmp = best_swap.b;
			best_swap.b = best_swap.d;
			best_swap.d = etmp;
			best_swap.c = best_swap.e;
			break;
		case 5: //t6
			Reverse(c, f);
			c->_Next = e; e->_Prev = c;
			d->_Next = f; f->_Prev = d;
			best_swap.d = best_swap.e;
			break;
		case 6: //t7
			Reverse(a, f);
			etmp = best_swap.c;
			best_swap.c = best_swap.d;
			best_swap.d = etmp;
			best_swap.b = best_swap.e;
			break;
		case 7: //t8
			Reverse(a, d);
			a->_Next = c; c->_Prev = a;
			b->_Next = d; d->_Prev = b;
			etmp = best_swap.b;
			best_swap.b = best_swap.c;
			best_swap.c = etmp;
			break;
		}

	}

	inline void std3opt()
	{
		bool ret;

		do
		{
			ret = false;
			float8 best = DBL_MAX;

			//cTSP_LIST_NODE b1 = null, b2 = null, b3 = null;

			l1 = list_tour._Myhead;
			l1b = l1->_Prev->_Prev; //n-1

			for (; l1 != l1b; l1 = l1->_Next)
			{
				t.a = l1->_Myval;
				t.b = l1->_Next->_Myval;
				l2b = list_tour._Myhead->_Prev; //n-1
				l3b = list_tour._Myhead;

				for (l2 = l1->_Next; l2 != l2b; l2 = l2->_Next)
				{
					t.c = l2->_Myval;
					t.d = l2->_Next->_Myval;
					if (t.d == t.a)
						break;

					for (l3 = l2->_Next; l3 != l3b; l3 = l3->_Next)
					{
						t.e = l3->_Myval;
						t.f = l3->_Next->_Myval;

						if ((t.swap = Best3optSwap()) > 0.0)
						{
							if (best > gain)
							{
								best = gain;
								best_swap = t;
								b1 = l1;
								b2 = l2;
								b3 = l3;

							}
						}

					}//for

				}//for

			}//for

			if (best < EPSILON)
			{
				ret = true;
				swapcase();
			}
		} while (ret);

		//return ret;
	}//std3opt

public:
	c3opt(int* _route_path, const multipath_t *_C, const int _C_dim)
	{
		EPSILON = -1.e-4;
		C = _C;
		C_dim = _C_dim;
		route_path = _route_path;

		BuildFromTour();
	}

	float8 Do3opt()
	{
		int i,n1;
		float8 sum;

		std3opt();
		
		BuildToTour();

		for(i=0, n1=C_dim-1, sum=0.0; i<n1; i++)
			sum += C[route_path[i]*C_dim + route_path[i+1]].cost;

		sum += C[route_path[n1]*C_dim + route_path[0]].cost;

		return sum;
	}

	~c3opt()
	{
		list_tour.clear();
	}
};

