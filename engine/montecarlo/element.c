

#include "mcradP.h"
#include "hierarchy.h"
#include "niederreiter.h"
#include "statistics.h"
#include "vertex.h"
#include "patch.h"
#include "geom.h"
#include "pools.h"
#include "error.h"
#include "render.h"

#ifndef NOPOOLS

static POOL *elemPool = (POOL *)NULL;
#define NEW_ELEM()	(ELEMENT *)NewPoolCell(sizeof(ELEMENT), 0, "MONTECARLO elements", &elemPool)
#define DISPOSE_ELEM(elem) Dispose((char *)elem, &elemPool)
static POOL *regsubelptrPool = (POOL *)NULL;
#define NEWREGULARSUBELEMENTPTRS()   (ELEMENT **)NewPoolCell(4*sizeof(ELEMENT *), 0, "MONTECARLO regular subels", &regsubelptrPool)
#define DISPOSEREGULARSUBELEMENTPTRS(ptrs)   Dispose((char *)ptrs, &regsubelptrPool)

#else 

#define NEW_ELEM()	(ELEMENT *)Alloc(sizeof(ELEMENT))
#define DISPOSE_ELEM(elem) Free((char *)elem, sizeof(ELEMENT))
#define NEWREGULARSUBELEMENTPTRS() (ELEMENT **)Alloc(4*sizeof(ELEMENT *))
#define DISPOSEREGULARSUBELEMENTPTRS(ptrs) Free((char *)ptrs, 4*sizeof(ELEMENT *))

#endif 

static ELEMENT *CreateElement(void)
{
  static long id = 1;
  ELEMENT *elem = NEW_ELEM();

  elem->pog.patch = (PATCH *)NULL;
  elem->id = id; id++;
  elem->area = 0.;
  InitCoefficients(elem);	

  COLORCLEAR(elem->Ed); COLORCLEAR(elem->Rd);

  elem->ray_index = 0;
  elem->quality = 0;
  elem->ng = 0.;

#ifdef IDMCR
  elem->imp = elem->unshot_imp = elem->source_imp = 0.;
  elem->imp_ray_index = 0;
#endif

#ifdef HMC
  VECTORSET(elem->midpoint, 0., 0., 0.);
  elem->vertex[0] = elem->vertex[1] = elem->vertex[2] = elem->vertex[3] = (VERTEX *)NULL;
  elem->parent = (ELEMENT *)NULL;
  elem->regular_subelements = (ELEMENT **)NULL;
  elem->irregular_subelements = (ELEMENTLIST *)NULL;
  elem->uptrans = (TRANSFORM2D *)NULL;
  elem->child_nr = -1;
  elem->nrvertices = 0;
  elem->iscluster = FALSE;
  elem->flags = 0;
#endif

  hierarchy.nr_elements++;

  return elem;
}

static void VertexAttachElement(VERTEX *v, ELEMENT *elem)
{
  v->radiance_data = ElementListAdd(v->radiance_data, elem);
}

static void VertexDetachElement(VERTEX *v, ELEMENT *elem)
{
  v->radiance_data = ElementListRemove(v->radiance_data, elem);
}

ELEMENT *CreateToplevelSurfaceElement(PATCH *patch)
{
  int i;
  ELEMENT *elem = CreateElement();
  elem->pog.patch = patch;
  elem->iscluster = FALSE;
  elem->area = patch->area;
  elem->midpoint = patch->midpoint;
  elem->nrvertices = patch->nrvertices;
  for (i=0; i<elem->nrvertices; i++) {
    elem->vertex[i] = patch->vertex[i];
    VertexAttachElement(elem->vertex[i], elem);
  }

  AllocCoefficients(elem);	
  CLEARCOEFFICIENTS(elem->rad, elem->basis);
  CLEARCOEFFICIENTS(elem->unshot_rad, elem->basis);
  CLEARCOEFFICIENTS(elem->received_rad, elem->basis);

  elem->Ed = PatchAverageEmittance(patch, DIFFUSE_COMPONENT);
  COLORSCALEINVERSE(M_PI, elem->Ed, elem->Ed);
  elem->Rd = PatchAverageNormalAlbedo(patch, BRDF_DIFFUSE_COMPONENT);

  return elem;
}

static ELEMENT *CreateCluster(GEOM *geom)
{
  ELEMENT *elem = CreateElement();
  float *bounds = geom->bounds;

  elem->pog.geom = geom;
  elem->iscluster = TRUE;

  COLORSETMONOCHROME(elem->Rd, 1.);
  COLORCLEAR(elem->Ed);

  
  VECTORSET(elem->midpoint,
	    (bounds[MIN_X] + bounds[MAX_X])/2., 
	    (bounds[MIN_Y] + bounds[MAX_Y])/2., 
	    (bounds[MIN_Z] + bounds[MAX_Z])/2.);

  AllocCoefficients(elem);	
  CLEARCOEFFICIENTS(elem->rad, elem->basis);
  CLEARCOEFFICIENTS(elem->unshot_rad, elem->basis);
  CLEARCOEFFICIENTS(elem->received_rad, elem->basis);
#ifdef IDMCR
  elem->imp = elem->unshot_imp = elem->received_imp = 0.;
#endif 

  hierarchy.nr_clusters++;

  return elem;
}

static void CreateSurfaceElementChild(PATCH *patch, ELEMENT *parent)
{
  ELEMENT *elem = patch->radiance_data;	
  elem->parent = parent;
  parent->irregular_subelements = ElementListAdd(parent->irregular_subelements, elem);
}

static ELEMENT *CreateClusterHierarchyRecursive(GEOM *world);
static void CreateClusterChild(GEOM *geom, ELEMENT *parent)
{
  ELEMENT *subcluster = CreateClusterHierarchyRecursive(geom);
  subcluster->parent = parent;
  parent->irregular_subelements = ElementListAdd(parent->irregular_subelements, subcluster);
}


static void InitClusterPull(ELEMENT *parent, ELEMENT *child)
{
  parent->area += child->area;
  PullRadiance(parent, child, parent->rad, child->rad);
  PullRadiance(parent, child, parent->unshot_rad, child->unshot_rad);
  PullRadiance(parent, child, parent->received_rad, child->received_rad);
#ifdef IDMCR
  PullImportance(parent, child, &parent->imp, &child->imp);
  PullImportance(parent, child, &parent->unshot_imp, &child->unshot_imp);
  PullImportance(parent, child, &parent->received_imp, &child->received_imp);
#endif
  
  
  COLORADDSCALED(parent->Ed, child->area, child->Ed, parent->Ed);
}

static void CreateClusterChildren(ELEMENT *parent)
{
  GEOM *geom = parent->pog.geom;

  if (GeomIsAggregate(geom)) {
    ForAllGeoms(childgeom, GeomPrimList(geom)) {
      CreateClusterChild(childgeom, parent);
    } EndForAll;
  } else {
    ForAllPatches(childpatch, GeomPatchList(geom)) {
      CreateSurfaceElementChild(childpatch, parent);
    } EndForAll;
  }

  ForAllIrregularSubelements(child, parent) {
    InitClusterPull(parent, child);
  } EndForAll;
  COLORSCALEINVERSE(parent->area, parent->Ed, parent->Ed);
}

static ELEMENT *CreateClusterHierarchyRecursive(GEOM *world)
{
  ELEMENT *topcluster;
  world->radiance_data = topcluster = (void *)CreateCluster(world);
  CreateClusterChildren(topcluster);
  return topcluster;
}

ELEMENT *CreateClusterHierarchy(GEOM *world)
{
  if (!world) 
    return (ELEMENT *)NULL;
  else
    return CreateClusterHierarchyRecursive(world);
}




TRANSFORM2D quadupxfm[4] = {
  
  {{{0.5 , 0.0}, {0.0 , 0.5}} , {0.0 , 0.0}},

  
  {{{0.5 , 0.0}, {0.0 , 0.5}} , {0.5 , 0.0}},
  
  
  {{{0.5 , 0.0}, {0.0 , 0.5}} , {0.0 , 0.5}},
  
  
  {{{0.5 , 0.0}, {0.0 , 0.5}} , {0.5 , 0.5}},
};


TRANSFORM2D triupxfm[4] = {
  
  {{{ 0.5 , 0.0}, {0.0 , 0.5}}, {0.0 , 0.0}},

  
  {{{ 0.5 , 0.0}, {0.0 , 0.5}}, {0.5 , 0.0}},

  
  {{{ 0.5 , 0.0}, {0.0 , 0.5}}, {0.0 , 0.5}},
  
  
  {{{-0.5 , 0.0}, {0.0 ,-0.5}}, {0.5 , 0.5}},
};


void ElementRange(ELEMENT *elem, int *nbits, niedindex *msb1, niedindex *rmsb2)
{
  int nb;
  niedindex b1, b2;

  nb = b1 = b2 = 0;
  while (elem->child_nr >= 0) {
    nb++;
    b1 = (b1 << 1) |  (niedindex)(elem->child_nr & 1);
    b2 = (b2 >> 1) | ((niedindex)(elem->child_nr & 2) << (NBITS-2)); 
    elem = elem->parent;
  }

  *nbits = nb; *msb1 = b1; *rmsb2 = b2;
}


ELEMENT *RegularSubelementAtPoint(ELEMENT *parent, double *u, double *v)
{
  ELEMENT *child = (ELEMENT *)NULL;
  double _u=*u, _v=*v;

  if (parent->iscluster || !parent->regular_subelements)
    return (ELEMENT *)NULL;

  
  switch (parent->nrvertices) {
  case 3:
    if (_u+_v <= 0.5) {
      child = parent->regular_subelements[0];
      *u = _u*2.; *v = _v*2.;
    } else if (_u > 0.5) {
      child = parent->regular_subelements[1];
      *u = (_u-0.5)*2.; *v = _v*2.;
    } else if (_v > 0.5) {
      child = parent->regular_subelements[2];
      *u = _u*2.; *v = (_v-0.5)*2.;
    } else {
      child = parent->regular_subelements[3];
      *u = (0.5-_u)*2.; *v = (0.5-_v)*2.;
    }
    break;
  case 4:
    if (_v <= 0.5) {
      if (_u < 0.5) {
	child = parent->regular_subelements[0];
	*u = _u * 2.;
      } else {
	child = parent->regular_subelements[1];
	*u = (_u-0.5) * 2.;
      }
      *v = _v * 2.;
    } else {
      if (_u < 0.5) {
	child = parent->regular_subelements[2];
	*u = _u * 2.;
      } else {
	child = parent->regular_subelements[3];
	*u = (_u-0.5) * 2.;
      }
      *v = (_v-0.5) * 2.;
    }
    break;
  default:
    Fatal(-1, "RegularSubelementAtPoint", "Can handle only triangular or quadrilateral elements");
  }

  return child;
}


ELEMENT *RegularLeafElementAtPoint(ELEMENT *top, double *u, double *v)
{
  ELEMENT *leaf;

  
  leaf = top;
  while (leaf->regular_subelements) {
    leaf = RegularSubelementAtPoint(leaf, u, v);
  }

  return leaf;
}

static VECTOR *InstallCoordinate(VECTOR *coord)
{
  VECTOR *v = VectorCreate(coord->x, coord->y, coord->z);
  hierarchy.coords = VectorListAdd(hierarchy.coords, v);
  return v;
}

static VECTOR *InstallNormal(VECTOR *norm)
{
  VECTOR *v = VectorCreate(norm->x, norm->y, norm->z);
  hierarchy.normals = VectorListAdd(hierarchy.normals, v);
  return v;
}

static VECTOR *InstallTexCoord(VECTOR *texCoord)
{
  VECTOR *t = VectorCreate(texCoord->x, texCoord->y, texCoord->z);
  hierarchy.texCoords = VectorListAdd(hierarchy.texCoords, t);
  return t;
}

static VERTEX *InstallVertex(VECTOR *coord, VECTOR *norm, VECTOR *texCoord)
{
  VERTEX *v = VertexCreate(coord, norm, texCoord, (PATCHLIST *)NULL);
  hierarchy.vertices = VertexListAdd(hierarchy.vertices, v);
  return v;
}

static VERTEX *NewMidpointVertex(ELEMENT *elem, VERTEX *v1, VERTEX *v2)
{
  VECTOR coord, norm, texCoord, *p, *n, *t;

  MIDPOINT(*(v1->point), *(v2->point), coord);
  p = InstallCoordinate(&coord);

  if (v1->normal && v2->normal) {
    MIDPOINT(*(v1->normal), *(v2->normal), norm);
    n = InstallNormal(&norm);
  } else
    n = &elem->pog.patch->normal;

  if (v1->texCoord && v2->texCoord) {
    MIDPOINT(*(v1->texCoord), *(v2->texCoord), texCoord);
    t = InstallTexCoord(&texCoord);
  } else
    t = NULL;

  return InstallVertex(p, n, t);
}


static ELEMENT *ElementNeighbour(ELEMENT *elem, int edgenr)
{
  VERTEX *from = elem->vertex[edgenr],
         *to = elem->vertex[(edgenr+1)%elem->nrvertices];

  ForAllElements(e, to->radiance_data) {
    if (e != elem &&
       ((e->nrvertices == 3 && 
	 ((e->vertex[0] == to && e->vertex[1] == from) ||
	  (e->vertex[1] == to && e->vertex[2] == from) ||
	  (e->vertex[2] == to && e->vertex[0] == from))) 
     || (e->nrvertices == 4 && 
	 ((e->vertex[0] == to && e->vertex[1] == from) ||
	  (e->vertex[1] == to && e->vertex[2] == from) ||
	  (e->vertex[2] == to && e->vertex[3] == from) ||
	  (e->vertex[3] == to && e->vertex[0] == from))))) return e;
  } EndForAll;

  return (ELEMENT *)NULL;
}

VERTEX *EdgeMidpointVertex(ELEMENT *elem, int edgenr)
{
  VERTEX *v = (VERTEX *)NULL,
         *to = elem->vertex[(edgenr+1)%elem->nrvertices];
  ELEMENT *neighbour = ElementNeighbour(elem, edgenr);

  if (neighbour && neighbour->regular_subelements) {
    
    int index = (to == neighbour->vertex[0] ? 0 :
		(to == neighbour->vertex[1] ? 1 :
		(to == neighbour->vertex[2] ? 2 :
		(to == neighbour->vertex[3] ? 3 : -1))));

    switch (neighbour->nrvertices) {
    case 3:
      switch (index) {
      case 0: v = neighbour->regular_subelements[0]->vertex[1]; break;
      case 1: v = neighbour->regular_subelements[1]->vertex[2]; break;

      case 2: v = neighbour->regular_subelements[2]->vertex[0]; break;
      default: Error("EdgeMidpointVertex", "Invalid vertex index %d", index); 
      }
      break;
    case 4:
      switch (index) {
      case 0: v = neighbour->regular_subelements[0]->vertex[1]; break;
      case 1: v = neighbour->regular_subelements[1]->vertex[2]; break;
      case 2: v = neighbour->regular_subelements[3]->vertex[3]; break;
      case 3: v = neighbour->regular_subelements[2]->vertex[0]; break;
      default: Error("EdgeMidpointVertex", "Invalid vertex index %d", index); 
      }
      break;
    default:
      Fatal(-1, "EdgeMidpointVertex", "only triangular and quadrilateral elements are supported");
    }
  }

  return v;
}

static VERTEX *NewEdgeMidpointVertex(ELEMENT *elem, int edgenr)
{
  VERTEX *v = EdgeMidpointVertex(elem, edgenr);
  if (!v) { 
    VERTEX *from = elem->vertex[edgenr],
           *to = elem->vertex[(edgenr+1)%elem->nrvertices];
    v = NewMidpointVertex(elem, from, to);
  }
  return v;
}

static VECTOR ElementMidpoint(ELEMENT *elem)
{
  int i;
  VECTORSET(elem->midpoint, 0., 0., 0.);
  for (i=0; i<elem->nrvertices; i++) {
    VECTORADD(elem->midpoint, *elem->vertex[i]->point, elem->midpoint);
  }
  VECTORSCALEINVERSE((float)elem->nrvertices, elem->midpoint, elem->midpoint);

  return elem->midpoint;
}

int ElementIsTextured(ELEMENT *elem)
{
  MATERIAL *mat;
  if (elem->iscluster) {
    Fatal(-1, "ElementIsTextured", "this routine should not be called for cluster elements");
    return FALSE;
  }
  mat = elem->pog.patch->surface->material;
  return BsdfIsTextured(mat->bsdf) || EdfIsTextured(mat->edf);
}

float ElementScalarReflectance(ELEMENT *elem)
{
  float rd;

  if (elem->iscluster)
    return 1.0;

  rd = COLORMAXCOMPONENT(elem->Rd);
  if (rd < EPSILON) rd = EPSILON;	
  return rd;                   
}


static void ElementComputeAverageReflectanceAndEmittance(ELEMENT *elem)
{
  PATCH *patch = elem->pog.patch;
  int i, nrsamples, istextured;
  int nbits;
  niedindex msb1, rmsb2, n;
  COLOR albedo, emittance;
  HITREC hit;
  InitHit(&hit, patch, NULL, &patch->midpoint, &patch->normal, patch->surface->material, 0.);
  
  istextured = ElementIsTextured(elem);
  nrsamples = istextured ? 100 : 1;
  COLORCLEAR(albedo); COLORCLEAR(emittance);
  ElementRange(elem, &nbits, &msb1, &rmsb2);
#ifdef NEVER
  fprintf(stderr, "%s:%d: elem = ", __FILE__ , __LINE__);
  PrintElement(stderr, elem);
  fprintf(stderr, "\n");
#endif

  n = 1;
  for (i=0; i<nrsamples; i++, n++) {
    COLOR sample;
    niedindex *xi = NextNiedInRange(&n, +1, nbits, msb1, rmsb2);
    hit.uv.u = (double)xi[0] * RECIP;
    hit.uv.v = (double)xi[1] * RECIP;
    hit.flags |= HIT_UV;
    PatchUniformPoint(patch, hit.uv.u, hit.uv.v, &hit.point);
    if (patch->surface->material->bsdf) {
      sample = BsdfScatteredPower(patch->surface->material->bsdf, &hit, &patch->normal, BRDF_DIFFUSE_COMPONENT);
      COLORADD(albedo, sample, albedo);
#ifdef NEVER
      fprintf(stderr, "%s:%d: n=%lld, u=%g,v=%g, point=", __FILE__, __LINE__, n, hit.uv.u, hit.uv.v);
      VectorPrint(stderr, hit.point);
      fprintf(stderr, ", sample=");
      ColorPrint(stderr, sample);
      fprintf(stderr, "\n");
#endif
    }
    if (patch->surface->material->edf) {
      sample = EdfEmittance(patch->surface->material->edf, &hit, DIFFUSE_COMPONENT);
      COLORADD(emittance, sample, emittance);
    }
  }
  COLORSCALEINVERSE((float)nrsamples, albedo, elem->Rd);
  COLORSCALEINVERSE((float)nrsamples, emittance, elem->Ed);
}


static void InitSurfacePush(ELEMENT *parent, ELEMENT *child)
{
  child->source_rad = parent->source_rad;
  PushRadiance(parent, child, parent->rad, child->rad);
  PushRadiance(parent, child, parent->unshot_rad, child->unshot_rad);
#ifdef IDMCR
  PushImportance(parent, child, &parent->imp, &child->imp);
  PushImportance(parent, child, &parent->source_imp, &child->source_imp);
  PushImportance(parent, child, &parent->unshot_imp, &child->unshot_imp);
#endif
  child->ray_index = parent->ray_index;
  child->quality = parent->quality;
  child->prob = parent->prob * child->area/parent->area;

  child->Rd = parent->Rd;
  child->Ed = parent->Ed;
  ElementComputeAverageReflectanceAndEmittance(child);
}


static ELEMENT *CreateSurfaceSubelement(ELEMENT *parent, int childnr, 
					VERTEX *v0, VERTEX *v1, VERTEX *v2, VERTEX *v3)
{
  int i;

  ELEMENT *elem = CreateElement();
  parent->regular_subelements[childnr] = elem;

  elem->pog.patch = parent->pog.patch;
  elem->nrvertices = parent->nrvertices;
  elem->vertex[0] = v0;
  elem->vertex[1] = v1;
  elem->vertex[2] = v2;
  elem->vertex[3] = v3;
  for (i=0; i<elem->nrvertices; i++)
    VertexAttachElement(elem->vertex[i], elem);

  elem->area = 0.25 * parent->area; 
  elem->midpoint = ElementMidpoint(elem);
  
  elem->parent = parent;
  elem->child_nr = childnr;
  elem->uptrans = elem->nrvertices == 3 ? &triupxfm[childnr] : &quadupxfm[childnr];

  AllocCoefficients(elem);
  CLEARCOEFFICIENTS(elem->rad, elem->basis);
  CLEARCOEFFICIENTS(elem->unshot_rad, elem->basis);
  CLEARCOEFFICIENTS(elem->received_rad, elem->basis);
#ifdef IDMCR
  elem->imp = elem->unshot_imp = elem->received_imp = 0.;
#endif 
  InitSurfacePush(parent, elem);

  return elem;
}


static ELEMENT **RegularSubdivideTriangle(ELEMENT *element)
{
  VERTEX *v0, *v1, *v2, *m0, *m1, *m2;
  
  v0 = element->vertex[0];
  v1 = element->vertex[1];
  v2 = element->vertex[2];
  m0 = NewEdgeMidpointVertex(element, 0);
  m1 = NewEdgeMidpointVertex(element, 1);
  m2 = NewEdgeMidpointVertex(element, 2);
  
  CreateSurfaceSubelement(element, 0, v0, m0, m2, NULL);
  CreateSurfaceSubelement(element, 1, m0, v1, m1, NULL);
  CreateSurfaceSubelement(element, 2, m2, m1, v2, NULL);
  CreateSurfaceSubelement(element, 3, m1, m2, m0, NULL);

#ifndef NO_SUBDIVISION_LINES  
  RenderSetColor(&renderopts.outline_color);
  RenderLine(v0->point, v1->point);
  RenderLine(v1->point, v2->point);
  RenderLine(v2->point, v0->point);
  RenderLine(m0->point, m1->point);
  RenderLine(m1->point, m2->point);
  RenderLine(m2->point, m0->point);
#endif
  return element->regular_subelements;
}

static ELEMENT **RegularSubdivideQuad(ELEMENT *element)
{
  VERTEX *v0, *v1, *v2, *v3, *m0, *m1, *m2, *m3, *mm;
  
  v0 = element->vertex[0];
  v1 = element->vertex[1];
  v2 = element->vertex[2];
  v3 = element->vertex[3];
  m0 = NewEdgeMidpointVertex(element, 0);
  m1 = NewEdgeMidpointVertex(element, 1);
  m2 = NewEdgeMidpointVertex(element, 2);
  m3 = NewEdgeMidpointVertex(element, 3);
  mm = NewMidpointVertex(element, m0, m2);
  
  CreateSurfaceSubelement(element, 0, v0, m0, mm, m3);
  CreateSurfaceSubelement(element, 1, m0, v1, m1, mm);
  CreateSurfaceSubelement(element, 2, m3, mm, m2, v3);
  CreateSurfaceSubelement(element, 3, mm, m1, v2, m2);
  
#ifndef NO_SUBDIVISION_LINES  
  RenderSetColor(&renderopts.outline_color);
  RenderLine(v0->point, v1->point);
  RenderLine(v1->point, v2->point);
  RenderLine(v2->point, v3->point);
  RenderLine(v3->point, v0->point);
  RenderLine(m0->point, m2->point);
  RenderLine(m1->point, m3->point);
#endif
  return element->regular_subelements;
}


ELEMENT **RegularSubdivideElement(ELEMENT *element)
{
  if (element->regular_subelements)
    return element->regular_subelements;

  if (element->iscluster) {
    Fatal(-1, "RegularSubdivideElement", "Cannot regularly subdivide cluster elements");
    return (ELEMENT **)NULL;
  }

  if (element->pog.patch->jacobian) {
    static int wgiv = FALSE;
    if (!wgiv)
      Warning("RegularSubdivideElement", "irregular quadrilateral patches are not correctly handled (but you probably won't notice it)");
    wgiv = TRUE;
  }

  
  element->regular_subelements = NEWREGULARSUBELEMENTPTRS(); 
  switch (element->nrvertices) {
  case 3: RegularSubdivideTriangle(element); break;
  case 4: RegularSubdivideQuad(element); break;
  default:
    Fatal(-1, "RegularSubdivideElement", "invalid element: not 3 or 4 vertices");
  }
  return element->regular_subelements;
}

static void DestroyElement(ELEMENT *elem)
{
  int i;
  if (!elem) return;

  if (elem->iscluster) hierarchy.nr_clusters--;
  hierarchy.nr_elements--;

  if (elem->irregular_subelements) ElementListDestroy(elem->irregular_subelements);
  if (elem->regular_subelements) DISPOSEREGULARSUBELEMENTPTRS(elem->regular_subelements);
  for (i=0; i<elem->nrvertices; i++) {
    ElementListDestroy(elem->vertex[i]->radiance_data);
    elem->vertex[i]->radiance_data = NULL;
  }
  DisposeCoefficients(elem);
  DISPOSE_ELEM(elem);
}

static void DestroySurfaceElement(ELEMENT *elem)
{
  if (!elem) return;
  ForAllRegularSubelements(child, elem) {
    DestroySurfaceElement(child);
  } EndForAll;
  DestroyElement(elem);
}

void DestroyToplevelSurfaceElement(ELEMENT *elem)
{
  DestroySurfaceElement(elem);
}

void DestroyClusterHierarchy(ELEMENT *top)
{
  if (!top || !top->iscluster) return;
  ForAllIrregularSubelements(child, top) {
    if (child->iscluster) DestroyClusterHierarchy(child);
  } EndForAll;
  DestroyElement(top);
}

static void TestPrintVertex(FILE *out, int i, VERTEX *v)
{
  fprintf(out, "vertex[%d]: %s", i, v ? "" : "(nil)");
  if (v) VertexPrint(out, v);
  fprintf(out, "\n");
}

static void PrintParents(FILE *out, ELEMENT *elem)
{
  if (!elem->parent) {
    fprintf(out, "(top)");
  } else {
    while (elem->parent) {
      fprintf(out, "%ld (child %d) ",
	      elem->parent->id, elem->child_nr);
      elem = elem->parent;
    }
  }
  fprintf(out, "\n");
}

void PrintElement(FILE *out, ELEMENT *elem)
{
  fprintf(out, "Element id %ld:\n", elem->id);
  
  fprintf(out, "Vertices: ");
  TestPrintVertex(out, 0, elem->vertex[0]);
  TestPrintVertex(out, 1, elem->vertex[1]);
  TestPrintVertex(out, 2, elem->vertex[2]);
  TestPrintVertex(out, 3, elem->vertex[3]);
  PrintBasis(elem->basis);

  if (!elem->iscluster) {
    int nbits;
    niedindex msb1, rmsb2;
    fprintf(out, "Surface element: material '%s', reflectosity = %g, self-emitted luminsoity = %g\n",
	    elem->pog.patch->surface->material->name,
	    ColorGray(elem->Rd),
	    ColorLuminance(elem->Ed));
    ElementRange(elem, &nbits, &msb1, &rmsb2);
    fprintf(out, "Element range: %d bits, msb1 = %016llx, rmsb2 = %016llx\n", nbits, msb1, rmsb2);
  }
  fprintf(out, "rad = "); PRINTRAD(out, elem->rad, elem->basis);
  fprintf(out, ", luminosity = %g\n", ColorLuminance(elem->rad[0])*M_PI);

  fprintf(out, "unshot rad = "); PRINTRAD(out, elem->unshot_rad, elem->basis);
  fprintf(out, ", luminosity = %g\n", ColorLuminance(elem->unshot_rad[0])*M_PI);
  fprintf(out, "received rad = "); PRINTRAD(out, elem->received_rad, elem->basis);
  fprintf(out, ", luminosity = %g\n", ColorLuminance(elem->received_rad[0])*M_PI);
  fprintf(out, "source rad = "); ColorPrint(out, elem->source_rad);
  fprintf(out, ", luminosity = %g\n", ColorLuminance(elem->source_rad)*M_PI);
  fprintf(out, "ray index = %d\n", (unsigned)elem->ray_index);
#ifdef IDMCR
  fprintf(out, "imp = %g, unshot_imp = %g, received_imp = %g, source_imp = %g\n", 
	  elem->imp, elem->unshot_imp, elem->received_imp, elem->source_imp);
  fprintf(out, "importance ray index = %d\n", (unsigned)elem->imp_ray_index);
#endif
  fprintf(out, "prob = %g, quality factor = %g\nng = %g\n",
	  elem->prob, elem->quality, elem->ng);
}


int ForAllChildrenElements(ELEMENT *top, void (*func)(ELEMENT *))
{
  if (!top) return FALSE;

  if (top->iscluster) {
    ForAllIrregularSubelements(p, top)
      func(p);
    EndForAll;
    return TRUE;
  } else if (top->regular_subelements) {
    ForAllRegularSubelements(p, top)
      func(p);
    EndForAll;
    return TRUE;
  } else {     
    return FALSE;
  }
}

void ForAllLeafElements(ELEMENT *top, void (*func)(ELEMENT *))
{
  if (!top) return;

  if (top->iscluster) {
    ForAllIrregularSubelements(p, top)
      ForAllLeafElements(p, func);
    EndForAll;
  } else if (top->regular_subelements) {
    ForAllRegularSubelements(p, top)
      ForAllLeafElements(p, func);
    EndForAll;
  } else
    func(top);
}

void ForAllClusterSurfaces(ELEMENT *top,
			      void (*func)(ELEMENT *))
{
  REC_ForAllClusterSurfaces(surf, top) {
    func(surf);
  } REC_EndForAllClusterSurfaces;
}

void ForAllSurfaceLeafs(ELEMENT *top, 
			   void (*func)(ELEMENT *))
{
  REC_ForAllSurfaceLeafs(leaf, top) {
    func(leaf);
  } REC_EndForAllSurfaceLeafs;
}

int ElementIsLeaf(ELEMENT *elem)
{
  return (!elem->regular_subelements && !elem->irregular_subelements);
}


int ElementVertices(ELEMENT *elem, POINT *p)
{
  if (elem->iscluster) {
    float *vol = GeomBounds(elem->pog.geom);

    VECTORSET(p[0], vol[MIN_X], vol[MIN_Y], vol[MIN_Z]);
    VECTORSET(p[1], vol[MIN_X], vol[MIN_Y], vol[MAX_Z]);
    VECTORSET(p[2], vol[MIN_X], vol[MAX_Y], vol[MIN_Z]);
    VECTORSET(p[3], vol[MIN_X], vol[MAX_Y], vol[MAX_Z]);
    VECTORSET(p[4], vol[MAX_X], vol[MIN_Y], vol[MIN_Z]);
    VECTORSET(p[5], vol[MAX_X], vol[MIN_Y], vol[MAX_Z]);
    VECTORSET(p[6], vol[MAX_X], vol[MAX_Y], vol[MIN_Z]);
    VECTORSET(p[7], vol[MAX_X], vol[MAX_Y], vol[MAX_Z]);

    return 8;
  } else {
    ForAllVerticesOfElement(v, elem) {
      *p++ = *(v->point);
    } EndForAll;
    return elem->nrvertices;
  }
}


float *ElementBounds(ELEMENT *elem, float *bounds)
{
  if (elem->iscluster) 
    BoundsCopy(elem->pog.geom->bounds, bounds);
  else if (!elem->uptrans) 
    PatchBounds(elem->pog.patch, bounds);
  else {
    BoundsInit(bounds);
    ForAllVerticesOfElement(v, elem) {
      BoundsEnlargePoint(bounds, v->point);
    } EndForAll;
  }
  return bounds;
}

ELEMENT *ClusterChildContainingElement(ELEMENT *parent, ELEMENT *descendant)
{
  while (descendant && descendant->parent != parent)
    descendant = descendant->parent;
  if (!descendant)
    Fatal(-1, "ClusterChildContainingElement", "descendant is not a descendant of parent");
  return descendant;
}

