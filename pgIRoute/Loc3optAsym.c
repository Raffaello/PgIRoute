/*******************************************
**** Loc3OptAsym.h						****
**** Code File							****
**** Route Interface for Postgres 9.x	****
**** Features:							****
****   -- 3optAsym						****
****									****
**** Author : Raffaello Bertini			****
*******************************************/

#include "Loc3OptAsym.h"

#ifdef __cplusplus
extern "C"
#endif
	double pg3opt(int** route_path, const int route_len, multipath_t* C)
{
	return Loc3optAsym(route_path, route_len, C);
}