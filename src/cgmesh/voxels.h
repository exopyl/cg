#pragma once
#include <functional>

#include "../cgimg/cgimg.h"

class Mesh;

// One exposed voxel face: its owner voxel and the 4 corner grid-coordinates
// (each {x,y,z}), ordered CCW as seen from OUTSIDE the voxel (outward normal).
// Produced by Voxels::forEachSurfaceQuad — the single surface-extraction core
// shared by triangulate() (OBJ export) and ToMesh() (in-memory mesh).
struct VoxelSurfaceFace
{
	unsigned int i, j, k;          // owner voxel
	int          face;             // 0:-X 1:+X 2:-Y 3:+Y 4:-Z 5:+Z
	unsigned int corner[4][3];     // 4 corners, grid coords, CCW seen from outside
};

//
//
//
class Voxel
{
	friend class Voxels;
	friend class MengerSponge;
public:
	Voxel ();
	~Voxel ();
private:
	bool m_bActivated;
	unsigned int m_iLabel;
	float m_fData;
	//void *m_pData;
};

//
//
//
class Voxels
{
public:
	Voxels (unsigned int _nx=0, unsigned int _ny=0, unsigned int _nz=0);
	~Voxels ();

	int init (unsigned int nx, unsigned int ny, unsigned int nz);
	
	int input_vxl (char *filename);
	int input_img (Img *img);
	int input_imgs (Img **img, unsigned int nImgs);
	int input (char *filename);

	float get_data (unsigned int i, unsigned int j, unsigned int k);
	float get_data_for_intersection (unsigned int i, unsigned int j, unsigned int k);
	void get_extremal_values (float *min, float *max);

	unsigned int get_nx () const { return m_nx; }
	unsigned int get_ny () const { return m_ny; }
	unsigned int get_nz () const { return m_nz; }
	bool is_activated (unsigned int i, unsigned int j, unsigned int k) const { return m_pVoxels[i][j][k].m_bActivated; }
	unsigned int get_label (unsigned int i, unsigned int j, unsigned int k) const { return m_pVoxels[i][j][k].m_iLabel; }
	void set_data (unsigned int i, unsigned int j, unsigned int k, float data) { m_pVoxels[i][j][k].m_fData = data; }

	void smooth_data (int n);
	void threshold_data (float threshold);

	inline void activate (unsigned int xi, unsigned int yi, unsigned int zi)
	{
		m_pVoxels[xi][yi][zi].m_bActivated = true;
	};
	inline void set_label (unsigned int xi, unsigned int yi, unsigned int zi, unsigned int ilabel)
	{
		m_pVoxels[xi][yi][zi].m_iLabel = ilabel;
	};
	void inverse_activation (void);

	// morphologic operators
	void dilation (void);


	// label management
	void reset_labels (void);

	// Set the colour palette from `ncolors` RGB triplets (bytes 0..255). Voxel
	// labels index into it; ToMesh() uses it to colour the surface vertices.
	void set_palette (const unsigned char* rgb, unsigned int ncolors);

	// export methods
	int export_slice_XY (char *filename, unsigned int islice);
	int export_slice_YZ (char *filename, unsigned int islice);
	int export_slice_XZ (char *filename, unsigned int islice);
	int triangulate (char *filename);

	// Triangulate the activated voxels into a newly-allocated Mesh.
	// Caller takes ownership of the returned pointer.
	// Pure geometry (no textures / materials) -- see triangulate(filename)
	// for the OBJ export with textures and materials.
	Mesh* ToMesh (void);
	
	int export_cubes (char *filename);
	int export_palette_to_mtl (char *mtlfile);

private:
	// Shared voxel-surface extraction core (used by triangulate() and ToMesh()).
	unsigned int gridIndex (unsigned int i, unsigned int j, unsigned int k) const;
	void buildVertexGrid (float* v) const;   // fills the (nx+1)(ny+1)(nz+1) grid positions
	void forEachSurfaceQuad (const std::function<void(const VoxelSurfaceFace&)>& emit) const;

protected:
	Voxel*** m_pVoxels;
	unsigned int m_nx, m_ny, m_nz;
	Palette *m_pPalette;
};
