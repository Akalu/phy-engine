/* tracepath.h: random walk generation */

#ifndef _TRACEPATH_H_
#define _TRACEPATH_H_

#ifdef __cplusplus
extern "C" {
#endif

/* path node: contains all necessary data for computing the score afterwards */
typedef struct PATHNODE {
  PATCH *patch;
  double probability;
  POINT inpoint, outpoint;
} PATHNODE;

/* a full path, basically an array of 'nrnodes' PATHNODEs */
typedef struct PATH {
  int nrnodes;
  int nodesalloced;
  PATHNODE *nodes;
} PATH;

/* initialises nrnodes, nodesalloced to zero and 'nodes' to the NULL pointer */
extern void InitPath(PATH *path);

/* sets nrnodes to zero (forgets old path, but does not free the memory for the
 * nodes */
extern void ClearPath(PATH *path);

/* adds a node to the path. Re-allocates more space for the nodes if necessary. */
extern void PathAddNode(PATH *path, PATCH *patch, double prob, POINT inpoint, POINT outpoint);

/* disposes of the memory for storing path nodes. */
extern void FreePathNodes(PATH *path);

/* Traces a random walk originating at 'origin', with birth probability
 * 'birth_prob' (filled in as probability of the origin node: source term
 * estimation is being suppressed --- survival probability at the origin is
 * 1). Survivial probability at other nodes than the origin is calculated by
 * 'SurvivalProbability()', results are stored in 'path', which should be an 
 * PATH struct, previously initialised by InitPath(). If required, TracePath()
 * allocates extra space for storing nodes calls to PathAddNode(). 
 * FreePathNodes() should be called in order to dispose of this memory
 * when no longer needed. */
extern PATH *TracePath(PATCH *origin,
		       double birth_prob,
		       double (*SurvivalProbability)(PATCH *P),
		       PATH *path);

/* traces 'nr_paths' such paths with given birth probabilities */
extern void TracePaths(long nr_paths,
		       double (*BirthProbability)(PATCH *P),
		       double (*SurvivalProbability)(PATCH *P),
		       void (*ScorePath)(PATH *, long nr_paths, double (*birth_prob)(PATCH *)),
		       void (*Update)(PATCH *P, double w));

#ifdef __cplusplus
}
#endif

#endif /*_TRACEPATH_H_*/
