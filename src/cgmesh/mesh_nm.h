#pragma once
#include "mesh.h"

class Mesh_nm :	public Mesh
{
public:
	Mesh_nm ();
	~Mesh_nm ();

	int load (char *filename);

private:
	int import_objnm (char *filename);

	float* vt;
	float* tangent;
	float* binormal;
	float* tangentSpaceLight;

	Material *texture;
};
