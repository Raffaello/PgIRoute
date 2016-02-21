/*******************************************
**** QuickSort.hpp						****
**** Code File							****
**** Route Interface for Postgres 9.x	****
**** Features:							****
****   -- Quick Sort					****
****									****
**** Author : Raffaello Bertini			****
*******************************************/
#pragma once

#include "pgIRoute_type.h"

template<typename T>
inline int Partition(T *A, const int p, const int r)
{
	T register x;
	int register i,j;

	x = A[p];
	i = p-1;
	j = r+1;

	for(;;)
	{
		while(A[--j]>x);
		while(A[++i]<x);
		if(i<j)
		{
			x = A[i];
			A[i] = A[j];
			A[j] = x;
		}

		else return j;
	}
}


template <typename T>
void  QuickSort(T* A, const int p, const int r)
{
	int q;
	if(p<r)
	{
		q = Partition(A,p,r);
		QuickSort(A,p,q);
		QuickSort(A,q+1,r);
	}
}

/* 
 * p,r are the index of the array of indexes of the array A
 */
template<typename T>
inline int Partition_index(T *A, unsigned int* I, const int p, const int r)
{
	T register x;
	int register i,j;

	x = A[I[p]];
	i = I[p-1];
	j = I[r+1];

	for(;;)
	{
		while(A[I[--j]]>x);
		while(A[I[++i]]<x);
		if(i<j)
		{
			unsigned int register t;
			t = I[i];
			I[i] = I[j];
			I[j] = t;
			x= A[I[j]];
		}
		else return j;
	}
}


/* Order the Index using the A value
 * Index init from 0 to n elements
 *
 */
template <typename T>
void  QuickSort_index(T* A, unsigned int* I, const int p, const int r)
{
	int q;
	if(p<r)
	{
		q = Partition_index(A,I,p,r);
		QuickSort_index(A,I,p,q);
		QuickSort_index(A,I,q+1,r);
	}
}

inline int Partition_indexGraphOverlay(multipath_t *A, unsigned int* I, const int p, const int r)
{
	multipath_t x;
	int register i,j;

	x.cost = A[I[p]].cost;
	i = p-1;
	j = r+1;

	for(;;)
	{
		while(A[I[--j]].cost>x.cost);
		while(A[I[++i]].cost<x.cost);
		if(i<j)
		{
			unsigned int register t;
			t = I[i];
			I[i] = I[j];
			I[j] = t;
			x= A[t];
		}
		else return j;
	}
}


/**************************************************************
*** QuickSort.cpp function inserted here					***
*** some conflicts when compiled (maybe is the template)	***
***	.......................................................	***
***															***
***				Wrapper Functions for C compiler			***
***															***
***************************************************************/

#ifdef __cplusplus
extern "C"
	void QuickSortInt(int *A, int p, int r)
{
	QuickSort<int>(A,p,r);
}
#endif

//#ifdef __cplusplus
//extern "C"
//
//	void QuickSortDouble(double *A, int p, int r)
//{
//	QuickSort<double>(A,p,r);
//}
//#endif

//#ifdef __cplusplus
//extern "C"
//	void QuickSortIndexDouble(double *A, unsigned int* I, int p, int r)
//{
//	QuickSort_index<double>(A,I,p,r);
//}
//#endif

/* Order the Index using the A value
 * Index init from 0 to n elements
 *
 */
#ifdef __cplusplus
extern "C"
void  QuickSort_indexGraphOverlay(multipath_t* A, unsigned int* I, const int p, const int r)
{
	int q;
	if(p<r)
	{
		q = Partition_indexGraphOverlay(A,I,p,r);
		QuickSort_indexGraphOverlay(A,I,p,q);
		QuickSort_indexGraphOverlay(A,I,q+1,r);
	}
}
#endif

#ifdef __cplusplus
extern "C"
	unsigned int* BuildSortedIndexGraphOverlay(multipath_t *A, unsigned int tot_sources)
{
	unsigned register i,j,z;
	unsigned int* I;
	z=tot_sources*tot_sources;
	I = (unsigned int*) malloc(sizeof(unsigned int)*z);
	if(I==NULL)
		return NULL;
	
	//init
	for(i=0, z=0; i<tot_sources; i++, z+=tot_sources)
		for(j=0; j<tot_sources; j++)
			I[z+j]=j;
			
	for(i=0, z=tot_sources-1, j=0; i<tot_sources; i++, j+=tot_sources)
		QuickSort_indexGraphOverlay(&A[j],&I[j],0,z);

	return I;
}
#endif

