/******************************************************
*** Updated for MSVC2010 (x86) Compiling			***	
*** Author:	Raffaello Bertini						***
******************************************************/
/* ---------------------------------------------------
 * INSPIRED BY:
 * Shortest path algorithm for PostgreSQL (pgRouting)
 */

//TODO: Make better Commit function without wrapper funciton procedure.
//		now they're calling some simply stored function. remove the stored function (pgpsql)
//		and commit data within the dll. (SPI_prepare_params .... and so on)


#include "pgIRoute.h"

static inline char* text2char(text *in)
{
  char *out = (char*) palloc(VARSIZE(in));

  memcpy(out, VARDATA(in), VARSIZE(in) - VARHDRSZ);
  out[VARSIZE(in) - VARHDRSZ] = '\0';
  return out;
}

static inline int Connect(char* funcname)
{
	int SPIcode = SPI_connect();
	if (SPIcode  != SPI_OK_CONNECT)
	{
		elog(ERROR, "%s: couldn't open a connection to SPI (%d)",funcname,SPIcode);
		return -1;
	}

	return SPIcode;
}

static inline int finish( int ret)
{
  int code = SPI_finish();
  if (code  != SPI_OK_FINISH )
  {
	elog(ERROR,"couldn't disconnect from SPI");
	return -1 ;
  }			
  return ret;
}

static inline int fetch_point_columns(SPITupleTable *tuptable, Point_columns_t  *point_columns, bool has_capacity)
{
	//columns source for points (or source_id?)
	point_columns->source_id = SPI_fnumber(SPI_tuptable->tupdesc,"source_id");
	if (point_columns->source_id == SPI_ERROR_NOATTRIBUTE) 
	{
		elog(ERROR,"FetchPoint: Error, query must have columns 'source_id'");
		return -1;
	}
	if (SPI_gettypeid(SPI_tuptable->tupdesc, point_columns->source_id) != INT4OID) 
	{
		elog(ERROR, "FetchPoint: Error, columns returned for 'source' must be of type int4");
		return -1;
	}
	
	//if(overlay)
	//{
		point_columns->x = SPI_fnumber(SPI_tuptable->tupdesc,"x");
		point_columns->y = SPI_fnumber(SPI_tuptable->tupdesc,"y");
		if ((point_columns->x == SPI_ERROR_NOATTRIBUTE) ||
			(point_columns->y == SPI_ERROR_NOATTRIBUTE))
		{
			elog(ERROR,"FetchPoint: Error, query must have also columns 'x', 'y'");
			return -1;
		}

		if ((SPI_gettypeid(SPI_tuptable->tupdesc, point_columns->x) != FLOAT8OID) ||
			(SPI_gettypeid(SPI_tuptable->tupdesc, point_columns->y) != FLOAT8OID))
		{
			elog(ERROR, "FetchPoint: Error, columns 'x', 'y' must be of type float8");
		}
	//}

	if(has_capacity)
	{
		point_columns->capacity = SPI_fnumber(SPI_tuptable->tupdesc,"capacity");
		if(point_columns->capacity == SPI_ERROR_NOATTRIBUTE)
		{
			elog(ERROR,"FetchPoint: Error, query must have also columns 'capacity'");
			return -1;
		}
	}

	return 0;
}

static inline void fetch_point(HeapTuple *tuple, TupleDesc *tupdesc, Point_columns_t *point_columns, Point_t *point, bool has_capacity)
{
	Datum binval;
	bool isnull;

	binval		= SPI_getbinval(*tuple, *tupdesc, point_columns->source_id, &isnull);
	if (isnull)
		elog(ERROR, "Get_Points_list: 'source_id' contains a null value");
	point->source_id = DatumGetInt32(binval);

	//if(overlay)
	//{
		binval = SPI_getbinval(*tuple, *tupdesc, point_columns->x, &isnull);
		if(isnull)
			elog(ERROR, "x contains a null value");
		point->x = DatumGetFloat8(binval);

		binval = SPI_getbinval(*tuple, *tupdesc, point_columns->y, &isnull);
		if(isnull)
			elog(ERROR,"y contains a null value");
		point->y = DatumGetFloat8(binval);
	//}
	//else
	//{
	//	point->x=-1;
	//	point->y=-1;
	//}
	
	if(has_capacity)
	{
		binval = SPI_getbinval(*tuple, *tupdesc, point_columns->capacity, &isnull);
		if(isnull)
			elog(ERROR, "capacity contains a null value");
		point->capacity = DatumGetInt32(binval);
	}

}

static inline char* ExtractTable(char* sql)
{
	char* res;
	int register i1,i2,j1,j2,jc;
	char from[] = "from ";
	int index = -1;
	
	i2 = strlen(sql)-1;
	jc = j2 = strlen(from)-1;
	j1=0;
	i1=7; // "SELECT "
	res = NULL;

	while(i1<i2)
	{
		//FROM 
		if(pg_tolower(sql[i1]) == from[j1])
		{
			if(j1>=j2)
			{
				index=i1+(jc-j1)+1; 
				break;
			}
			j1++;
		}
		else if(j1!=0)
		{ 
			j1=0;
			i1--;
		}
		//MORF
		if(pg_tolower(sql[i2] == from[j2]))
		{
			if(j2<=j1)
			{
				index=i2 +(jc-j2)+1;
				break;
			}
			j2--;
		}
		else if(j2!=jc)
		{
			j2=jc;
			i2++;
		}

		i1++;
		i2--;
	}
	
	if(index!=-1)
	{
		int register size=strlen(sql)-index+1;
		res = (char*) palloc(size);
		snprintf(res,size,"%s",&(sql[index]));
	}

	return res;
}

//*SPI_connect not estabilished **
static inline int64 GetTotalTuples(char* table)
{
	char sql[256];
	//int SPIcode;
	SPIPlanPtr SPIplan;
	Portal SPIportal;
	Datum binval;
	bool isnull;

	SPITupleTable *tuptable;
	TupleDesc tupdesc;
	HeapTuple tuple;
	
	int col;
	int64 rows=0;

	//vedere se c'è una funzione che retituisce direttamente il valore invece di fare la query.
	//sarebbe più veloce ancora.
	strcpy_s(sql,256,"SELECT Count(*) FROM "); //emily_2po_4pgr");
	strcat_s(sql,256,table);

	SPIplan = SPI_prepare(sql, 0, NULL);
	if (SPIplan  == NULL)
	{
		elog(ERROR, "GetTotalTuples: couldn't create query plan via SPI");
		return -1;
	}

	if ((SPIportal = SPI_cursor_open(NULL, SPIplan, NULL, NULL, true)) == NULL) 
		return 0;

	
	SPI_cursor_fetch(SPIportal, TRUE, 1);
	//ntuples		= SPI_processed;
	
	col		= SPI_fnumber(SPI_tuptable->tupdesc, "count");
	if (SPI_gettypeid(SPI_tuptable->tupdesc, col) != INT8OID )
	{
		elog(ERROR, "GetTotalTuples: Error, columns  must be of type int8, ");
		return -1;
	}

	tuptable	= SPI_tuptable;
	tupdesc		= SPI_tuptable->tupdesc;
	tuple		= tuptable->vals[0];
	
	binval = SPI_getbinval(tuple, tupdesc, col, &isnull);
	if (isnull)
	{
		snprintf(sql,256,"count contains a null value");
		elog(ERROR, sql);
	}
	rows = DatumGetInt64(binval);
	
	SPI_freetuptable(tuptable);
	return rows;
}

/*
* SPI_connect() NOT alredy establish
* the query result must have one column.
* RETURN: -1 error, total sources
*  SPIcode 	do not close the connection, else loose allocated memory
*/
static inline int Get_Points_list(char *subsql, /*bool overlay,*/ Point_t** _points, bool has_capacity)
{
	SPIPlanPtr SPIplan;
	Portal SPIportal;
	SPITupleTable *tuptable;
	TupleDesc tupdesc;
	HeapTuple tuple;
	int ntuples;
	int tottuples;
	bool moredata=true;
	Point_t* points;
	Point_columns_t point_columns;
	
	char * tables;
	int64 rows;
	
	tables = ExtractTable(subsql);
	if(tables!=NULL)
	{
		rows = GetTotalTuples(tables);
		pfree(tables);
	}
	else rows=0;

	tottuples=0;
	points=NULL;
	point_columns.source_id=-1;

	if(rows>0)
		points = (Point_t*) palloc( ((Size)rows) * sizeof(Point_t));
	
	SPIplan = SPI_prepare(subsql, 0, NULL);
	if (SPIplan  == NULL)
	{
		elog(ERROR, "Get_Points_list: couldn't create query plan via SPI");
		return -1;
	}

	if ((SPIportal = SPI_cursor_open(NULL, SPIplan, NULL, NULL, true)) == NULL) 
		return -1;
	
	while(moredata)
	{
		SPI_cursor_fetch(SPIportal, TRUE, TUPLIMIT);

		if (point_columns.source_id == -1) 
		{
			if(fetch_point_columns(SPI_tuptable, &point_columns/*,overlay*/,has_capacity) == -1)
				return -1;
		}

		ntuples		= SPI_processed;
		tottuples += ntuples;
		if(rows==0)
		{
			if(points == NULL)
				points = (Point_t*) palloc(tottuples * sizeof(Point_t));
			else
				points = (Point_t*) repalloc(points, tottuples * sizeof(Point_t));
			if(points == NULL)
			{
				elog(ERROR, "Out of memory");
				return -2;
			}
		}

		if(ntuples > 0)
		{
			int register t;
			Point_t *p  = &points[tottuples - ntuples];
			tuptable	= SPI_tuptable;
			tupdesc		= SPI_tuptable->tupdesc;
			
			for(t=0; t<ntuples; t++)
			{
				tuple		= tuptable->vals[t];
				fetch_point(&tuple, &tupdesc, &point_columns, &p[t],has_capacity);
			}
			
			SPI_freetuptable(tuptable);
		}
		else
			moredata = false;
	}

	*_points = points;
	return tottuples;
}

static inline int fetch_edge_columns(SPITupleTable *tuptable, edge_columns_t *edge_columns, bool has_reverse_cost)
{
	edge_columns->id = SPI_fnumber(SPI_tuptable->tupdesc, "id");
	edge_columns->source = SPI_fnumber(SPI_tuptable->tupdesc, "source");
	edge_columns->target = SPI_fnumber(SPI_tuptable->tupdesc, "target");
	edge_columns->cost = SPI_fnumber(SPI_tuptable->tupdesc, "cost");
	if (edge_columns->id == SPI_ERROR_NOATTRIBUTE ||
		edge_columns->source == SPI_ERROR_NOATTRIBUTE ||
		edge_columns->target == SPI_ERROR_NOATTRIBUTE ||
		edge_columns->cost == SPI_ERROR_NOATTRIBUTE) 
	{
		elog(ERROR, "Error, query must return columns 'id', 'source', 'target' and 'cost'");
		return -1;
	}

	if (SPI_gettypeid(SPI_tuptable->tupdesc, edge_columns->source) != INT4OID ||
		SPI_gettypeid(SPI_tuptable->tupdesc, edge_columns->target) != INT4OID ||
		SPI_gettypeid(SPI_tuptable->tupdesc, edge_columns->cost) != FLOAT8OID) 
	{
		elog(ERROR, "Error, columns 'source', 'target' must be of type int4, 'cost' must be of type float8");
		return -1;
	}

	if (has_reverse_cost)
	{
		edge_columns->reverse_cost = SPI_fnumber(SPI_tuptable->tupdesc, 
			"reverse_cost");

		if (edge_columns->reverse_cost == SPI_ERROR_NOATTRIBUTE) 
		{
			elog(ERROR, "Error, reverse_cost is used, but query did't return "
				"'reverse_cost' column");
			return -1;
		}

		if (SPI_gettypeid(SPI_tuptable->tupdesc, edge_columns->reverse_cost) 
			!= FLOAT8OID) 
		{
			elog(ERROR, "Error, columns 'reverse_cost' must be of type float8");
			return -1;
		}
	}

	
  return 0;
}

static inline void fetch_edge(HeapTuple *tuple, TupleDesc *tupdesc, edge_columns_t *edge_columns, edge_t *target_edge)
{
  Datum binval;
  bool isnull;

  binval = SPI_getbinval(*tuple, *tupdesc, edge_columns->id, &isnull);
  if (isnull)
	elog(ERROR, "id contains a null value");
  target_edge->id = DatumGetInt32(binval);

  binval = SPI_getbinval(*tuple, *tupdesc, edge_columns->source, &isnull);
  if (isnull)
	elog(ERROR, "source contains a null value");
  target_edge->source = DatumGetInt32(binval);

  binval = SPI_getbinval(*tuple, *tupdesc, edge_columns->target, &isnull);
  if (isnull)
	elog(ERROR, "target contains a null value");
  target_edge->target = DatumGetInt32(binval);

  binval = SPI_getbinval(*tuple, *tupdesc, edge_columns->cost, &isnull);
  if (isnull)
	elog(ERROR, "cost contains a null value");
  target_edge->cost = DatumGetFloat8(binval);

  if (edge_columns->reverse_cost != -1) 
	{
	  binval = SPI_getbinval(*tuple, *tupdesc, edge_columns->reverse_cost, 
							 &isnull);
	  if (isnull)
		elog(ERROR, "reverse_cost contains a null value");
	  target_edge->reverse_cost =  DatumGetFloat8(binval);
	}
}

#ifndef DISABLED_CODE
static inline float8 RouteCost(path_element_t *path, int path_count)
{
	register int i;
	float8 totcost=0.0;
	
	for(i=0;i<path_count;i++)
		totcost+=path[i].cost;

	return totcost;
}
#endif

/*
* The SPI_connect() alreadyy established.
* return -1 fail, 0 ok
*/
//[NOT USED, FIX THIS]
#ifndef DISABLED_CODE
static inline int commit_dijkstra_results(int start_vertex, int end_vertex, float8 totcost)
{
	int SPIcode;
	char sql[256];
	char buf[20];

	strcpy_s(sql,256,"SELECT insert_dijkstra_result(");
	_itoa_s(start_vertex,buf,20,10);
	strcat_s(sql,256,buf);
	strcat_s(sql,256,",");

	_itoa_s(end_vertex,buf,20,10);
	strcat_s(sql,256,buf);
	strcat_s(sql,256,",");

	snprintf(buf,20,"%.12lf",totcost);
	strcat_s(sql,256,buf);
	strcat_s(sql,256,");");

	SPIcode = SPI_execute(sql,false,1);
	if(SPIcode < 0)
	{
		elog(ERROR, "INSERT RESULT: NOT INSERT RESULT");
		return -1;
	}

	return 0;
}
#endif

#ifdef CACHE_RESULTS
//The SPI_connect() alreadyy established.
static inline int commit_GraphOverlay_result(float8 totcost, Point_t p1, Point_t p2)
{
	int SPIcode;
	char sql[256];

	snprintf(sql,256,"SELECT Insert_GraphOverlay_result(%d,%d,%.12lf,%.12lf,%.12lf,%.12lf,%.12lf);",p1.source_id, p2.source_id, totcost,p1.x,p1.y,p2.x,p2.y);

	//elog(NOTICE,"SQL: %s",sql);
	SPIcode = SPI_execute(sql,false,1);
	if(SPIcode < 0)
	{
		elog(ERROR, "Commit GraphOverlay result: Error!!!");
		return -1;
	}

	return 0;
}
#endif

#ifdef CACHE_RESULTS
#ifdef CACHE_DETAILED_RESULTS
//The SPI_connect() alreadyy established.
//edge_id is the path just ordered!!
static inline int commit_GraphOverlayDetails_result(const int4 source, const int4 target, path_element_t *path, int edge_count, int v_min_id)
{
	int SPIcode;
	char sql[256];
	char *str;
	int sql_i;
	int register i,n;

	snprintf(sql,256,"SELECT Insert_GraphOverlayDetails_result(%d,%d,",source,target);
	sql_i = strlen(sql);
	str = &sql[sql_i];
	for(i=0, n=edge_count; i<n; i++)
	{
		snprintf(str,256,"%d,%d);",path[i].edge_id + v_min_id,i);
		//elog(NOTICE,"SQL: %s",sql);

		SPIcode = SPI_execute(sql,false,1);
		if(SPIcode < 0)
		{
			elog(ERROR, "Commit GraphOverlayDetails result: Error!!!");
			return -1;
		}

	}
	return 0;
}
#endif
#endif

/******************************
 ***	Utils Functions		***
 ******************************/
#ifdef __cplusplus
extern "C"
#endif
	int inline CopyOverlayToPath(const multipath_t *C, const int tot_size, path_element_t **path, int* path_count, const int* RoutePath, bool wayback)
{
	int register i,z1,z2,rc,ni;
	
	z1 = rc = tot_size-1;
	if(wayback)
		z1++;
	*path_count=z1;
	*path = (path_element_t*) malloc(sizeof(path_element_t) * z1);

	if(*path==NULL)
		return -1;

	for(i=0,z2=0; i<rc; i++)
	{
		ni = RoutePath[i]*tot_size + RoutePath[i+1];
		(*path)[i].cost = C[ni].cost;
		(*path)[i].vertex_id = C[ni].path[0].vertex_id; 
		//(*path)[i].vertex_id = sources[i]; 

		//vertex_target_id in edge_id
		(*path)[i].edge_id= C[RoutePath[i+1]*tot_size + RoutePath[i]].path[0].vertex_id;
		//(*path)[i].edge_id= sources[i+1]; 
	}

	if(wayback)
	{
		ni = RoutePath[rc]*tot_size + RoutePath[0];
		(*path)[rc].cost = C[ni].cost;
		(*path)[rc].vertex_id = C[ni].path[0].vertex_id;
		//(*path)[rc].vertex_id =sources[rc];
		(*path)[rc].edge_id= C[RoutePath[0]*tot_size + RoutePath[rc]].path[0].vertex_id;
		//(*path)[rc].edge_id= sources[0];
	}

	return 0;
}

/******************************
 ***	Utils Functions		***
 ******************************/
#ifdef __cplusplus
extern "C"
#endif
	int inline CopyRouteToPath(const multipath_t *C, const int tot_size, path_element_t **path, int* path_count, const int* RoutePath, bool wayback)
{
	int register i,j,z1,z2,rc,ni;
	
	rc = tot_size-1;
	for(i=0,z1=0; i<rc; i++)
		z1 += C[RoutePath[i]*tot_size + RoutePath[i+1]].path_count;
	if(wayback)
		z1 += C[RoutePath[rc]*tot_size + RoutePath[0]].path_count;

	*path_count=z1;
	*path = (path_element_t*) malloc(sizeof(path_element_t) * z1);

	if(*path==NULL)
		return -1;

	for(i=0,z2=0; i<rc; i++)
	{
		ni = RoutePath[i]*tot_size + RoutePath[i+1];
		z1=C[ni].path_count;
		
		for(j=0; j< z1;j++)
		{
			(*path)[z2] = C[ni].path[j];
			(*path)[z2++].vertex_id = C[ni].path[j].vertex_id; 
		}
	}

	if(wayback)
	{
		ni = RoutePath[rc]*tot_size + RoutePath[0];
		z1= C[ni].path_count;
		for(j=0; j<z1;j++)
		{
			(*path)[z2] = C[ni].path[j];
			(*path)[z2++].vertex_id = C[ni].path[j].vertex_id; 
		}
	}

	return 0;
}


int inline CopyOverlayVRPToPath(const multipath_t *C, const int tot_size, path_element_t **path, int* path_count, const int** RoutesPath,const int tot_routes, const int* RoutesLength, bool wayback)
{
	
	int register ii,i,z1,rc,ni;
	
	z1= tot_size-1;
	if(wayback)
		z1 += tot_routes;

	*path_count=z1;
	*path = (path_element_t*) malloc(sizeof(path_element_t) * (z1)); 

	if(*path==NULL)
		return -1;

	for(ii=0,z1=0;ii<tot_routes; ii++)
	{
		rc=RoutesLength[ii]-1;
		for(i=0; i<rc; i++)
		{
			ni = RoutesPath[ii][i]*tot_size + RoutesPath[ii][i+1];
			(*path)[z1].cost = C[ni].cost;
			(*path)[z1].vertex_id = C[ni].path[0].vertex_id; 
			(*path)[z1++].edge_id= C[RoutesPath[ii][i+1]*tot_size + RoutesPath[ii][i]].path[0].vertex_id;
		}
		if(wayback)
		{
			ni = RoutesPath[ii][rc]*tot_size + RoutesPath[ii][0];
			(*path)[z1].cost = C[ni].cost;
			(*path)[z1].vertex_id = C[ni].path[0].vertex_id;
			(*path)[z1++].edge_id= C[RoutesPath[ii][0]*tot_size + RoutesPath[ii][rc]].path[0].vertex_id;
		}
	}
	
	return 0;
}

int inline CopyRouteVRPToPath(const multipath_t *C, const int tot_size, path_element_t **path, int* path_count, const int** RoutesPath, const int tot_routes, const int* RoutesLength, bool wayback)
{
	int register ii,i,j,z1,z2,rc,ni;
	
	//rc = tot_size-1;
	for(ii=0,z1=0; ii<tot_routes;ii++)
	{
		for(i=0, rc=RoutesLength[ii]-1; i < rc; i++)
			z1 += C[RoutesPath[ii][i]*tot_size + RoutesPath[ii][i+1]].path_count;
		if(wayback)
			z1 += C[RoutesPath[ii][rc]*tot_size + RoutesPath[ii][0]].path_count;
	}
	
	*path = (path_element_t*) malloc(sizeof(path_element_t) * z1);
	if(*path==NULL)
		return -1;
	*path_count=z1;

	for(ii=0,z2=0; ii<tot_routes; ii++)
	{
		for(i=0, rc=RoutesLength[ii]-1; i<rc; i++)
		{
			ni = RoutesPath[ii][i]*tot_size + RoutesPath[ii][i+1];
			z1=C[ni].path_count;
			for(j=0; j< z1;j++)
			{
				(*path)[z2] = C[ni].path[j];
				(*path)[z2++].vertex_id = C[ni].path[j].vertex_id; 
			}
		}

		if(wayback)
		{
			ni = RoutesPath[ii][rc]*tot_size + RoutesPath[ii][0];
			z1= C[ni].path_count;
			for(j=0; j<z1;j++)
			{
				(*path)[z2] = C[ni].path[j];
				(*path)[z2++].vertex_id = C[ni].path[j].vertex_id; 
			}
		}
	}

	return 0;
}


//The SPI_connect() alreadyy established.
//static inline int commit_WayPointRoute_result_step(const int4 source, const int4 target, const int4 route_path_value, char* name)
static inline int commit_WayPointRoute_result_step(const Point_t source, const Point_t target, const int4 route_path_value, char* name, int vrp_id)
{
	int SPIcode;
	char sql[ROUTE_NAME_LEN+256];
	
	//snprintf(sql,256,"SELECT Insert_WayPointRoute_result(%d,%d,%d);", source, target, route_path_value);
	
	//snprintf(sql,256,"SELECT Insert_WayPointRoute_result(%d,%d,%d,%s);", source, target, route_path_value,name);
	//elog(NOTICE,"source_id=%d, target_id=%d, route_path_val=%d, sx=%f, sy=%f, tx=%f, ty=%f, vrp_id=%d", source.source_id, target.source_id, route_path_value, source.x,source.y, target.x, target.y,vrp_id);
	//elog(NOTICE,"...");
	snprintf(sql,ROUTE_NAME_LEN+256,"SELECT Insert_WayPointRoute_result(%d,%d,%d,%s,%lf,%lf,%lf,%lf,%d);", source.source_id, target.source_id, route_path_value,name, source.x,source.y, target.x, target.y,vrp_id);
	
	//elog(NOTICE,"STEP: ");
	SPIcode = SPI_execute(sql,false,0);
	//elog(NOTICE, "###################    SPI CODE = %d ###################################",SPIcode);
	if(SPIcode < 0)
	{
		elog(ERROR, "CommitWayPointRoute result: Error!!!");
		return -1;
	}

	return 0;
}

static inline char* SourcesSort(const Point_t* points, const int tot_sources, char* alg)
{
	char route_set[ROUTE_NAME_LEN];
	char* name;
	char buf[16];
	int *sorted_sources;
	int register i;

	//1. sort the sources set.
	sorted_sources = (int*)palloc(sizeof(int)*tot_sources);
	if(sorted_sources==NULL)
		elog(ERROR,"Out of Memory!");

	for(i=0; i<tot_sources; i++)
		sorted_sources[i] = points[i].source_id;
	
	QuickSortInt(sorted_sources,0,tot_sources-1);
	
	//2. make unique ID for the solution.
	route_set[0]='\0';
	for(i=0;i<tot_sources;i++)
	{
		_itoa_s(sorted_sources[i],buf,16,10);
		strcat_s(route_set,ROUTE_NAME_LEN,buf);
	}
	name = (char*) palloc(sizeof(char)*ROUTE_NAME_LEN+64);
	snprintf(name,ROUTE_NAME_LEN+64,"\'Route Solution of %d sources using %s - RouteSet{%s}.\'",tot_sources, alg,route_set);
	pfree(sorted_sources);
#ifdef ROUTEPATH_LOG
	//elog(NOTICE,"(%d) ROUTE SET: %s",strlen(route_set),route_set);
	elog(INFO,"NAME: %s",name);
#endif
	return name;

}

static inline void commit_WayPointRoute_result(const Point_t *points, const int tot_sources, const int4 *route_path, bool wayback, char* name, int vrp_id, int vrp_length)
{
	int register i,j;
	//*** COMMIT WayPoint Route ***

	//3. insert the solution (tot_sources row)  into the table
	if(vrp_id!=-1)
		j=vrp_length-1;
	else
		j=tot_sources-1;

	//elog(NOTICE,"vrpo_id=%d -- j=%d --- tot_sources=%d --- vrp_length=%d",vrp_id, j, tot_sources, vrp_length);

	for(i=0/*,j=tot_sources-1*/; i<j; i++)
	{
		//elog(NOTICE,"route %d=%d --- points[rp[i]]=%d -- ",i,route_path[i],points[route_path[i]].source_id);
		if(commit_WayPointRoute_result_step(points[route_path[i]]/*.source_id*/, 
											points[route_path[i+1]]/*.source_id*/,
											i,name,vrp_id) == -1)
			elog(WARNING,"Commit geo-result error!!! (WayPointRoute)");
		
	}
	//elog(NOTICE,"route %d=%d --- points[i]=%d",i,route_path[i],points[route_path[i]].source_id);
	if(wayback) //route_path_id == -1
	{
		//elog(NOTICE,"route %d=%d --- points[i]=%d",0,route_path[0],points[route_path[0]].source_id);
		
		if(commit_WayPointRoute_result_step(points[route_path[i]]/*.source_id*/, 
											points[route_path[0]]/*.source_id*/,
											-1,name,vrp_id) == -1)
				elog(WARNING,"Commit geo-result error!!! (WayPointRoute)");
	}
}

//Call after the commit_WayPointRoute.
static inline int commit_VRPRoute_result_step(const int _vrp_id, float8 tour_cost, char* name, int cap_used)
{
	int SPIcode;
	char sql[ROUTE_NAME_LEN+256];

	snprintf(sql, ROUTE_NAME_LEN+256, "SELECT Insert_VRPRoute_result(%d,%lf,%d,%s);",_vrp_id, tour_cost, cap_used, name);
	SPIcode = SPI_execute(sql,false,0);
	if(SPIcode < 0)
	{
		elog(ERROR, "CommitVRPRoute result: Error!!!");
		return -1;
	}

	return 0;
}

static inline void commit_VRPRoute_result(const int n_routes, char *name, float8* route_cost, int* capacities_used)
{
	int i;

	for(i=0; i<n_routes; i++)
		if(commit_VRPRoute_result_step(i,route_cost[i],name,capacities_used[i])==-1)
			return;
}

/*
 * The SPI_connect() already established.
 * create and close.
 * return :	 0 have to compute the route
 *			 1 already computed.
 *			-1 error.
 *
 * [NOT USED! FIX THIS]
 */
#ifndef DISABLED_CODE
#ifdef CACHE_RESULTS
static inline int CheckPrecomputedRoute(int start_vertex, 
								 int end_vertex, dijkstra_result_t *res)
{
	char sql[256];
	char buf[20];
	//int SPIcode;
	SPIPlanPtr SPIplan;
	Portal SPIportal;
	dijkstra_result_column_t col;
	Datum binval;
	bool isnull;
	SPITupleTable *tuptable;
	TupleDesc tupdesc;
	HeapTuple tuple;
	//int ntuples;

	strcpy_s(sql,256,"SELECT * FROM get_dijkstra_result(");
	_itoa_s(start_vertex,buf,20,10);
	strcat_s(sql,256,buf);

	strcat_s(sql,256,",");
	_itoa_s(end_vertex,buf,20,10);
	strcat_s(sql,256,buf);
	strcat_s(sql,256,");");

	//elog(NOTICE,"SQL: %s",sql);

	SPIplan = SPI_prepare(sql, 0, NULL);
	if (SPIplan  == NULL)
	{
		elog(ERROR, "CheckPrecomputedRoute: couldn't create query plan via SPI");
		return -1;
	}

	if ((SPIportal = SPI_cursor_open(NULL, SPIplan, NULL, NULL, true)) == NULL) 
		return -1;

	
	SPI_cursor_fetch(SPIportal, TRUE, 1);
	//ntuples		= SPI_processed;
	
	col.source		= SPI_fnumber(SPI_tuptable->tupdesc, "start_vertex");
	col.target		= SPI_fnumber(SPI_tuptable->tupdesc, "end_vertex");
	col.totcost		= SPI_fnumber(SPI_tuptable->tupdesc, "totcost");
	if (col.source == SPI_ERROR_NOATTRIBUTE ||
		col.target == SPI_ERROR_NOATTRIBUTE ||
		col.totcost == SPI_ERROR_NOATTRIBUTE) 
	{
		elog(ERROR,"CheckPrecomputedRoute: Error, query must return columns 'start_vertex', 'end_vertex' and 'totcost' %d %d %d",col.source, col.target, col.totcost);
		return -1;
	}

	if (SPI_gettypeid(SPI_tuptable->tupdesc, col.source) != INT4OID ||
	  SPI_gettypeid(SPI_tuptable->tupdesc, col.target) != INT4OID ||
	  SPI_gettypeid(SPI_tuptable->tupdesc, col.totcost) != FLOAT8OID) 
	{
		elog(ERROR, "CheckPrecomputedRoute: Error, columns 'start_vertex', 'end_vertex' must be of type int4, 'totcost' must be of type float8");
		return -1;
	}

	tuptable	= SPI_tuptable;
	tupdesc		= SPI_tuptable->tupdesc;
	tuple		= tuptable->vals[0];
	
	binval = SPI_getbinval(tuple, tupdesc, col.source, &isnull);
	if (isnull)
		return 0;
	res->start_vertex = DatumGetInt32(binval);
	
	binval = SPI_getbinval(tuple, tupdesc, col.target, &isnull);
	if (isnull)
		return -1;
	res->end_vertex = DatumGetInt32(binval);

	binval = SPI_getbinval(tuple, tupdesc, col.totcost, &isnull);
	if (isnull)
		return -1;
	res->totcost = DatumGetFloat8(binval);

	SPI_freetuptable(tuptable);
	
	return 1;
}
#endif
#endif

static inline int PrepareEdges(char* sql, bool has_reverse_cost, 
								 edge_t** _edges, int* _v_min_id)
{
	SPIPlanPtr SPIplan;
	Portal SPIportal;
	bool moredata = TRUE;
	int ntuples;
	int total_tuples;
	edge_t *edges;

	int64 rows;
	char* tables;
	int ret;
	int v_min_id;
	register int z,z1,z2;

#ifdef __GCC__
	edge_columns_t edge_columns = {id: -1, source: -1, target: -1, 
cost: -1, reverse_cost: -1};
#else
	edge_columns_t edge_columns = {-1, -1, -1, -1, -1};
#endif

	v_min_id=INT_MAX;

	ret = -2;

	edges = NULL;
	total_tuples = 0;
	
	tables = ExtractTable(sql);
	if(tables!= NULL)
	{
		rows = GetTotalTuples(tables);
		pfree(tables);
	}
	else
		rows=0;

	if(rows>0)
		edges  = (edge_t*)  palloc( ((Size)rows) * sizeof(edge_t));

	SPIplan = SPI_prepare(sql, 0, NULL);
	if (SPIplan  == NULL)
	{
		elog(ERROR, "Dijkstra: couldn't create query plan via SPI");
		return -1;
	}

	if ((SPIportal = SPI_cursor_open(NULL, SPIplan, NULL, NULL, true)) == NULL) 
	{
		elog(ERROR, "Dijkstra: SPI_cursor_open('%s') returns NULL", sql);
		return -1;
	}

	while (moredata == TRUE)
	{
		SPI_cursor_fetch(SPIportal, TRUE, TUPLIMIT);

		if (edge_columns.id == -1) 
		{
			if (fetch_edge_columns(SPI_tuptable, &edge_columns, 
				has_reverse_cost) == -1)
				return ret;
		}

		ntuples = SPI_processed;
		total_tuples += ntuples;
		if(rows==0)
		{
			if (!edges)
				edges = (edge_t*) palloc(total_tuples * sizeof(edge_t));
			else 
				edges = (edge_t*) repalloc(edges, total_tuples * sizeof(edge_t));

			if (edges == NULL) 
			{
				elog(ERROR, "Out of memory");
				return ret;	  
			}

		}
		
		if (ntuples > 0) 
		{
			int register t;
			SPITupleTable *tuptable = SPI_tuptable;
			TupleDesc tupdesc = SPI_tuptable->tupdesc;
			edge_t* e = &edges[total_tuples - ntuples];
			
			for (t = 0; t < ntuples; t++) 
			{
				HeapTuple tuple = tuptable->vals[t];

				fetch_edge(&tuple, &tupdesc, &edge_columns, 
					&e[t]);

				if(e[t].source<v_min_id)
					v_min_id=e[t].source;

				if(e[t].target<v_min_id)
					v_min_id=e[t].target;
			}
			
			SPI_freetuptable(tuptable);
		} 
		else 
			moredata = FALSE;
	}

	//::::::::::::::::::::::::::::::::::::  
	//:: reducing vertex id (renumbering)
	//::::::::::::::::::::::::::::::::::::
	//moved inside the graph building (Boost_Graph Class: Boost_Wrapper.hpp)
	//for(z=0, z1=v_min_id, z2=total_tuples; z<z2 /*total_tuples*/; z++)
	//{
	//	edges[z].source-=z1;
	//	edges[z].target-=z1;
	//}

	*_edges		= edges;
	*_v_min_id	= v_min_id;

	return total_tuples;
}

/*****************************************************************
 ***					CORE FUNCTION							***
 *** ---------------------------------------------------------- ***
 *** compute_Dijkstra											***
 *** input:														***
 ***	sql				: query string to fetch data			***
 ***	start_vertex	: Starting Node							***
 ***	end_vertex		: Target Node							***
 ***	directed		: directed graph?						***
 ***	has_reverse_cost: reverse cost (j,i) from (i,j)			***
 ***	sx, sy			: Coordinate Point of Starting Node		***
 ***	tx, ty			: Coordinate Point of Target Node		***
 *** output:													***
 ***	*path			: Array of path_element_t (solution)	***
 ***	path_count		: length of *path						***
 *** return:													***
 ***	= 0				: OK									***
 ***	< 0				: ERRROR								***
 ******************************************************************
 */
static inline int compute_Dijkstra(char* sql, int start_vertex, 
								 int end_vertex, bool directed, 
								 bool has_reverse_cost, 
								 float8 sx, float8 sy, float8 tx, float8 ty,
								 path_element_t **path, int *path_count) 
{
	edge_t *edges;
	int v_min_id;
	char *err_msg;
	int ret, total_tuples;
	//register int z,z1,z2;
	float8 totcost;
	
#ifdef CACHE_RESULTS	
	Point_t p1,p2;
	//dijkstra_result_t res;
#endif

	ret = Connect("Dijkstra");
#ifdef CACHE_RESULTS
	ret=0;
#ifdef CACHE_DETAILED_RESULTS
	//check if the route was precomputed...
	//FIX: if it was precomputed, need the 'SET OF path_results" --> all edge_id for returning the detailed path.
	// not neccessary for the overlay.
	//FIX: make it better...
	//ret = CheckPrecomputedRoute(start_vertex, end_vertex, &res);
	ret=0; //...XIF
#endif
#else
	ret=0;
#endif


	switch (ret)
	{
		case 0:
			if ((total_tuples = PrepareEdges(sql, has_reverse_cost, &edges, &v_min_id)) < 2)
				return -1;

			totcost = Boost_Dijkstra(edges, total_tuples, start_vertex, end_vertex,
								directed, has_reverse_cost,
								path, path_count, &err_msg, v_min_id);

			if (totcost < 0)
			{
				ereport(ERROR, (errcode(ERRCODE_E_R_E_CONTAINING_SQL_NOT_PERMITTED), 
					errmsg("Error computing path: %s", err_msg)));
			} 
			
			elog(INFO,"Dijkstra Total Cost = %f",totcost);

#ifdef CACHE_RESULTS 
			p1.source_id=start_vertex;	p1.x=sx;	p1.y=sy;
			p2.source_id=end_vertex;	p2.x=tx;	p2.y=ty;

			//Commit 1 arc of GraphOverlay (the one just computed)
			if(commit_GraphOverlay_result(totcost,p1,p2) == -1)
				elog(WARNING,"Dijkstra: Commit geo-result error !!! (GraphOverlay)");
#ifdef CACHE_DETAILED_RESULTS
			//Commit the detailed Route for the Overlay Arc
			if(commit_GraphOverlayDetails_result(start_vertex, end_vertex, *path ,*path_count) == -1)
				elog(WARNING,"Dijkstra: Commit geo-result error !!! (GraphOverlayDetails)");
#endif
#endif
			return finish((totcost>=0.0)? 0: -1);
		break; //case 0
		case 1: //FIX THIS.
				elog(ERROR,"Already Computed!, come fare a ritornare il path che non è memorizzato? :)");
		break; //case 1
		case -1:
		default:
				elog(ERROR,"ERROR in get_dijkstra result!");
			break;
	}

	//return -1;
	return finish(-1);
}

/*
 * Free Allocated Memory for GraphOverlay
 */
static inline void FreeC(multipath_t** C, int tot_sources)
{
	int register z1,z;

	for(z1=0,z = tot_sources*tot_sources; z1 < z; z1++)
		free((*C)[z1].path);
	free((*C));
}


/*****************************************************************
 ***					CORE FUNCTION							***
 *** ---------------------------------------------------------- ***
 *** compute_MultiDijkstra										***
 *** input:														***
 ***	sql				: query string to fetch data			***
 ***	subsql			: query string to fetch sources			***
 ***						(source_id, x, y)					***
 ***	directed		: directed graph?						***
 ***	has_reverse_cost: reverse cost (j,i) from (i,j)			***
 ***	wayback			: Compute the wayback path?				***
 ***	overlayroute	: result based on Overlay Graph			***
 *** ----------------------------------------------------------	***
 *** |						NB:								  |	***
 *** |	if(overlayroute) ==->	edge_id AS target			  |	***
 *** ----------------------------------------------------------	***
 *** output:													***
 ***	*path			: Array of path_element_t (solution)	***
 ***															***
 ***	path_count		: length of *path						***
 *** return:													***
 ***	= 0				: OK									***
 ***	< 0				: ERRROR								***
 ******************************************************************
 */
static inline int compute_MultiDijkstra(char *sql, char* subsql,	bool directed, bool has_reverse_cost, bool wayback, bool overlayroute,
										path_element_t **path, int *path_count, int node0)
{
	int tot_sources;
	int ret;
	multipath_t* C;
	Point_t* points;
	edge_t *edges;
	int v_min_id;
	char *err_msg;
	int total_tuples;
	register int z;
	unsigned int* C_index;
	register int i;
#ifdef CACHE_RESULTS
	register int j;
#ifdef CACHE_DETAILED_RESULTS
	register int k;
#endif
#endif
	int* sources=NULL;
	int* route_path;
	float8 tot_cost;
	char algo_str[256];
	char* name;
	//algorithm_u algo;

	ret = Connect("MultiDijkstra");

	tot_sources = Get_Points_list(subsql, &points, false);
	
	//FIX THIS (sources isn't necessary)
	//but usefull for translate source_id starting from 0 index value
	sources = (int*) palloc(sizeof(int)*tot_sources);
	for(z=0; z < tot_sources; z++)
		sources[z] = points[z].source_id;
	//-----
	
	if((sources==NULL)||(tot_sources==0))
		elog(ERROR,"MultiDijkstra: no sources!!!");
	
#ifdef CACHE_RESULTS	
#ifdef CACHE_DETAILED_RESULTS
	/*************************************************************/
	//FIX: CACHED Result of MUltidijkstra. NOT IMPLEMENTED (NOT COMPLETED)
	//check if the route was precomputed...
	//ret = CheckPrecomputedRoute(start_vertex, end_vertex, &res);
	ret=0;
#else //CACHE_RESULTS
	//leggere i valori già presenti nella tabella Graph_overlay 
	//per i valori di sources...
	//quelli non presenti settare il cost = -1
	//così dopo si sa che si deve calcolare il percorso dei nodi in cui il costo è -1
	//se dovessero mancare dei nodi...
	ret=0;
#endif
#else
	ret=0;
#endif
	
	switch (ret)
	{
		case 0:
			if ((total_tuples = PrepareEdges(sql, has_reverse_cost, &edges, &v_min_id)) < 2)
				return -1;
			
			//translate the source_id to 0 based array.
			//don't used anymore...
			//doing in the boost_Graph class
			//for(z=0; z<tot_sources;z++)
			//	sources[z]-=v_min_id;

			//Multiple Dijkstra Calling!!!
			//Return Graph_Overlay in C
			C=NULL;
			ret = Boost_GraphOverlay(edges, total_tuples, sources, tot_sources,
										directed, has_reverse_cost,
										&err_msg, &C, v_min_id);
			
			if ((ret < 0)||((C)==NULL))
			{
				ereport(ERROR, (errcode(ERRCODE_E_R_E_CONTAINING_SQL_NOT_PERMITTED), 
					errmsg("Error computing GraphOverlay: %s", err_msg)));
			}
			
			// using graph overlay
			//TODO: fare uno swith sulla variabile algorithm o simile.
			//aggiungere quindi un parametro di tipo int e poi usare una enum
			//in concomitanza con postgres.

			//Ordering The Overlay Graph
			C_index = BuildSortedIndexGraphOverlay(C,tot_sources);
			if(C_index==NULL)
				elog(ERROR,"Out of Memory!");

			route_path = NearestNeighbourSorted(C,tot_sources,&tot_cost,C_index,node0);
			if(route_path==NULL)
				elog(ERROR,"NN error: NULL route_path");
			elog(INFO,"NN node0=%d tot_cost = %f",node0,tot_cost);

			//tot_cost = Loc3opt(C,tot_sources,route_path);
			//elog(WARNING,"3opt = %f",tot_cost);
			//3opt
			tot_cost = pg3opt(&route_path,tot_sources,C);
			elog(INFO,"3opt Asym = %f",tot_cost);

			//::::::::::::::::::::::::::::::::
			//:: restoring original vertex id
			//:: need for path variable
			//:: not here...
			//::::::::::::::::::::::::::::::::
			//for(z=0; z<tot_sources;z++)
			//		sources[z]+=v_min_id;


#ifdef CACHE_RESULTS
			//COMMIT the Graph Overlay 
			for(i=0,z=tot_sources; i<z; i++)
			{
				int register index= i*tot_sources;
				for(j=0; j<tot_sources; j++, index++)
				{
					if(i==j)
						continue;
//#ifdef CACHE_RESULTS					
					if(commit_GraphOverlay_result(C[index].cost,points[i],points[j]) == -1)
						elog(WARNING,"MultiDijkstra: Commit geo-result error!!! (GraphOveraly)");
					
#ifdef CACHE_DETAILED_RESULTS
					//for(k=1;k<C[index].path_count;k++)
					//{
					//	da fare!!!
					//}
					//COMMIT THE Graph Overlay Details
					if(commit_GraphOverlayDetails_result(points[i].source_id, points[j].source_id, C[index].path, C[index].path_count, v_min_id) == -1)
						elog(WARNING,"MultiDijkstra: Commit geo-result error!!! (GraphOveraly)");
#endif
				}

			}
#endif			
			
			//FIX This Function: id is already computed. return the cached result...
			 //if(Algorithm(ALG_INIT_NN,ALG_LOC_NONE,algo_str,node0) == -1)
			if(Algorithm(ALG_INIT_NN,ALG_LOC_3OPT,algo_str,node0) == -1)
				elog(ERROR,"No Compatible Algorithm selected!!!");
			 
			 //COMMIT WayPoint Route
			
			name = SourcesSort(points,tot_sources,algo_str);
			commit_WayPointRoute_result(points,tot_sources,route_path,wayback,name,-1,0);
			pfree(name);
			//Copy the aspected result 
			if(overlayroute)
			{
				if(CopyOverlayToPath(C, tot_sources, path, path_count, route_path, wayback) != 0)
					elog(ERROR,"CopyOverlayPath Error!!! pc=%d",*path_count);
			}
			else 
			{
				if(CopyRouteToPath(C, tot_sources, path, path_count, route_path, wayback) != 0)
					elog(ERROR,"CopyRoutePath Error!!! pc=%d",*path_count);
			}

#ifdef ROUTEPATH_LOG
			elog(INFO,"Total Sources = %d", tot_sources);
			for(z=0, i=tot_sources; z<i; z++)
				elog(INFO,"RoutePath[%d] = %d",z,route_path[z]);	
			if(wayback)
				elog(INFO,"RoutePath[%d] = %d",z,route_path[0]);

			elog(INFO,"Total Path element = %d",*path_count);

			//for(i=0;i<tot_sources;i++)
			//	for(j=0;j<tot_sources;j++)
			//		elog(INFO,"C[%d,%d]=%f -- pc=%d  --- p[0]=%d",i,j,C[i*tot_sources +j].cost, C[i*tot_sources +j].path_count, (*path)[0].edge_id);
#endif

			//*====================*
			//* Free Graph Overlay *
			//*====================*
			FreeC(&C,tot_sources);
			pfree(sources);
			pfree(points);

			return finish(ret);
		break; //case 0
		case 1:
				elog(ERROR,"GIà calcolato, come fare a ritornare il path che non è memorizzato? :)");
		break; //case 1
		case -1:
		default:
				elog(ERROR,"ERROR in get_* result!");
			break;
	}

	//return -1;
	return finish(-1);
}


/*****************************************************************
 ***					CORE FUNCTION							***
 *** ---------------------------------------------------------- ***
 *** compute_VRP												***
 *** FIX: Merge in a better big function (ex:MultiDijkstra)		***
 *** input:														***
 ***	sql				: query string to fetch data			***
 ***	subsql			: query string to fetch sources			***
 ***						(source_id, x, y)					***
 ***	directed		: directed graph?						***
 ***	has_reverse_cost: reverse cost (j,i) from (i,j)			***
 ***	wayback			: Compute the wayback path?				***
 ***	overlayroute	: result based on Overlay Graph			***
 *** ----------------------------------------------------------	***
 *** |						NB:								  |	***
 *** |	if(overlayroute) ==->	edge_id AS target			  |	***
 *** ----------------------------------------------------------	***
 *** output:													***
 ***	*path			: Array of path_element_t (solution)	***
 ***															***
 ***	path_count		: length of *path						***
 *** return:													***
 ***	= 0				: OK									***
 ***	< 0				: ERRROR								***
 ******************************************************************
 */
static inline int compute_VRP(char *sql, char* subsql,	bool directed, bool has_reverse_cost, bool wayback, bool overlayroute,
										path_element_t **path, int *path_count, char* subsql_depot)
{
	int tot_sources;
	int ret;
	multipath_t* C;
	Point_t* points;
	edge_t *edges;
	int v_min_id;
	char *err_msg;
	int total_tuples;
	register int z;
	//unsigned int* C_index;
	register int i;
#ifdef CACHE_RESULTS

	register int j;
#ifdef CACHE_DETAILED_RESULTS
	register int k;
#endif
#endif
	int* sources=NULL;
	int* capacities=NULL;
	int* capacities_used;
	int** routes_path;
	int* routes_length;
	float8* route_cost;
	int n_route;
	float8 tot_cost;
	char algo_str[256];
	//algorithm_u algo;
	char* name;
	
	int tot_depots;
	Point_t* depots;
	int tot_nodes;
	int depot_cap_used;
	Point_t* points_tmp;

	ret = Connect("VRP");


	//-----
	//get Deopt(s)
	//usare Get_Points_list??? si
	//il magazzino ha una x y capacità ...
	//usare depot_sources come vettore.
	//per il calcolo del grafo overlay usare multipath_t con un valore intero per capacita depot.
	//oppure impostare le capacità con valore negativo, ad indicare che sono capacità in funzione inversa rispetto alle richieste.

	//quindi per il calcolo overlay si ha in tutto tot_sources+tot_depot
	//se passo il sources_depot. 
	//riscrivere in sources la somma dei points.source_id + il magazzino


	// ******** FARE COSI' ****************
	//si hanno 2 chiamate a funzioni Get_Point_list
	//quindi 2 strutture ritornate di point_t + tot_sources e tot_depots
	//allocare sources come tot_sources + tot_depots
	//fare 2 cicli for copiarci i dati dentro (compreso anche capacities
	//mettere per primo i depositi. (in single depot è un valore solo)

	tot_depots = Get_Points_list(subsql_depot,&depots,true);
	tot_sources = Get_Points_list(subsql, &points_tmp, true);
	
	tot_nodes= tot_depots + tot_sources;
	//fix this, make it better
	points = (Point_t*) palloc(sizeof(Point_t)* (tot_nodes));
	 
	capacities = (int*) palloc(sizeof(int)* (tot_nodes));
	sources = (int*) palloc(sizeof(int)* (tot_nodes));
	if(sources==NULL)
	{
		elog(ERROR, "Out of Memory!");
		return -1;
	}

	for(z=0; z < tot_depots; z++)
	{
		
		points[z].source_id = sources[z] = depots[z].source_id;
		points[z].capacity = capacities[z] = depots[z].capacity;  //negative value for depots
		points[z].x = depots[z].x;
		points[z].y = depots[z].y;
	}
	pfree(depots); //depots in points are in ]0, tot_depots]
	
	for(z=0, i=tot_depots; z < tot_sources; z++,i++)
	{
		points[i].source_id = sources[i] = points_tmp[z].source_id;
		points[i].capacity = capacities[i]= points_tmp[z].capacity;
		points[i].x = points_tmp[z].x;
		points[i].y = points_tmp[z].y;
	}
	pfree(points_tmp); //source in ]tot_depots, tot_sources]
	
	
	if((sources==NULL)||(/*tot_sources*/ tot_nodes==0))
		elog(ERROR,"VRP: no sources!!!");
	
#ifdef CACHE_RESULTS	
#ifdef CACHE_DETAILED_RESULTS
	/*************************************************************/
	//FIX: CACHED Result of MUltidijkstra. NOT IMPLEMENTED (NOT COMPLETED)
	//check if the route was precomputed...
	//ret = CheckPrecomputedRoute(start_vertex, end_vertex, &res);
	ret=0;
#else //CACHE_RESULTS
	//leggere i valori già presenti nella tabella Graph_overlay 
	//per i valori di sources...
	//quelli non presenti settare il cost = -1
	//così dopo si sa che si deve calcolare il percorso dei nodi in cui il costo è -1
	//se dovessero mancare dei nodi...
	ret=0;
#endif
#else
	ret=0;
#endif
	
	switch (ret)
	{
		case 0:
			if ((total_tuples = PrepareEdges(sql, has_reverse_cost, &edges, &v_min_id)) < 2)
				return -1;
			
			//if translate to 0 index based array, edges.source and edges.target are also to do the same (do in PrepareEdges)
			//after that it can be 'restored inside the postprocessing boost_wrapper function for the dijkstra results.
			//not the *sources
			//for(z=0; z<tot_nodes;z++)
			//	sources[z]-=v_min_id;
	

			//Multiple Dijkstra Calling!!!
			//Return Graph_Overlay in C
			C=NULL;
			//ret = Boost_GraphOverlay(edges, total_tuples, sources, tot_sources,
			//							directed, has_reverse_cost,
			//							&err_msg, &C, v_min_id);
			ret = Boost_GraphOverlay(edges, total_tuples, sources, tot_nodes,
										directed, has_reverse_cost,
										&err_msg, &C, v_min_id);

			if ((ret < 0)||((C)==NULL))
			{
				ereport(ERROR, (errcode(ERRCODE_E_R_E_CONTAINING_SQL_NOT_PERMITTED), 
					errmsg("Error computing GraphOverlay: %s", err_msg)));
			}

			//sources not used anymore... don't need to restore original value
			//for(z=0; z<tot_nodes;z++)
			//	sources[z] +=v_min_id;

			//Ordering The Overlay Graph
			//C_index = BuildSortedIndexGraphOverlay(C,tot_nodes);
			//if(C_index==NULL)
			//	elog(ERROR,"Out of Memory!");
			
			//Compute Heuristic Trivial VRP (C_index not used)
			elog(INFO,"VRP...");
			routes_length=NULL;
			//routes_path = vrp(C,/* C_index,*/ tot_sources, node0, &n_route, &routes_length, &route_cost, capacities, &capacities_used);
			routes_path = vrp(C, tot_nodes, 0, &n_route, &routes_length, &route_cost, capacities, &capacities_used, &depot_cap_used);

			if (routes_path == NULL)
				elog(ERROR,"VRP Computing ERROR!!! (n_route=%d)",n_route);
			if(routes_length==NULL)
				elog(ERROR, "routes length !!! ERROR");
			if(capacities_used==NULL)
				elog(ERROR, "routes capacities used ERROR!!!");

			//show tot_cost
			for(z=0, tot_cost=0.0; z < n_route; z++)
			{
				elog(INFO,"VRP COST %d = %lf",z,route_cost[z]);
				tot_cost += route_cost[z];
			}
			elog(INFO,"VRP Total Cost = %lf",tot_cost);
			elog(INFO,"Depot capacity used= %d", depot_cap_used);

#ifdef ROUTEPATH_LOG
			elog(NOTICE,"VRP ROUTES = %d",n_route);
			elog(INFO,"Total Sources = %d", tot_nodes);
			for(z=0; z<n_route; z++)
			{
				elog(INFO,"routes_length[%d] = %d",z, routes_length[z]);

				for(i=0;i<routes_length[z]; i++)
					elog(INFO,"RoutesPath[%d][%d] = %d",z,i,routes_path[z][i]);	

				if(wayback)
					elog(INFO,"RoutesPath[%d][%d] = %d",z,i,routes_path[z][0]);
			}

#endif
			
			//FIX This Function: id is already computed. return the cached result...
			 //if(Algorithm(ALG_INIT_NN,ALG_LOC_NONE,algo_str,node0) == -1)
			if(Algorithm(ALG_INIT_CI,ALG_LOC_NONE,algo_str,0) == -1)
					elog(ERROR,"No Compatible Algorithm selected!!!");

			name = SourcesSort(points,tot_nodes,algo_str);

			for(z=0; z<n_route; z++)
			{
				 //COMMIT WayPoint Route
				elog(INFO,"Route %d length = %d",z,routes_length[z]);
				commit_WayPointRoute_result(points,tot_nodes,routes_path[z],wayback,name,z,routes_length[z]);
			}	
			//Commit VRP Route (single row)
			commit_VRPRoute_result(n_route, name, route_cost, capacities_used);

			pfree(name);

			//COMMIT the Graph Overlay 
			for(i=0,z=tot_nodes; i<z; i++)
			{
				int register index= i*tot_nodes;
				for(j=0; j<tot_nodes; j++, index++)
				{
					if(i==j)
						continue;
#ifdef CACHE_RESULTS
					//elog(WARNING,"index=%d , i=%d, j=%d (tot_nodes=%d) - cost=%f",index,i,j,tot_nodes,C[index].cost);
					if(commit_GraphOverlay_result(C[index].cost,points[i],points[j]) == -1)
						elog(WARNING,"MultiDijkstra: Commit geo-result error!!! (GraphOveraly)");
					
#ifdef CACHE_DETAILED_RESULTS
					for(k=1;k<C[index].path_count;k++)
						C[index].path[k].edge_id+=v_min_id;
					//COMMIT THE Graph Overlay Details
					if(commit_GraphOverlayDetails_result(points[i].source_id, points[j].source_id, C[index].path, C[index].path_count, v_min_id) == -1)
						elog(WARNING,"MultiDijkstra: Commit geo-result error!!! (GraphOveraly)");
#endif
				}

			}
#endif		
			
			//Copy the aspected result all the vrp solution...
			elog(NOTICE,"CopyResults...");
			if(overlayroute)
			{
				elog(WARNING,"COPY RESULT OVERLAY...");
				if(CopyOverlayVRPToPath(C, tot_nodes, path, path_count, routes_path, n_route, routes_length, wayback) != 0)
					elog(ERROR,"CopyOverlayPath Error!!! pc=%d",*path_count);
			}
			else 
			{
				if(CopyRouteVRPToPath(C, tot_nodes, path, path_count, routes_path, n_route, routes_length, wayback) != 0)
					elog(ERROR,"CopyRoutePath Error!!! pc=%d",*path_count);
			}
				

			//*====================*
			//* Free Graph Overlay *
			//*====================*
			FreeC(&C,tot_nodes);
			pfree(sources);
			pfree(capacities);
			pfree(points);
			//pfree(depots);

			return finish(ret);
		break; //case 0
		case 1:
				elog(ERROR,"GIà calcolato, come fare a ritornare il path che non è memorizzato? :)");
		break; //case 1
		case -1:
		default:
				elog(ERROR,"ERROR in get_* result!");
			break;
	}

	//return -1;
	return finish(-1);
}


/*****************
**	DLL Exports	**
******************/
PG_FUNCTION_INFO_V1(Dijkstra);
#ifdef __cplusplus
extern "C"
#endif
#ifdef _WIN32
PGDLLEXPORT
#endif 
			Datum Dijkstra(PG_FUNCTION_ARGS)
{
  FuncCallContext     *funcctx;
  int                  call_cntr;
  int                  max_calls;
  TupleDesc            tuple_desc;
  path_element_t      *path;

  /* stuff done only on the first call of the function */
  if (SRF_IS_FIRSTCALL())
	{
	  MemoryContext   oldcontext;
	  int path_count = 0;
	  int ret;
	  

	  /* create a function context for cross-call persistence */
	  funcctx = SRF_FIRSTCALL_INIT();

	  /* switch to memory context appropriate for multiple function calls */
	  oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
	  
	  ret = compute_Dijkstra(text2char(PG_GETARG_TEXT_P(0)),
								  PG_GETARG_INT32(1),
								  PG_GETARG_INT32(2),
								  PG_GETARG_BOOL(3),
								  PG_GETARG_BOOL(4), 
								  PG_GETARG_FLOAT8(5),
								  PG_GETARG_FLOAT8(6),
								  PG_GETARG_FLOAT8(7),
								  PG_GETARG_FLOAT8(8),
								  &path, &path_count);
	  
	  /* total number of tuples to be returned */
	  funcctx->max_calls = path_count;
	  funcctx->user_fctx = path;

	  funcctx->tuple_desc = 
		BlessTupleDesc(RelationNameGetTupleDesc("path_result"));

	  MemoryContextSwitchTo(oldcontext);
	}

  /* stuff done on every call of the function */
  funcctx = SRF_PERCALL_SETUP();

  call_cntr = funcctx->call_cntr;
  max_calls = funcctx->max_calls;
  tuple_desc = funcctx->tuple_desc;
  path = (path_element_t*) funcctx->user_fctx;

  if (call_cntr < max_calls)    /* do when there is more left to send */
	{
	  HeapTuple    tuple;
	  Datum        result;
	  Datum *values;
	  char* nulls;

	  /* This will work for some compilers. If it crashes with segfault, try to change the following block with this one    
 
	  values = palloc(4 * sizeof(Datum));
	  nulls = palloc(4 * sizeof(char));
  
	  values[0] = call_cntr;
	  nulls[0] = ' ';
	  values[1] = Int32GetDatum(path[call_cntr].vertex_id);
	  nulls[1] = ' ';
	  values[2] = Int32GetDatum(path[call_cntr].edge_id);
	  nulls[2] = ' ';
	  values[3] = Float8GetDatum(path[call_cntr].cost);
	  nulls[3] = ' ';
	  */
	
	  values = (Datum*) palloc(3 * sizeof(Datum));
	  nulls = (char*)palloc(3 * sizeof(char));
	  
	  values[0] = Int32GetDatum(path[call_cntr].vertex_id);
	  nulls[0] = ' ';
	  values[1] = Int32GetDatum(path[call_cntr].edge_id);
	  nulls[1] = ' ';
	  values[2] = Float8GetDatum(path[call_cntr].cost);
	  nulls[2] = ' ';
			  
	  tuple = heap_formtuple(tuple_desc, values, nulls);

	  /* make the tuple into a datum */
	  result = HeapTupleGetDatum(tuple);

	  /* clean up (this is not really necessary) */
	  pfree(values);
	  pfree(nulls);

	  SRF_RETURN_NEXT(funcctx, result);
	}
  else    /* do when there is no more left */
	{
	  SRF_RETURN_DONE(funcctx);
	}
}


PG_FUNCTION_INFO_V1(MultiDijkstra);
#ifdef __cplusplus
extern "C"
#endif
#ifdef _WIN32
PGDLLEXPORT
#endif 
			Datum MultiDijkstra(PG_FUNCTION_ARGS)
{
  FuncCallContext     *funcctx;
  int                  call_cntr;
  int                  max_calls;
  TupleDesc            tuple_desc;
  path_element_t      *path;
  
  /* stuff done only on the first call of the function */
  if (SRF_IS_FIRSTCALL())
	{
		MemoryContext   oldcontext;
		int path_count = 0;
		int ret;
		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context appropriate for multiple function calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		ret = compute_MultiDijkstra(text2char(PG_GETARG_TEXT_P(0)),
			text2char(PG_GETARG_TEXT_P(1)),
			PG_GETARG_BOOL(2),
			PG_GETARG_BOOL(3),
			PG_GETARG_BOOL(4),
			PG_GETARG_BOOL(5),
			&path, &path_count,
			PG_GETARG_INT32(6));
		
		/* total number of tuples to be returned */
		funcctx->max_calls = path_count;
		funcctx->user_fctx = path;

		funcctx->tuple_desc = 
			BlessTupleDesc(RelationNameGetTupleDesc("path_result"));

		MemoryContextSwitchTo(oldcontext);
  }

  /* stuff done on every call of the function */
  funcctx = SRF_PERCALL_SETUP();

  call_cntr = funcctx->call_cntr;
  max_calls = funcctx->max_calls;
  tuple_desc = funcctx->tuple_desc;
  path = (path_element_t*) funcctx->user_fctx;

  if (call_cntr < max_calls)    /* do when there is more left to send */
	{
	  HeapTuple    tuple;
	  Datum        result;
	  Datum *values;
	  char* nulls;

	  values = (Datum*) palloc(3 * sizeof(Datum));
	  nulls = (char*)palloc(3 * sizeof(char));

	  values[0] = Int32GetDatum(path[call_cntr].vertex_id);
	  nulls[0] = ' ';
	  values[1] = Int32GetDatum(path[call_cntr].edge_id);
	  nulls[1] = ' ';
	  values[2] = Float8GetDatum(path[call_cntr].cost);
	  nulls[2] = ' ';
	
	  tuple = heap_formtuple(tuple_desc, values, nulls);

	  /* make the tuple into a datum */
	  result = HeapTupleGetDatum(tuple);

	  /* clean up (this is not really necessary) */
	  pfree(values);
	  pfree(nulls);

	  SRF_RETURN_NEXT(funcctx, result);
	}
  else    /* do when there is no more left */
	{
	  SRF_RETURN_DONE(funcctx);
	}
}


PG_FUNCTION_INFO_V1(VRP);
#ifdef __cplusplus
extern "C"
#endif
#ifdef _WIN32
PGDLLEXPORT
#endif 
			Datum VRP(PG_FUNCTION_ARGS)
{
  FuncCallContext     *funcctx;
  int                  call_cntr;
  int                  max_calls;
  TupleDesc            tuple_desc;
  path_element_t      *path;
  
  /* stuff done only on the first call of the function */
  if (SRF_IS_FIRSTCALL())
	{
		MemoryContext   oldcontext;
		int path_count = 0;
		int ret;
		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context appropriate for multiple function calls */
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		//call VRP instead of multidijkstra. 
		//FIX: Join the function in a better (big) one.
		ret = compute_VRP(text2char(PG_GETARG_TEXT_P(0)),
			text2char(PG_GETARG_TEXT_P(1)),
			PG_GETARG_BOOL(2),
			PG_GETARG_BOOL(3),
			PG_GETARG_BOOL(4),
			PG_GETARG_BOOL(5),
			&path, &path_count,
			text2char(PG_GETARG_TEXT_P(6)));
		
		/* total number of tuples to be returned */
		funcctx->max_calls = path_count;
		funcctx->user_fctx = path;

		funcctx->tuple_desc = 
			BlessTupleDesc(RelationNameGetTupleDesc("path_result"));

		MemoryContextSwitchTo(oldcontext);
  }

  /* stuff done on every call of the function */
  funcctx = SRF_PERCALL_SETUP();

  call_cntr = funcctx->call_cntr;
  max_calls = funcctx->max_calls;
  tuple_desc = funcctx->tuple_desc;
  path = (path_element_t*) funcctx->user_fctx;

  if (call_cntr < max_calls)    /* do when there is more left to send */
	{
	  HeapTuple    tuple;
	  Datum        result;
	  Datum *values;
	  char* nulls;

	  values = (Datum*) palloc(3 * sizeof(Datum));
	  nulls = (char*)palloc(3 * sizeof(char));

	  values[0] = Int32GetDatum(path[call_cntr].vertex_id);
	  nulls[0] = ' ';
	  values[1] = Int32GetDatum(path[call_cntr].edge_id);
	  nulls[1] = ' ';
	  values[2] = Float8GetDatum(path[call_cntr].cost);
	  nulls[2] = ' ';
	
	  tuple = heap_formtuple(tuple_desc, values, nulls);

	  /* make the tuple into a datum */
	  result = HeapTupleGetDatum(tuple);

	  /* clean up (this is not really necessary) */
	  pfree(values);
	  pfree(nulls);

	  SRF_RETURN_NEXT(funcctx, result);
	}
  else    /* do when there is no more left */
	{
	  SRF_RETURN_DONE(funcctx);
	}
}