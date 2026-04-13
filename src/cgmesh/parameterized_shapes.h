#pragma once
#include "parameterized.h"
#include "mesh.h"
#include "voxels_menger_sponge.h"

//
// Example: parameterized wrapper around CreateCube / a scaled cube.
// Exposes a single parameter: edge length.
//
class ParameterizedCube : public IParameterized
{
public:
	ParameterizedCube();
	~ParameterizedCube() override;

	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Cube"; }

	Mesh* GetMesh() { return m_pMesh; }

	// Transfer ownership of the current mesh to the caller.
	// After this call, GetMesh() returns nullptr until the next Regenerate().
	Mesh* TakeMesh() override { Mesh *m = m_pMesh; m_pMesh = nullptr; return m; }

private:
	float m_edgeLength = 1.f;
	bool m_triangulated = true;
	Mesh *m_pMesh = nullptr;
};

//
// Example: parameterized Menger sponge.
// Exposes the recursion level (1..4 -- upper bound imposed by CreateMengerSponge).
//
class ParameterizedMengerSponge : public IParameterized
{
public:
	ParameterizedMengerSponge();
	~ParameterizedMengerSponge() override;

	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Menger Sponge"; }

	Mesh* GetMesh() { return m_pMesh; }
	Mesh* TakeMesh() override { Mesh *m = m_pMesh; m_pMesh = nullptr; return m; }

private:
	int m_level = 2;
	Mesh *m_pMesh = nullptr;
};
