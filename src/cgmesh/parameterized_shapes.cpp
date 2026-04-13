#include "parameterized_shapes.h"
#include "surface_basic.h"

// ---------------------------------------------------------------------------
// ParameterizedCube
// ---------------------------------------------------------------------------

ParameterizedCube::ParameterizedCube()
{
	Regenerate();
}

ParameterizedCube::~ParameterizedCube()
{
	delete m_pMesh;
}

std::vector<Parameter> ParameterizedCube::GetParameters()
{
	return {
		Parameter::MakeFloat("Edge length", &m_edgeLength, 0.01f, 100.f),
		Parameter::MakeBool("Triangulated", &m_triangulated),
	};
}

void ParameterizedCube::Regenerate()
{
	delete m_pMesh;
	m_pMesh = CreateCube(m_triangulated);

	// scale the cube in place (CreateCube produces a cube of edge 2 centered at origin)
	float scale = m_edgeLength / 2.f;
	for (unsigned int i = 0; i < m_pMesh->m_nVertices; i++)
	{
		m_pMesh->m_pVertices[3 * i]     *= scale;
		m_pMesh->m_pVertices[3 * i + 1] *= scale;
		m_pMesh->m_pVertices[3 * i + 2] *= scale;
	}
	m_pMesh->ComputeNormals();
}

// ---------------------------------------------------------------------------
// ParameterizedMengerSponge
// ---------------------------------------------------------------------------

ParameterizedMengerSponge::ParameterizedMengerSponge()
{
	Regenerate();
}

ParameterizedMengerSponge::~ParameterizedMengerSponge()
{
	delete m_pMesh;
}

std::vector<Parameter> ParameterizedMengerSponge::GetParameters()
{
	return {
		Parameter::MakeInt("Level", &m_level, 1, 4),
	};
}

void ParameterizedMengerSponge::Regenerate()
{
	delete m_pMesh;
	MengerSponge sponge((unsigned int)m_level);
	m_pMesh = sponge.ToMesh();
	if (m_pMesh)
		m_pMesh->ComputeNormals();
}
