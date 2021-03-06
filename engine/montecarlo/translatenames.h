/* translatenames.h: translates names of functions so they
 * are unique in Phy2. */

#ifndef _MCR_TRASNLATE_NAMES_H_
#define _MCR_TRASNLATE_NAMES_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CreateToplevelSurfaceElement McrCreateToplevelSurfaceElement
#define DestroyToplevelSurfaceElement McrDestroyToplevelSurfaceElement
#define CreateClusterHierarchy McrCreateClusterHierarchy
#define DestroyClusterHierarchy McrDestroyClusterHierarchy
#define PrintElement McrPrintElement
#define ForAllLeafElements McrForAllLeafElements
#define ForAllSurfaceLeafs McrForAllSurfaceLeafs
#define ForAllClusterSurfaces McrForAllClusterSurfaces
#define ForAllChildrenElements McrForAllChildrenElements
#define ElementIsLeaf McrElementIsLeaf
#define ElementRange McrElementRange
#define PushRadiance McrPushRadiance
#define PushImportance McrPushImportance
#define PullRadiance McrPullRadiance
#define PullImportance McrPullImportance
#define ElementBounds McrElementBounds
#define ElementVertices McrElementVertices
#define ClusterChildContainingElement McrClusterChildContainingElement
#define RegularSubdivideElement McrRegularSubdivideElement
#define RegularSubelementAtPoint McrRegularSubelementAtPoint
#define RegularLeafElementAtPoint McrRegularLeafElementAtPoint
#define EdgeMidpointVertex McrEdgeMidpointVertex
#define ElementDisplayRadiance McrElementDisplayRadiance
#define ElementDisplayRadianceAtPoint McrElementDisplayRadianceAtPoint
#define RenderElement McrRenderElement
#define RenderElementOutline McrRenderElementOutline
#define ElementComputeNewVertexColors McrElementComputeNewVertexColors
#define ElementAdjustTVertexColors McrElementAdjustTVertexColors
#define ElementTVertexElimination McrElementTVertexElimination
#define ElementColor McrElementColor
#define quadupxfm mcr_quadupxfm
#define triupxfm mcr_triupxfm
#define VarianceEstimate McrVarianceEstimate
#define DoNonDiffuseFirstShot McrDoNonDiffuseFirstShot

#define triBasis mcr_triBasis
#define quadBasis mcr_quadBasis
#define clusterBasis mcr_clusterBasis
#define dummyBasis mcr_dummyBasis
#define approxdesc mcr_approxdesc
#define basis mcr_basis
#define InitBasis McrInitBasis
#define PrintBasis McrPrintBasis
#define ColorAtUV McrColorAtUV

#ifdef __cplusplus
}
#endif

#endif /*_TRANSLATE_NAMES_H_*/
