#include "disparity.h"

static unsigned int block_SAD(Img *pBlock1, Img *pBlock2)
{
	unsigned int w = pBlock1->width();
	unsigned int h = pBlock1->height();
	unsigned int sad = 0;
	unsigned char r1, g1, b1, r2, g2, b2, a;
	for (unsigned int j=0; j<h; j++)
		for (unsigned int i=0; i<w; i++)
		{
			pBlock1->get_pixel (i, j, &r1, &g1, &b1, &a);
			pBlock2->get_pixel (i, j, &r2, &g2, &b2, &a);
			sad += ABS((int) r2 -r1);
			sad += ABS((int) g2 -g1);
			sad += ABS((int) b2 -b1);
		}

	return sad;
}

int DisparityEvaluator::Compute_Block (void)
{
	unsigned int width = m_pLeft->width ();
	unsigned int height = m_pLeft->height ();
	m_pDisparity = new Img (width, height);

	unsigned int block_size = 16;
	unsigned int maxdisp = 100;

	Img *pBlock1 = new Img ();
	Img *pBlock2 = new Img ();
	for (unsigned int j=0; j<height; j++)
	{
		printf ("%d / %d\n", j, height);
		for (unsigned int i=0; i<width; i++)
		{
			pBlock1->crop (m_pLeft, i-block_size/2, j-block_size/2, block_size, block_size);

			unsigned int mindiff = 765 * block_size * block_size;
			unsigned int mind = 0;

			for (unsigned int d=40; d<maxdisp; d++)
			{
				int x2 = i - d;
				if (x2 < 0)
					continue;

				unsigned int sumdiff;
				pBlock2->crop (m_pRight, x2-block_size/2, j-block_size/2, block_size, block_size);

				sumdiff = block_SAD(pBlock1, pBlock2);
				if (sumdiff < mindiff)
				{
					mindiff = sumdiff;
					mind = d;
				}
			}

			m_pDisparity->set_pixel (i, j, mind, mind, mind, 255);
		}
	}

	delete pBlock1;
	delete pBlock2;

	return 0;
}

