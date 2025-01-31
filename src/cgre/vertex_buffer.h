#pragma once

#include "../cgmesh/cgmesh.h"

class VertexBuffer
{
public:
	VertexBuffer ()
	{
		m_pMesh = NULL;
	};
	~VertexBuffer ();

	void SetMesh	(Mesh *pMesh) { m_pMesh = pMesh; };
	Mesh* GetMesh	(void) { return m_pMesh; };

	void Draw (bool bColor);

private:
	Mesh*	m_pMesh;
};
