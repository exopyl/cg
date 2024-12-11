#include "mesh.h"

void Mesh::triangulate_regular_heightfield (unsigned int width, unsigned int height)
{
	DeleteFaces ();
	
	if (width * height > m_nVertices)
	{
		printf ("Not enough vertices to triangulate\n");
		return;
	}

	InitFaces ((width-1)*(height-1));

	for (unsigned int j=0; j<height-1; j++)
		for (unsigned int i=0; i<width-1; i++)
		{
			SetFace ((width-1)*j+i, (width)*j+i, (width)*(j+1)+i, (width)*(j+1)+i+1, (width)*j+i+1);
			m_pFaces[(width-1)*j+i]->SetTexCoord ((width-1)*j+i, i/(width-1), j/(height-1));
		}
}
