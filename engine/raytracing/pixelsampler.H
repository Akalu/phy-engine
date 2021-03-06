/* pixelsampler.H
 *
 * samples a pixel. 
 */

#ifndef _PIXELSAMPLER_H_
#define _PIXELSAMPLER_H_

#include "sampler.H"

class CPixelSampler : public CSampler
{
public:
  // Sample : newNode gets filled, others may change
  virtual bool Sample(CPathNode *prevNode, CPathNode *thisNode,
		      CPathNode *newNode, double x_1, double x_2,
		      bool doRR = false, 
		      BSDFFLAGS flags = BSDF_ALL_COMPONENTS);
  virtual double EvalPDF(CPathNode *thisNode, CPathNode *newNode,
			 BSDFFLAGS flags = BSDF_ALL_COMPONENTS, 
			 double *pdf = NULL, double *pdfRR = NULL);
  // Set pixel : sets the current pixel. This pixel will be sampled
  void SetPixel(int nx, int ny, CAMERA *cam = NULL);

protected:
  int m_nx, m_ny;
  double m_px, m_py;
  double m_h,m_v;
};

#endif // _PIXELSAMPLER_H_
