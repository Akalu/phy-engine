/* bsdfcomp.H
 * Some bsdf component stuff.
 */

#ifndef _BSDFCOMP_H_
#define _BSDFCOMP_H_

class CBsdfComp
{
  public:

    // Only storage is this !
    COLOR comp[BSDFCOMPONENTS];

    // Methods

    // Default constructor/destructor

    // Element access

    inline COLOR& operator[](int index)
    {
      return comp[index];
    }

    // Conversion to COLOR *

    inline operator COLOR*() { return comp; }

    void Clear(const BSDFFLAGS flags = BSDF_ALL_COMPONENTS)
    {
      for(int i = 0; i < BSDFCOMPONENTS; i++)
	if(flags & (BSDF_INDEXTOCOMP(i)))
	  COLORCLEAR(comp[i]);
    }

    void Fill(const COLOR col, const BSDFFLAGS flags = BSDF_ALL_COMPONENTS)
    {
      for(int i = 0; i < BSDFCOMPONENTS; i++)
	if(flags & (BSDF_INDEXTOCOMP(i)))
	  comp[i] = col;
    }
  
    COLOR Sum(const BSDFFLAGS flags = BSDF_ALL_COMPONENTS)
    {
      COLOR result;
      
      COLORCLEAR(result);
      
      for(int i = 0; i < BSDFCOMPONENTS; i++)
      {
	if(flags & (BSDF_INDEXTOCOMP(i)))
	{
	  COLORADD(result, comp[i], result);
	}
      }

      return result;
    }

    void Print(void)
    {
      printf("D R %6g G %6g S %6g T R %6g G %6g S %6g\n",
	     COLORAVERAGE(comp[BRDF_DIFFUSE_INDEX]), 
	     COLORAVERAGE(comp[BRDF_GLOSSY_INDEX]), 
	     COLORAVERAGE(comp[BRDF_SPECULAR_INDEX]), 
	     COLORAVERAGE(comp[BTDF_DIFFUSE_INDEX]), 
	     COLORAVERAGE(comp[BTDF_GLOSSY_INDEX]), 
	     COLORAVERAGE(comp[BTDF_SPECULAR_INDEX]));
    }
};

/*
inline void CopyBsdfComp(COLOR in, COLOR *inComp,
			 COLOR *out, COLOR *outComp)
{
  if(out) *out = in;
  for(int i = 0; i < BSDFCOMPONENTS; i++)
  {
    outComp[i] = inComp[i];
  }
}
*/

#endif /* _BSDFCOMP_H_ */
