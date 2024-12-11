#ifndef __GEOMETRIC_PRIMITIVES_H__
#define __GEOMETRIC_PRIMITIVES_H__

#include "../cgmath/cgmath.h"
#include "mesh_half_edge.h"

//
//
//
class VectorizedElement
{
public:
	enum VectorizedElementType {GEOMETRIC_PRIMITIVE_UNKNOWN = 0,
								GEOMETRIC_PRIMITIVE_POINT,
								GEOMETRIC_PRIMITIVE_LINE,
								GEOMETRIC_PRIMITIVE_PLANE,
								GEOMETRIC_PRIMITIVE_CIRCLE};
	VectorizedElement (unsigned int *pElements, unsigned int nElements, VectorizedElementType type);
	~VectorizedElement() {};
	inline VectorizedElementType get_type () { return m_type; };

	inline unsigned int get_n_elements (void) { return m_nElements; };
	inline unsigned int* get_elements (void) { return m_pElements; };

private:
	VectorizedElementType m_type;
	//Mesh_half_edge *m_pMesh;
	unsigned int m_nElements;
	unsigned int *m_pElements;
};

//
// Vectorized Point
//
class VectorizedPoint : public VectorizedElement
{
public:
	VectorizedPoint (unsigned int *pElements, unsigned int nElements, Vector3f pos);
	~VectorizedPoint () {};

//private:
	Vector3f m_position;
};

//
// Vectorized Line
//
class VectorizedLine : public VectorizedElement
{
public:
	VectorizedLine (unsigned int *pElements, unsigned int nElements, Vector3f start, Vector3f end);
	~VectorizedLine () {};

//private:
	Vector3f m_start, m_end;
};

//
// Vectorized Plane
//
class VectorizedPlane : public VectorizedElement, Plane
{
public:
	VectorizedPlane (unsigned int *pElements, unsigned int nElements, Plane *pPlane);
	~VectorizedPlane () {};

	void set_area (float fArea) { m_fArea = fArea; };
	float get_area (void) { return m_fArea; };

//private:
	Plane *m_pPlane;
	float m_fArea;
};

//
//
//
class Cgeometric_primitives
{
	public:
		Cgeometric_primitives (Mesh_half_edge *model, char *filename);
		~Cgeometric_primitives ();

		void add_geometric_primitive (VectorizedElement *geometric_primitive);
		int  get_n_geometric_primitives (void);
	    VectorizedElement* get_geometric_primitive (int index);

		void dump (void);
		void refresh_vertices_colors (void);
		void draw (void);
	
	private:
		Mesh_half_edge *model;
		int ngp;
		VectorizedElement **gp;	
};

#endif // __GEOMETRIC_PRIMITIVES_H__
