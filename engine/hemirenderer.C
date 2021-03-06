
#include <math.h>

#include "hemirenderer.H"
#include "vector.h"
#include "color.h"
#include "render.h"
#include "renderhook.h"

// Constructor : nr. of steps in phi and theta
CHemisphereRenderer::CHemisphereRenderer()
{
  m_phiSteps = m_thetaSteps = 0;
  m_renderData = NULL;
  m_renderEnabled = false;
}
  
CHemisphereRenderer::~CHemisphereRenderer(void)
{
  if(m_renderData != NULL)
  {
    EnableRendering(false);
    delete[] m_renderData;
    m_renderData = NULL;
  }
}

// AcquireData fills in the data on the hemisphere.
// The callback is called for every phi and theta value.

void CHemisphereRenderer::Initialize(int Nphi, int Ntheta, 
				     VECTOR center, COORDSYS *coordsys,
				     CHRAcquireCallback cb, void *cbData)
{
  VECTOR vec;
  RGB col;
  double curPhi, curTheta, dist;
  int t,p, index;

  // Copy parameters

  m_center = center;
  m_coordsys = *coordsys;

  // Allocate enough room for the data

  if((Nphi != m_phiSteps) || (Ntheta != m_thetaSteps))
  {
    // different resolution
    if(m_renderData != NULL)
    {
      delete[] m_renderData;
      m_renderData = NULL;
    }
    
    m_phiSteps = Nphi;
    m_thetaSteps = Ntheta;
    m_deltaPhi = 2.0 * M_PI / m_phiSteps;
    m_deltaTheta = 0.5 * M_PI / m_thetaSteps;
  }

  if(m_renderData == NULL)
  {
    m_renderData = new CHRVertexData[m_phiSteps * (m_thetaSteps - 1)];
  }


  // Now fill the data array

  curTheta = M_PI / 2.0;  // theta = 0 -> Z-axis = top of hemisphere
  index = 0;

  for(t = 0; t < m_thetaSteps - 1; t++)
  {
    curPhi = 0.0;

    for(p = 0; p < m_phiSteps; p++)
    {
      // Calculate WC vector

      SphericalCoordToVector(&m_coordsys, &curPhi, &curTheta, &vec);

      cb(&vec, &m_coordsys, curPhi, curTheta, cbData, &col, &dist);

      VECTORADDSCALED(m_center, dist, vec, m_renderData[index].point);
      VECTORCOPY(vec, m_renderData[index].normal);
      m_renderData[index].rgb = col;
      
      curPhi += m_deltaPhi;
      index++;
    }

    curTheta -= m_deltaTheta;
  }

  // Now fill in the top of the hemisphere

  curPhi = 0;
  curTheta = 0;

  SphericalCoordToVector(&m_coordsys, &curPhi, &curTheta, &vec);

  cb(&vec, &m_coordsys, curPhi, curTheta, cbData, &col, &dist);

  VECTORADDSCALED(m_center, dist, vec, m_top.point);
  VECTORCOPY(vec, m_top.normal);
  m_top.rgb = col;
}

// Render : renders the hemispherical data

void CHemisphereRenderer::Render(void)
{
  int t,p, index1, index2;

  if(m_renderData == NULL)
    return;

  index1 = 0; // First row
  index2 = m_phiSteps; // Second row

  for(t = 0; t < m_thetaSteps - 2; t++)
  {
    // Start a new triangle strip
    RenderBeginTriangleStrip();

    for(p = 0; p < m_phiSteps; p++)
    {
      RenderNextTrianglePoint(&m_renderData[index2].point,
			      &m_renderData[index2].rgb);
      RenderNextTrianglePoint(&m_renderData[index1].point,
			      &m_renderData[index1].rgb);
      index1++;
      index2++;
    }

    // Send first points again to close strip

    RenderNextTrianglePoint(&m_renderData[index1].point,
			    &m_renderData[index1].rgb);
    RenderNextTrianglePoint(&m_renderData[index1 - m_phiSteps].point,
			    &m_renderData[index1 - m_phiSteps].rgb);
    
    // End triangle strip

    RenderEndTriangleStrip();
  }

  // Now render the top of the hemisphere

  // Start a new triangle strip
  RenderBeginTriangleStrip();
  
  for(p = 0; p < m_phiSteps; p++)
  {
    RenderNextTrianglePoint(&m_top.point,
			    &m_top.rgb);
    RenderNextTrianglePoint(&m_renderData[index1].point,
			    &m_renderData[index1].rgb);
    index1++;
  }
  
  // Send first points again to close strip
  
  RenderNextTrianglePoint(&m_top.point,
			  &m_top.rgb);
  RenderNextTrianglePoint(&m_renderData[index1 - m_phiSteps].point,
			  &m_renderData[index1 - m_phiSteps].rgb);
  
  // End triangle strip
  
  RenderEndTriangleStrip();
}

// Callback needed for the render hook

static void RenderHemisphereHook(void *data)
{
  CHemisphereRenderer *hr = (CHemisphereRenderer *)data;

  hr->Render();
}


void CHemisphereRenderer::EnableRendering(bool flag)
{
  if(flag)
  {
    if(!m_renderEnabled)
    {
      AddRenderHook(RenderHemisphereHook, this);
    }
  }
  else
  {
    if(m_renderEnabled)
    {
      RemoveRenderHook(RenderHemisphereHook, this);
    }
  }

  m_renderEnabled = flag;
}

