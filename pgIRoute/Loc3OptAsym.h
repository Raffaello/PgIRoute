/*******************************************
**** Loc3OptAsym.h						****
**** Code File							****
**** Route Interface for Postgres 9.x	****
**** Features:							****
****   -- 3optAsym						****
****									****
**** Author : Raffaello Bertini			****
*******************************************/

#pragma once
#include "pgIRoute_type.h"
#include <climits>

inline void ComputeZ(int* sol, multipath_t*  d, double* zold, double* zorario, double* zantior, int nsol1, int nsol2)
{
	register int js;
	*zold = 0;
	zorario[0] = 0;
	zantior[nsol1] = 0;
	
	for (js = 0; js < nsol1; js++)
	{
		*zold += d[sol[js]*nsol1 + sol[js + 1]].cost;
		zorario[js + 1] = *zold;
		zantior[nsol2 - js] = zantior[nsol1 - js ] + d[sol[nsol1 - js]*nsol1 + sol[nsol2 - js]].cost;
	}

}

inline double Loc3optAsym(int** sol_orig,const int sol_length , multipath_t*  d)
{
	int *solnew; 
	int* sol; 
	double* zorario;   // costi incrementali tour in senso orario
	double* zantior;   // costi incrementali tour in senso antiorario
	int end1a, end2a, end3a, end1b, end2b, end3b;
	double z, znew = INT_MAX, zorg = 0,  zold, cost1, cost2;
	int i, k, n, n1, n2, nk, lk;
	int size_sol;
	int nsol = n = sol_length+1;  // con l'ultimo uguale al primo
	int ind1,ind2,ind3;

	size_sol = sizeof(double)*(nsol);
	zorario = (double*) malloc(size_sol); // costi incrementali tour in senso orario
	zantior = (double*) malloc(size_sol);  // costi incrementali tour in senso antiorario

	size_sol = sizeof(int)*nsol;
	solnew = (int*) malloc(size_sol);
	sol = (int*) malloc(size_sol);
	
	//memcpy(sol,sol_orig,sizeof(int)*sol_length);
	memcpy_s(sol,sizeof(int)*sol_length,*sol_orig,sizeof(int)*sol_length);
	sol[sol_length]=sol[0];

	n2 = nsol - 2;
	n1 = nsol - 1;
	zold = 0;

	// Inizio di una nuova ricerca

	ComputeZ(sol, d, &zold, zorario, zantior, n1, n2);
	zorg = zold;
	//if (nsol <= 5)
	{
		free(zorario);
		free(zantior);
		free(solnew);
		free(sol);
		return zorg;
	}

	goto labStart2;
labStart:
	ComputeZ(sol, d, &zold, zorario, zantior, n1, n2);
labStart2:
	for (ind1 = 1; ind1 < n2; ind1++)                   // 0 ... ind1-1
	{
		end1a = sol[ind1 - 1];
		end1b = sol[ind1];

		for (ind2 = ind1 + 1; ind2 < n1; ind2++)       // ind1 ... ind2-1
		{
			end2a = sol[ind2 - 1];
			end2b = sol[ind2];

			for (ind3 = ind2 + 1; ind3 < nsol; ind3++)  // ind2 ... nsol
			{
				end3a = sol[ind3 - 1];
				end3b = sol[ind3];

				// ------------------------------- primo riattacco:   1a-2a, 1b-3a, 2b-3b

				if (end1b == end2a) goto labSecondo;
				if (end2b == end3a) goto labSecondo;

				cost1 = zorario[n1];
				cost2 = zorario[ind1 - 1] + d[end1a*n1 + end2a].cost +
					(zantior[ind1] - zantior[ind2 - 1]) + d[end1b*n1 + end3a].cost +
					(zantior[ind2] - zantior[ind3 - 1]) + d[end2b*n1 + end3b].cost +
					(zorario[n1] - zorario[ind3]);

				if (cost2 < cost1)
				{
					// Qui ho migliorato
					for (k = 0; k < ind1; k++)
						solnew[k] = sol[k];

					k = ind1;
					for (lk = ind2 - 1; lk >= ind1; lk--)
					{
						solnew[k] = sol[lk];
						k++;
					}
					for (lk = ind3 - 1; lk >= ind2; lk--)
					{
						solnew[k] = sol[lk];
						k++;
					}
					for (k = ind3; k < nsol; k++)
						solnew[k] = sol[k];

					//znew = valutaSol(solnew, sol, nsol);
					memcpy(sol,solnew,size_sol);
					
					//solnew.CopyTo(sol, 0);
					znew = cost2;
					zold = znew;

					goto labStart;
				}

				// provo a riattaccare in senso antiorario
				cost1 = (cost1 < zantior[0] ? cost1 : zantior[0]);
				cost2 = zantior[ind3] + d[end3b*n1 + end2b].cost +
					(zorario[ind3 - 1] - zorario[ind2]) + d[end3a*n1 + end1b].cost +
					(zorario[ind2 - 1] - zorario[ind1]) + d[end2a*n1 + end1a].cost +
					(zantior[0] - zantior[ind1 - 1]);

				if (cost2 < cost1)
				{
					nk = 0;
					for (lk = n1; lk >= ind3; lk--) // n ... ind3 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind2; lk < ind3; lk++)    // ind2 ... ind3-1
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind1; lk < ind2; lk++)    // ind1 ... ind2-1
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind1 - 1; lk >= 0; lk--)    // 0 ... ind1-1
					{
						solnew[nk] = sol[lk];
						nk++;
					}

					//solnew.CopyTo(sol, 0);
					memcpy(sol,solnew,size_sol);
					znew = cost2;
					zold = znew;

					goto labStart;
				}
				// ------------------------------- secondo riattacco: 1a-3a, 2b-1b, 2a-3b

labSecondo: if (end1b == end2a) goto labTerzo;
				if (end3b == end1a) goto labTerzo;
				if (end2b == end3a) goto labTerzo;

				cost1 = zorario[n1];
				cost2 = zorario[ind1 - 1] + d[end1a*n1 + end3a].cost +
					(zantior[ind2] - zantior[ind3 - 1]) + d[end2b*n1 + end1b].cost +
					(zorario[ind2 - 1] - zorario[ind1]) + d[end2a*n1 + end3b].cost +
					(zorario[n1] - zorario[ind3]);

				if (cost2 < cost1)
				{
					// Qui ho migliorato
					for (k = 0; k < ind1; k++)
						solnew[k] = sol[k];

					nk = ind1;
					for (lk = ind3 - 1; lk >= ind2; lk--)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind1; lk < ind2; lk++)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (k = ind3; k < nsol; k++)
					{
						solnew[nk] = sol[k];
						nk++;
					}

					//znew = valutaSol(solnew, sol, nsol);
					//solnew.CopyTo(sol, 0);
					memcpy(sol,solnew,size_sol);

					znew = cost2;
					zold = znew;

					goto labStart;
				}

				// provo a riattaccare in senso antiorario
				cost1 = (cost1 < zantior[0] ? cost1 : zantior[0]);
				cost2 = zantior[ind3] + d[end3b*n1 + end2a].cost +
					(zantior[ind1] - zantior[ind2 - 1]) + d[end1b*n1 + end2b].cost +
					(zorario[ind3 - 1] - zorario[ind2]) + d[end3a*n1 + end1a].cost +
					(zantior[0] - zantior[ind1 - 1]);

				if (cost2 < cost1)
				{
					nk = 0;
					for (lk = n1; lk >= ind3; lk--) // n ... ind3 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind2 - 1; lk >= ind1; lk--)    // ind1 ... ind2-1 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind2; lk < ind3; lk++)    // ind2 ... ind3-1 
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind1 - 1; lk >= 0; lk--)    // 0 ... ind1-1 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}

					//znew = valutaSol(solnew, sol, nsol);
					//solnew.CopyTo(sol, 0);
					memcpy(sol,solnew,size_sol);
					znew = cost2;
					zold = znew;

					goto labStart;
				}

				// ---------------------------------- terzo riattacco:   1a-2b, 3a-2a, 1b-3b

labTerzo: if (end2b == end3a) goto labQuarto;
				if (end1a == end3b) goto labQuarto;
				if (end1b == end2a) goto labQuarto;

				cost1 = zorario[n1];
				cost2 = zorario[ind1 - 1] + d[end1a*n1 + end2b].cost +
					(zorario[ind3 - 1] - zorario[ind2]) + d[end3a*n1 + end2a].cost +
					(zantior[ind1] - zantior[ind2 - 1]) + d[end1b*n1 + end3b].cost +
					(zorario[n1] - zorario[ind3]);

				if (cost2 < cost1)
				{
					// Qui ho migliorato
					for (k = 0; k < ind1; k++)
						solnew[k] = sol[k];

					nk = ind1;
					for (lk = ind2; lk < ind3; lk++)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind2 - 1; lk >= ind1; lk--)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (k = ind3; k < nsol; k++)
					{
						solnew[nk] = sol[k];
						nk++;
					}

					//znew = valutaSol(solnew, sol, nsol);
					//solnew.CopyTo(sol, 0);
					memcpy(sol,solnew,size_sol);
					znew = cost2;
					zold = znew;

					goto labStart;
				}

				// provo a riattaccare in senso antiorario
				cost1 = (cost1 < zantior[0] ? cost1 : zantior[0]);
				cost2 = zantior[ind3] + d[end3b*n1 + end1b].cost +
					(zorario[ind2 - 1] - zorario[ind1]) + d[end2a*n1 + end3a].cost +
					(zantior[ind2] - zantior[ind3 - 1]) + d[end2b*n1 + end1a].cost +
					(zantior[0] - zantior[ind1 - 1]);

				if (cost2 < cost1)
				{
					nk = 0;
					for (lk = nsol - 1; lk >= ind3; lk--) // n ... ind3 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind1; lk < ind2; lk++)    // ind1 ... ind2-1 
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind3 - 1; lk >= ind2; lk--)    // ind3-1 ... ind2 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind1 - 1; lk >= 0; lk--)    // 0 ... ind1-1 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}

					//znew = valutaSol(solnew, sol, nsol);
					//solnew.CopyTo(sol, 0);
					memcpy(sol,solnew,size_sol);
					znew = cost2;
					zold = znew;

					goto labStart;
				}

				// ------------------------------- quarto riattacco: 1a-2b, 3a-1b, 2a-3b

labQuarto: if (end1a == end3b) continue;
				if (end1b == end2a) continue;
				if (end2b == end3a) continue;

				cost1 = zorario[n1];
				cost2 = zorario[ind1 - 1] + d[end1a*n1 + end2b].cost +
					(zorario[ind3 - 1] - zorario[ind2]) + d[end3a*n1 + end1b].cost +
					(zorario[ind2 - 1] - zorario[ind1]) + d[end2a*n1 + end3b].cost +
					(zorario[n1] - zorario[ind3]);

				if (cost2 < cost1)
				{
					// Qui ho migliorato
					for (k = 0; k < ind1; k++)
						solnew[k] = sol[k];

					nk = ind1;
					for (lk = ind2; lk <= ind3 - 1; lk++)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind1; lk <= ind2 - 1; lk++)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (k = ind3; k < nsol; k++)
					{
						solnew[nk] = sol[k];
						nk++;
					}

					//znew = valutaSol(solnew, sol, nsol);
					//solnew.CopyTo(sol, 0);
					memcpy(sol,solnew,size_sol);
					znew = cost2;
					zold = znew;

					goto labStart;
				}

				// provo a riattaccare in senso antiorario
				cost1 = (cost1 < zantior[0] ? cost1 : zantior[0]);
				cost2 = zantior[ind3] + d[end3b*n1 + end2a].cost +
					(zantior[ind1] - zantior[ind2 - 1]) + d[end1b*n1 + end3a].cost +
					(zantior[ind2] - zantior[ind3 - 1]) + d[end2b*n1 + end1a].cost +
					(zantior[0] - zantior[ind1 - 1]);

				if (cost2 < cost1)
				{
					nk = 0;
					for (lk = n1; lk >= ind3; lk--) // n ... ind3 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind2 - 1; lk >= ind1; lk--)    // ind1 ... ind2-1 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind3 - 1; lk >= ind2; lk--)    // ind2 ... ind3-1 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}
					for (lk = ind1 - 1; lk >= 0; lk--)    // 0 ... ind1-1 (reversed)
					{
						solnew[nk] = sol[lk];
						nk++;
					}

					//solnew.CopyTo(sol, 0);
					memcpy(sol,solnew,size_sol);
					znew = cost2;
					zold = znew;

					goto labStart;
				}

			}  // for l3 (inizio)
		}  // for l2 (inizio)
	}  // for l1 (inizio) 
	// write(*,*) ' fine opt3: ldold',ldold

	if (zorg > znew)
	{  
		zorg = znew;
		goto labStart;
	}

	z = 0;
	//sol[n1] = sol[0];
	for (i = 0; i < n1; i++)
		z += d[sol[i]*n1 + sol[i + 1]].cost;

	free(solnew);
	//free(soltemp);
	free(zorario);  // costi incrementali tour in senso orario
	free(zantior);  
	*sol_orig = sol;
	return (znew);
}