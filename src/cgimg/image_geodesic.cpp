#include <queue> // to use a priority queue
using namespace std;

#include "image.h"

#include "../cgmath/common.h"

// references to use a priority queue with a struct :
// http://en.cppreference.com/w/cpp/container/priority_queue
// http://comsci.liu.edu/~jrodriguez/cs631sp08/c++priorityqueue.html

typedef struct PixelClose {
	int i, j;
	float d;
	int offset;
} PixelClose;

class ComparePixelClose {
    public:
	bool operator()(PixelClose& c1, PixelClose& c2) // Returns true if t1 is earlier than t2
		{
			if (c1.d < c2.d) return true;
			return false;
		};
};

static void _compute_offsets_for_neighbours (int offset_neighbours[4], int i, int j, int w, int h)
{
	int offset = w*j+i;
	offset_neighbours[0] = (j == 0)? -1 : offset - w; // up
	offset_neighbours[1] = (j == h-1)? -1 : offset + w; // down
	offset_neighbours[2] = (i == 0)? -1 : offset - 1; // left
	offset_neighbours[3] = (i == w-1)? -1 : offset + 1; // right
}

// geodesic
int Img::geodesic (void)
{
	// distances
	float *d = (float*)malloc(m_iWidth*m_iHeight*sizeof(float));
	memset (d, 0, m_iWidth*m_iHeight*sizeof(float));

	// labels : 0 -> unprocessed / 1 -> close / 2 -> fixed
	char *l = (char*)malloc(m_iWidth*m_iHeight*sizeof(char));
	memset (l, 0, m_iWidth*m_iHeight*sizeof(char));
	
	//
	// init d & l
	//
	unsigned i, j;
	int offset;

	for (j=0; j<m_iHeight; j++)
		for (i=0; i<m_iWidth; i++)
		{
			int offset = m_iWidth*j + i;
			d[offset] = 100000000.; // infinite
			l[offset] = 0;   // unprocessed
		}

	// identify the fixed pixels
	for (j=0; j<m_iHeight; j++)
		for (i=0; i<m_iWidth; i++)
		{
			int offset = m_iWidth*j + i;
			if (get_r (i, j) == 0) // black => source
			{
				d[offset] = 0.;
				l[offset] = 2; // fixed
				
				// update the 4-neighbours
				int offsetneighbours[4];
				_compute_offsets_for_neighbours (offsetneighbours, i, j, m_iWidth, m_iHeight);

				for (int k=0; k<4; k++)
				{
					if (offsetneighbours[k] >= 0 && l[offsetneighbours[k]] != 2)
					{
						l[offsetneighbours[k]] = 1; // close
						d[offsetneighbours[k]] = 1.; // h = 1

						// debug
						m_pPixels[4*offsetneighbours[k]]   = 1;
						m_pPixels[4*offsetneighbours[k]+1] = 1;
						m_pPixels[4*offsetneighbours[k]+2] = 1;
						m_pPixels[4*offsetneighbours[k]+3] = 255;
					}
				}
			}
		}

	//
	// compute geodesic
	//
	// init the priority queue
	priority_queue<PixelClose, vector<PixelClose>, ComparePixelClose> pq;
	for (j=1; j<m_iHeight-1; j++)
		for (i=1; i<m_iWidth-1; i++)
		{
			if (l[m_iWidth*j + i] == 1)
			{
				PixelClose pc;
				pc.i = i;
				pc.j = j;
				pc.d = d[m_iWidth*j + i];
				pq.push(pc);
			}
		}

	// compute
	int mm = 1; // for debug
	while (!pq.empty())
	{
		PixelClose pc = pq.top();
		pq.pop();

		offset = m_iWidth*pc.j+pc.i;

		// debug
		//printf ("%d %d\n", pc.i, pc.j);
		mm++;
		m_pPixels[4*offset]   = (unsigned char)d[offset];
		m_pPixels[4*offset+1] = (unsigned char)d[offset];
		m_pPixels[4*offset+2] = (unsigned char)d[offset];
		m_pPixels[4*offset+2] = (unsigned char)d[offset];
		//if (mm==250)
		//	break;

		
		int offsetneighbours[4];
		_compute_offsets_for_neighbours (offsetneighbours, pc.i, pc.j, m_iWidth, m_iHeight);

		if (offsetneighbours[0] == -1 ||
		    offsetneighbours[1] == -1 ||
		    offsetneighbours[2] == -1 ||
		    offsetneighbours[3] == -1)
			continue;

		for (int i=0; i<4; i++) // for each neighbour
		{
			if (offsetneighbours[i] != -1 && l[offsetneighbours[i]] == 0) // unprocessed
			{
				// create a new close pixel
				PixelClose pc_new;
				pc_new.offset = offsetneighbours[i];
				l[offsetneighbours[i]] = 1;
			
				// evaluate
				int offsetneighbours2[4];
				if (i==0) // up
				{
					pc_new.i = pc.i;
					pc_new.j = pc.j-1;
				}
				else if (i==1) // down
				{
					pc_new.i = pc.i;
					pc_new.j = pc.j+1;
				}
				else if (i==2) // left
				{
					pc_new.i = pc.i-1;
					pc_new.j = pc.j;
				}
				else if (i==3) // right
				{
					pc_new.i = pc.i+1;
					pc_new.j = pc.j;
				}
				_compute_offsets_for_neighbours (offsetneighbours2, pc.i, pc.j-1, m_iWidth, m_iHeight);

				if (offsetneighbours2[0] == -1 ||
				    offsetneighbours2[1] == -1 ||
				    offsetneighbours2[2] == -1 ||
				    offsetneighbours2[3] == -1)
				    continue;

				// solve quadratic
				bool UseCol = ((l[offsetneighbours2[0]] == 2) || (l[offsetneighbours2[1]] == 2));
				bool UseRow = ((l[offsetneighbours2[2]] == 2) || (l[offsetneighbours2[3]] == 2));
				float ColTerm = MIN (d[offsetneighbours2[0]], d[offsetneighbours2[1]]);
				float RowTerm = MIN (d[offsetneighbours2[2]], d[offsetneighbours2[3]]);
				
				float a, b, c;
				if ((UseRow && !UseCol) || (UseCol && !UseRow))
				{
					float T = MAX (RowTerm, ColTerm);
					a = 1.;
					b = 2. * T;
					c = T * T - (1 / (1*1));
				}
				else
				{
					a = 2.;
					b = 2. * ColTerm + 2. * RowTerm;
					c = RowTerm * RowTerm + ColTerm * ColTerm - 1 / (1 * 1);
				}
				float Delta = b*b-4*a*c;
				d[offsetneighbours[i]] = (-b+sqrt(Delta))/(2*a);
				pc_new.d = d[offsetneighbours[i]];

				// add the pixel in the priority queue
				pq.push(pc_new);
			}
		}
	}

	return 0;
}
