# PgIRoute

This is a fork of pgRouting v1.4 with advanced routing algorithms.

I decide to disclose this project as a show case,

anyway i suggest to check the new version of pgRouting instead of using this one. [pgRouting](http://pgrouting.org/).

This fork was developed at the time that pgRouting was a little bit buggy and not maintained for a while.

----

# Main characteristics

Please note the original project is hosted here [](http://pgrouting.org/), 
since the developing of this fork, other works has been done in the pgRouting project itself.

The advanced routing algorithm respect of the pgRouting v1.4 are capable of:

- compute Dijkstra algorithm with PQ.
- candidate list using QuickSort
- Compute Nearest Neighbour
- compute TSP based on the overlaygraph with an asymmetric 3-opt heuristic algorithm
- compute a single base depot VRP Insertion.
- return the results in a format table as a overlay graph or in detailed for each node used from the routing network.

The structured of table used is described here: [Graph Network](https://pgiroute.codeplex.com/wikipage?title=Graph%20Network&referringTitle=Home)

## Requirements

- PosgreSql
- PostGis 2.0
- C++ / C (Visual Studio suggested)
- Boost lib
- OSM data imported in a structured table (see relative document for the structure of table)
