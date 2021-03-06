/* localline.h: generate and trace a local line */

#ifndef _LOCALLINE_H_
#define _LOCALLINE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ray.h"

/* constructs a ray with uniformly chosen origin on patch and cosine distributed 
 * direction w.r.t. patch normal. Origin and direction are uniquely determined by
 * the 4-dimensional sample vector xi. */
extern RAY McrGenerateLocalLine(PATCH *patch, double *xi);

/* determines nearest intersection point and patch */
extern HITREC *McrShootRay(PATCH *P, RAY *ray, HITREC *hitstore);

#ifdef __cplusplus
}
#endif

#endif /*_LOCALLINE_H_*/
