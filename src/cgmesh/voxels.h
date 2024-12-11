#ifndef __VOXELS_H__
#define __VOXELS_H__

#include "../cgimg/cgimg.h"

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

	// export methods
	int export_slice_XY (char *filename, unsigned int islice);
	int export_slice_YZ (char *filename, unsigned int islice);
	int export_slice_XZ (char *filename, unsigned int islice);
	int triangulate (char *filename);
	
	int export_cubes (char *filename);
	int export_palette_to_mtl (char *mtlfile);

protected:
	Voxel*** m_pVoxels;
	unsigned int m_nx, m_ny, m_nz;
	Palette *m_pPalette;
};

#endif // __VOXELS_H__
