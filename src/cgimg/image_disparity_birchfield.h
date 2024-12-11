#ifndef  __IMAGE_DISPARITY_BIRCHFIELD_H__
#define  __IMAGE_DISPARITY_BIRCHFIELD_H__

#include "image.h"

class DisparityBirchfield
{
public:
	DisparityBirchfield();
	~DisparityBirchfield();

	void SetStereoPair(Img *pLeft, Img *pRight);

	void setOcclusionPenalty(int);
	int getOcclusionPenalty(void);
	void setReward(int);
	int getReward(void);


	void Process(void);
	void PostProcess(void);

	Img* GetDisparity (void)
	{
		return m_pDisparity1;
	};

private:
	Img *m_pImgLeft, *m_pImgRight;
	Img *m_pDisparity1, *m_pDiscontinuities1; // Results after matching the scanlines independently
	Img *m_pDisparity2, *m_pDiscontinuities2; // Results after postprocessing the first disparity map
};

#endif // __IMAGE_DISPARITY_BIRCHFIELD_H__