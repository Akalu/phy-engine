
#include <math.h>

#include "lightlist.H"
#include "patch.h"
#include "color.h"
#include "error.h"



CLightList *gLightList = NULL;



double LightEvalPDFImportant(PATCH *light, VECTOR *lightPoint,
			     VECTOR *point, VECTOR *normal)
{
  return gLightList->EvalPDFImportant(light, lightPoint,
				      point, normal);
}





void CLightList::IncludeVirtualPatches(bool newValue)
{
  includeVirtual = newValue;
}

CLightList::CLightList(PATCHLIST *list, bool includeVirtualPatches)
{
  CLightInfo info;
  COLOR lightColor;
  {
    static int wgiv = 0;
    if (!wgiv) {
      Warning("CLightList::CLightList", "not yet ready for texturing");
      wgiv = 1;
    }
  }

  totalFlux = 0.0;
  lightCount = 0;
  includeVirtual = includeVirtualPatches;
  
  ForAllInList(PATCH, light, list)
    {
      if(!IsPatchVirtual(light) || includeVirtual)
	if(light->surface->material->edf != NULL)
	  {
	    info.light = light;

	    // calc emittedFlux
	    if(IsPatchVirtual(light))
	    {  
	      COLOR e = EdfEmittance(light->surface->material->edf, (HITREC *)NULL, DIFFUSE_COMPONENT);
	      info.emittedFlux = COLORAVERAGE(e);
	    }
	    else
	    {
	      lightColor = PatchAverageEmittance(light, DIFFUSE_COMPONENT);
	      info.emittedFlux = COLORAVERAGE(lightColor) * light->area;
	    }

	    totalFlux += info.emittedFlux;
	    lightCount++;
	    Append(info);
	  }
    }
  EndForAll;
}

CLightList::~CLightList(void)
{
  RemoveAll();
}


// Returns sampled patch, scales x_1 back to a random in 0..1

PATCH *CLightList::Sample(double *x_1, double *pdf)
{
  CLightInfo *info, *lastInfo;
  CTSList_Iter<CLightInfo> iterator(*this);

  double rnd = *x_1 * totalFlux;
  double currentSum;

  info = iterator.Next();
  while( (info != NULL) && (IsPatchVirtual(info->light)) && (!includeVirtual) ) info = iterator.Next();

  if(info == NULL)
  {
    Warning("CLightList::Sample", "No lights available");
    return NULL;
  }

  currentSum = info->emittedFlux;

  while((rnd > currentSum) && (info != NULL))
  {
    lastInfo = info;
    info = iterator.Next();
    while( (info != NULL) && (IsPatchVirtual(info->light)) && (!includeVirtual) ) info = iterator.Next();

    if(info != NULL)
    {
      currentSum += info->emittedFlux;
    }
    else
    {
      info = lastInfo;
      rnd = currentSum - 1.0; // :-(  Damn float inaccuracies
    }
  }

  if(info != NULL)
  {
    *x_1 = ((*x_1 - ((currentSum - info->emittedFlux) / totalFlux)) / 
	    (info->emittedFlux / totalFlux));
    *pdf = info->emittedFlux / totalFlux;
    return(info->light);
  }
  
  return NULL;
}

double CLightList::EvalPDF_virtual(PATCH *light, POINT *)
{
  // EvalPDF for virtual patches (see EvalPDF)
  double pdf;

  // Prob for choosing this light
  XXDFFLAGS all = DIFFUSE_COMPONENT || GLOSSY_COMPONENT || SPECULAR_COMPONENT;

  COLOR e = EdfEmittance(light->surface->material->edf, (HITREC *)NULL, all);
  pdf =  COLORAVERAGE(e) / totalFlux;

  return pdf;
}

double CLightList::EvalPDF_real(PATCH *light, POINT *)
{
  // Eval PDF for normal patches (see EvalPDF)
  COLOR col;
  double pdf;

  col = PatchAverageEmittance(light, DIFFUSE_COMPONENT);

  // Prob for choosing this light
  pdf = COLORAVERAGE(col) * light->area / totalFlux;

  return pdf;
}

double CLightList::EvalPDF(PATCH *light, POINT *point)
{
  // TODO!!!  1) patch should become class
  //          2) virtual patch should become child-class
  //          3) this method should be handled by specialisation
  if(totalFlux < EPSILON) return 0.0;
  if(IsPatchVirtual(light)) return EvalPDF_virtual(light, point);
  else return EvalPDF_real(light, point);
}  






double CLightList::ComputeOneLightImportance_virtual(PATCH *light,
						     const VECTOR *,
						     const VECTOR *,
						     float )
{
  // ComputeOneLightImportance for virtual patches
  XXDFFLAGS all = DIFFUSE_COMPONENT || GLOSSY_COMPONENT || SPECULAR_COMPONENT;

  COLOR e = EdfEmittance(light->surface->material->edf, (HITREC *)NULL, all);
  return COLORAVERAGE(e);
}  

double CLightList::ComputeOneLightImportance_real(PATCH *light,
						  const VECTOR *point, 
						  const VECTOR *normal,
						  float emittedFlux)
{
  // ComputeOneLightImportance for real patches
  int tried = 0;  // No points on the patch are tried yet
  int done = false;
  double contribution = 0.0; 
  VECTOR lightPoint, light_normal;
  VECTOR dir;
  double cosRayPatch, cosRayLight, dist2;

  while(!done && tried <= light->nrvertices)
  {
    // Choose a point on the patch according to 'tried'

    if(tried == 0)
    {
      lightPoint = light->midpoint;
      light_normal = light->normal;
    }
    else
    {
      lightPoint = *(light->vertex[tried-1]->point);
      if(light->vertex[tried-1]->normal != NULL)
	light_normal = *(light->vertex[tried-1]->normal);
      else
	light_normal = light->normal;
    }

    // Previous
    //    case 0: u = v = 0.5; break;
    //    case 1: u = v = 0.0; break;
    //    case 2: u = 1.0; v = 0.0; break;
    //    case 3: u = 0.0; v = 1.0; break;
    //    case 4: u = v = 1.0; break;
    //
    //      // TODO ? allow more vertices
    //    }
    //
    //    PatchPoint(light, u, v, &lightPoint);

    // Estimate the contribution

    // light_normal = PatchNormalAtUV(light, u, v);

    // ray direction (but no ray is shot of course)

    VECTORSUBTRACT(lightPoint, *point, dir);
    dist2 = VECTORNORM2(dir);
    // VECTORSCALE(1/dist, dir, dir);

    
    
    // Cosines have an addition dist length in them

    cosRayLight = -VECTORDOTPRODUCT(dir, light_normal);
    cosRayPatch = VECTORDOTPRODUCT(dir, *normal);
    
    if(cosRayLight > 0 && cosRayPatch > 0)
    {
      // Orientation of surfaces ok.
      // BRDF is not taken into account since
      // we're expecting diffuse/glossy surfaces here.

      contribution = (cosRayPatch * cosRayLight * emittedFlux / (M_PI * dist2));
      done = true;
    }
   
    tried++; // trie next point on light
  }

  return contribution;
}

double CLightList::ComputeOneLightImportance(PATCH *light,
					     const VECTOR *point,
					     const VECTOR *normal,
					     float emittedFlux)
{
  // TODO!!!  1) patch should become class
  //          2) virtual patch should become child-class
  //          3) this method should be handled by specialisation
  if(IsPatchVirtual(light)) return 
   ComputeOneLightImportance_virtual(light, point, normal, emittedFlux);
  else return 
   ComputeOneLightImportance_real(light, point, normal, emittedFlux);
  
}  

void CLightList::ComputeLightImportances(VECTOR *point, VECTOR *normal)
{
  if((VECTOREQUAL(*point, lastPoint, EPSILON)) && 
     (VECTOREQUAL(*normal, lastNormal, EPSILON)))
  {
    return; // Still ok !!
  }


  CLightInfo *info;
  CTSList_Iter<CLightInfo> iterator(*this);
  double imp;

  totalImp = 0.0;

  // next
  info = iterator.Next();
  while( (info != NULL) && (IsPatchVirtual(info->light)) && (!includeVirtual) ) info = iterator.Next();

  while(info)
  {
    imp = ComputeOneLightImportance(info->light, point, normal,
				    info->emittedFlux);
    totalImp += imp;
    info->importance = imp;

    // next
    info = iterator.Next();
    while( (info != NULL) && (IsPatchVirtual(info->light)) && (!includeVirtual) ) info = iterator.Next();
  }
}

PATCH *CLightList::SampleImportant(POINT *point, VECTOR *normal, 
				   double *x_1, double *pdf)
{
  CLightInfo *info, *lastInfo;
  CTSList_Iter<CLightInfo> iterator(*this);
  double rnd, currentSum;

  ComputeLightImportances(point, normal);

  if(totalImp == 0)
  {
    // No light is important, but we must return one (->optimize ?)
    return(Sample(x_1, pdf));
  }

  rnd = *x_1 * totalImp;

  // next
  info = iterator.Next();
  while( (info != NULL) && (IsPatchVirtual(info->light)) && (!includeVirtual) ) info = iterator.Next();

  if(info == NULL)
  {
    Warning("CLightList::Sample", "No lights available");
    return NULL;
  }

  currentSum = info->importance;

  while((rnd > currentSum) && (info != NULL))
  {
    lastInfo = info;

    // next
    info = iterator.Next();
    while( (info != NULL) && (IsPatchVirtual(info->light)) && (!includeVirtual) ) info = iterator.Next();

    if(info != NULL)
    {
      currentSum += info->importance;
    }
    else
    {
      info = lastInfo;
      rnd = currentSum - 1.0; // :-(  Damn float inaccuracies
    }
  }

  if(info != NULL)
  {
    *x_1 = ((*x_1 - ((currentSum - info->importance) / totalImp)) / 
	    (info->importance / totalImp));
    *pdf = info->importance / totalImp;
    return(info->light);
  }
  
  return NULL;
}

double CLightList::EvalPDFImportant(PATCH *light, POINT *,
				    POINT *litPoint, VECTOR *normal)
{
  double pdf;
  CLightInfo *info;
  CTSList_Iter<CLightInfo> iterator(*this);

  ComputeLightImportances(litPoint, normal);

  // Search the light in the list :-(

  while((info = iterator.Next()) && info->light != light);

  if(info == NULL)
  {
    Warning("CLightList::EvalPDFImportant", "Could not find light");
    return 0.0;
  }

  // Prob for choosing this light
  if (totalImp < EPSILON)
    pdf = 0.0;
  else
    pdf = info->importance / totalImp;

  return pdf;
}

