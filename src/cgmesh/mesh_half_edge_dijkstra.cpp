#include <assert.h>
#include <stdlib.h>
#include <stdio.h>


#include "mesh_half_edge.h"

#if 0
#include "../Common/queue.h"

/**
*
* Compute the shortest path between two vertices according to the Dijkstra's algorithm.
*
*/
void
Mesh_half_edge::dijkstra_shortest_path (int source, int target, int *n, int **path)
{
  float *distances    = (float*)malloc(nv*sizeof(float));
  int   *predecessors = (int*)malloc(nv*sizeof(int));
  assert (distances && predecessors);
  
  Queue q;
  int i, j;
  for (i=0; i<nv; i++)
    {
      predecessors[i] = -1;
      if (i == source)
	{
	  q.add (i);
	  distances[i] = 0.0;
	}
      else
	distances[i] = -1.0;
    }
  
  int target_reached = 0;
  while (!q.isEmpty () && !target_reached)
    {
      int icurrent = q.get ();
      //printf ("treating %d...\n", icurrent);
      Che *e = m_edges_vertex[icurrent];
      Che *e_walk = e;
      do
	{
	  int itmp = e_walk->m_v_end;
	  //printf ("   itmp = %d\n", itmp);
	  Vector3d a (v[3*icurrent], v[3*icurrent+1], v[3*icurrent+2]);
	  Vector3d b (v[3*itmp], v[3*itmp+1], v[3*itmp+2]);
	  a = a - b;
	  float weight = a.getLength ();
	  if (distances[itmp] < 0.0 || distances[itmp] > distances[icurrent] + weight)
	    {
	      distances[itmp] = distances[icurrent] + weight;
	      predecessors[itmp] = icurrent;
	      q.add (itmp);
	      
	      if (itmp == target) target_reached = 1;
	    }
	  //printf ("       predecessor = %d\n", predecessors[itmp]);
	  e_walk = e_walk->m_he_next->m_he_next->m_pair;
	} while (e_walk && e_walk != e);
    }
  
  
  
  //printf ("dump\n");
  int size;
  for (i=target, size=1; i!=source; size++, i=predecessors[i]);
  int *_path = (int*)malloc(size*sizeof(int));
  for (i = target, j=0; i!=source; j++, i=predecessors[i]) _path[size-1-j] = i;
  _path[0] = source;
  *path = _path;
  *n = size;
}
#endif

