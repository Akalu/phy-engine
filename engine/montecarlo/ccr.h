/* ccr.h: Constant Control Radiosity */

#ifndef _PHY_CCR_H_
#define _PHY_CCR_H_

#ifdef __cplusplus
extern "C" {
#endif

/* determines and returns optimal constant control radiosity value for
 * the given radiance distribution: this is, the value of beta that 
 * minimises F(beta) = sum over all patches P of P->area times 
 * absolute value of (get_radiance(P) - get_scaling(P) * beta).
 *
 * - GetRadiance() returns the radiance to be propagated from a 
 * given ELEMENT. 
 * - GetScaling() returns a scale factor (per color component) to be 
 * multiplied with the radiance of the element. If GetScaling is a NULL 
 * pointer, no scaling is applied. Scaling is used in the context of 
 * random walk radiosity (see randwalk.c). */
extern COLOR DetermineControlRadiosity(COLOR *(*GetRadiance)(ELEMENT *),
				       COLOR (*GetScaling)(ELEMENT *));

#ifdef __cplusplus
}
#endif

#endif /* _PHY_CCR_H_ */
