

#include "mcradP.h"
#include "edf.h"
#include "statistics.h"
#include "scene.h"
#include "localline.h"
#include "render.h"

static struct LIGHTSOURCETABLE {
  PATCH *patch;
  double flux;
} *lights;
static int nrlights, nrsamples;
static double totalflux;

static void InitLight(struct LIGHTSOURCETABLE* light, PATCH *l, double flux)
{
  light->patch = l;
  light->flux = flux;
}

static void MakeLightSourceTable(void)
{
  int i=0; totalflux = 0.;
  nrlights = nrlightsources;
  lights = (struct LIGHTSOURCETABLE*)Alloc(nrlights * sizeof(struct LIGHTSOURCETABLE));
  ForAllPatches(l, LightSourcePatches) {
    COLOR emitted_rad = PatchAverageEmittance(l, ALL_COMPONENTS);
    double flux = M_PI * l->area * COLORSUMABSCOMPONENTS(emitted_rad);
    totalflux += flux;
    InitLight(&lights[i], l, flux);
    i++;
  } EndForAll;
  ForAllPatches(P, Patches) {
    CLEARCOEFFICIENTS(RAD(P), BAS(P));
    CLEARCOEFFICIENTS(UNSHOT_RAD(P), BAS(P));
    CLEARCOEFFICIENTS(RECEIVED_RAD(P), BAS(P));
    COLORCLEAR(SOURCE_RAD(P));
  } EndForAll;
}

static void NextLightSample(PATCH* patch, double* zeta)
{
  double *xi = Sample4D(RAY_INDEX(patch)++);
  if (patch->nrvertices == 3) {
    double u = xi[0], v = xi[1];
    FoldSampleF(&u, &v);
    zeta[0] = u;
    zeta[1] = v;
  } else {
    zeta[0] = xi[0];
    zeta[1] = xi[1];
  }
  zeta[2] = xi[2];
  zeta[3] = xi[3];
}

static RAY SampleLightRay(PATCH* patch, COLOR *emitted_rad, double *point_selection_pdf, double *dir_selection_pdf)
{
  RAY ray;
  do {
    double zeta[4];
    HITREC hit;
    NextLightSample(patch, zeta);
  
    PatchUniformPoint(patch, zeta[0], zeta[1], &ray.pos);

    InitHit(&hit, patch, (GEOM*)0, &ray.pos, &patch->normal, patch->surface->material, 0.);
    ray.dir = EdfSample(patch->surface->material->edf, &hit, ALL_COMPONENTS, zeta[2], zeta[3], emitted_rad, dir_selection_pdf);
  } while (*dir_selection_pdf == 0.);

  // TODO: the following is only correct if no rejections would result in the
  // loop above, i.o.w. the surface is not textured, or it is textured, but there
  // are no areas that are non-selfemitting.
  *point_selection_pdf = 1./patch->area;  // uniform area sampling
  return ray;
}

static void SampleLight(struct LIGHTSOURCETABLE* light, double light_selection_pdf)
{
  COLOR rad;
  double point_selection_pdf, dir_selection_pdf;
  RAY ray = SampleLightRay(light->patch, &rad, &point_selection_pdf, &dir_selection_pdf);
  HITREC hitstore, *hit;

  mcr.traced_rays ++;
  hit = McrShootRay(light->patch, &ray, &hitstore);
  if (hit) {
    double pdf = light_selection_pdf * point_selection_pdf * dir_selection_pdf;
    double outcos = VECTORDOTPRODUCT(ray.dir, light->patch->normal);
    COLOR rcvrad, Rd = REFLECTANCE(hit->patch);
    COLORSCALE((outcos/(M_PI * hit->patch->area * pdf * nrsamples)), rad, rcvrad);
    COLORPROD(Rd, rcvrad, rcvrad);
    COLORADD(RAD(hit->patch)[0], rcvrad, RAD(hit->patch)[0]);
    COLORADD(UNSHOT_RAD(hit->patch)[0], rcvrad, UNSHOT_RAD(hit->patch)[0]);
    COLORADD(SOURCE_RAD(hit->patch), rcvrad, SOURCE_RAD(hit->patch));
  }
}

static void SampleLightSources(int nr_samples)
{
  double rnd = drand48();
  int count = 0, i;
  double p_cumul = 0.;
  nrsamples = nr_samples;
  fprintf(stderr, "Shooting %d light rays ", nrsamples); fflush(stderr);
  for (i=0; i<nrlights; i++) {
    int j;
    double p = lights[i].flux / totalflux;
    int samples_this_light = 
      (int)floor((p_cumul+p) * (double)nrsamples + rnd) - count;

    for (j=0; j<samples_this_light; j++)
      SampleLight(&lights[i], p);

    p_cumul += p;
    count += samples_this_light;
  }

  fputc('\n', stderr);
}

static void Summarize(void)
{
  COLORCLEAR(mcr.unshot_flux);	mcr.unshot_ymp = 0.;
  COLORCLEAR(mcr.total_flux);	mcr.total_ymp = 0.;
  COLORCLEAR(mcr.imp_unshot_flux);
  ForAllPatches(P, Patches) {
    COLORADDSCALED(mcr.unshot_flux, M_PI*P->area, UNSHOT_RAD(P)[0], mcr.unshot_flux);
    COLORADDSCALED(mcr.total_flux, M_PI*P->area, RAD(P)[0], mcr.total_flux);
#ifdef IDMCR
    COLORADDSCALED(mcr.imp_unshot_flux, M_PI*P->area*(IMP(P)-SOURCE_IMP(P)), UNSHOT_RAD(P)[0], mcr.imp_unshot_flux);
    mcr.unshot_ymp += P->area * fabs(UNSHOT_IMP(P));
    mcr.total_ymp += P->area * IMP(P);
    mcr.source_ymp += P->area * SOURCE_IMP(P);
#endif
    McrPatchComputeNewColor(P);
  } EndForAll;
}

void DoNonDiffuseFirstShot(void)
{
  MakeLightSourceTable();
  SampleLightSources(mcr.initial_ls_samples * nrlights);
  Summarize();
  RenderScene();
}
