#pragma once
#include "parameterized.h"
#include "mesh.h"

//
// Common base for parameterized objects that own a Mesh*. Handles
// destruction and TakeMesh() boilerplate so subclasses only need to
// implement GetParameters(), Regenerate() and GetName().
//
class ParameterizedMesh : public IParameterized
{
public:
	~ParameterizedMesh() override { delete m_pMesh; }
	Mesh* GetMesh() { return m_pMesh; }
	Mesh* TakeMesh() override { Mesh *m = m_pMesh; m_pMesh = nullptr; return m; }

protected:
	Mesh *m_pMesh = nullptr;
};

// ---------------------------------------------------------------------------
// Basic shapes
// ---------------------------------------------------------------------------

class ParameterizedCube : public ParameterizedMesh
{
public:
	ParameterizedCube();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Cube"; }
private:
	float m_edgeLength = 1.f;
	bool m_triangulated = true;
};

class ParameterizedSphere : public ParameterizedMesh
{
public:
	ParameterizedSphere();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Sphere"; }
private:
	int m_nu = 20, m_nv = 20;
};

class ParameterizedCylinder : public ParameterizedMesh
{
public:
	ParameterizedCylinder();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Cylinder"; }
private:
	float m_height = 2.f, m_radius = 1.f;
	int m_nVertices = 32;
	bool m_cap = true;
};

class ParameterizedCone : public ParameterizedMesh
{
public:
	ParameterizedCone();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Cone"; }
private:
	float m_height = 2.f, m_radius = 1.f;
	int m_nVertices = 32;
	bool m_cap = true;
};

class ParameterizedCapsule : public ParameterizedMesh
{
public:
	ParameterizedCapsule();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Capsule"; }
private:
	int m_n = 20;
	float m_height = 2.f, m_radius = 1.f;
};

class ParameterizedTorus : public ParameterizedMesh
{
public:
	ParameterizedTorus();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Torus"; }
private:
	int m_nu = 30, m_nv = 30;
	float m_R = 5.f, m_r = 2.f;
};

// ---------------------------------------------------------------------------
// Parametric surfaces
// ---------------------------------------------------------------------------

class ParameterizedKleinBottle : public ParameterizedMesh
{
public:
	ParameterizedKleinBottle();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Klein Bottle"; }
private:
	int m_thetaRes = 20, m_phiRes = 20;
};

class ParameterizedHelicoid : public ParameterizedMesh
{
public:
	ParameterizedHelicoid();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Helicoid"; }
private:
	int m_nu = 20, m_nv = 20;
	float m_a = 1.f, m_b = 1.f, m_c = 0.2f;
};

class ParameterizedSeashell : public ParameterizedMesh
{
public:
	ParameterizedSeashell();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Seashell"; }
private:
	int m_nu = 50, m_nv = 50;
};

class ParameterizedSeashellVonSeggern : public ParameterizedMesh
{
public:
	ParameterizedSeashellVonSeggern();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Seashell (von Seggern)"; }
private:
	int m_nu = 50, m_nv = 50;
	float m_a = 0.2f, m_b = 1.f, m_c = 0.1f, m_n = 2.f;
};

class ParameterizedCorkscrew : public ParameterizedMesh
{
public:
	ParameterizedCorkscrew();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Corkscrew"; }
private:
	int m_nu = 30, m_nv = 30;
	float m_a = 1.f, m_b = 0.5f;
};

class ParameterizedMobiusStrip : public ParameterizedMesh
{
public:
	ParameterizedMobiusStrip();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Mobius Strip"; }
private:
	int m_nu = 50, m_nv = 10;
	float m_w = 0.1f, m_r = 0.5f;
};

class ParameterizedRadialWave : public ParameterizedMesh
{
public:
	ParameterizedRadialWave();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Radial Wave"; }
private:
	int m_nu = 50, m_nv = 50;
	float m_radius = 10.f, m_height = 20.f, m_frequency = 0.6f;
};

class ParameterizedBreather : public ParameterizedMesh
{
public:
	ParameterizedBreather();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Breather"; }
private:
	int m_nu = 50, m_nv = 50;
};

class ParameterizedHyperbolicParaboloid : public ParameterizedMesh
{
public:
	ParameterizedHyperbolicParaboloid();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Hyperbolic Paraboloid"; }
private:
	int m_nu = 30, m_nv = 30;
	float m_xmin = -5.f, m_xmax = 5.f, m_ymin = -5.f, m_ymax = 5.f;
};

class ParameterizedMonkeySaddle : public ParameterizedMesh
{
public:
	ParameterizedMonkeySaddle();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Monkey Saddle"; }
private:
	int m_nu = 30, m_nv = 30;
	float m_xmin = -5.f, m_xmax = 5.f, m_ymin = -5.f, m_ymax = 5.f;
};

class ParameterizedBlobs : public ParameterizedMesh
{
public:
	ParameterizedBlobs();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Blobs"; }
private:
	int m_nu = 50, m_nv = 50;
	float m_xmin = -5.f, m_xmax = 5.f, m_ymin = -5.f, m_ymax = 5.f;
};

class ParameterizedDrop : public ParameterizedMesh
{
public:
	ParameterizedDrop();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Drop"; }
private:
	int m_nu = 50, m_nv = 50;
	float m_xmin = -5.f, m_xmax = 5.f, m_ymin = -5.f, m_ymax = 5.f;
};

class ParameterizedGuimard : public ParameterizedMesh
{
public:
	ParameterizedGuimard();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Guimard"; }
private:
	int m_nu = 30, m_nv = 30;
	float m_a = 2.f, m_b = 3.f, m_c = 1.f;
};

// ---------------------------------------------------------------------------
// Knots
// ---------------------------------------------------------------------------

class ParameterizedTorusKnot : public ParameterizedMesh
{
public:
	ParameterizedTorusKnot();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Torus Knot"; }
private:
	int m_nu = 100, m_nv = 20;
	int m_a = 3, m_b = 4;
};

class ParameterizedCinquefoilKnot : public ParameterizedMesh
{
public:
	ParameterizedCinquefoilKnot();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Cinquefoil Knot"; }
private:
	int m_nu = 100, m_nv = 20;
	int m_a = 3;
};

class ParameterizedTrefoilKnot : public ParameterizedMesh
{
public:
	ParameterizedTrefoilKnot();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Trefoil Knot"; }
private:
	int m_nu = 100, m_nv = 20;
};

class ParameterizedBorromeanRings : public ParameterizedMesh
{
public:
	ParameterizedBorromeanRings();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Borromean Rings"; }
private:
	int m_nu = 100, m_nv = 20;
};

// ---------------------------------------------------------------------------
// Fractal shapes
// ---------------------------------------------------------------------------

class ParameterizedMengerSponge : public ParameterizedMesh
{
public:
	ParameterizedMengerSponge();
	std::vector<Parameter> GetParameters() override;
	void Regenerate() override;
	std::string GetName() const override { return "Menger Sponge"; }
private:
	int m_level = 2;
};
