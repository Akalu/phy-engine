// pmapoptions.H : class for Photon map options

#ifndef _PMAPOPTIONS_H_
#define _PMAPOPTIONS_H_

#include "options.h"  // For parsing options
#include "color.h"
#include <time.h>

#include "raytracing/screenbuffer.H"


/**** Used constants ****/

const int MAXRECONPHOTONS = 400;

typedef enum 
{
  GLOBAL_MAP,
  CAUSTIC_MAP,
  IMPORTANCE_MAP
} PMAP_TYPE;

typedef enum 
{
  GLOBAL_DENSITY,
  CAUSTIC_DENSITY,
  REC_CDENSITY,
  REC_GDENSITY,
  IMPORTANCE_CDENSITY,
  IMPORTANCE_GDENSITY,
  GLOBAL_RADIANCE,
  CAUSTIC_RADIANCE
} RADRETURN_OPTION;

typedef enum
{
  NO_DENSITY_CONTROL,
  CONSTANT_RD,
  IMPORTANCE_RD
} DENSITY_CONTROL_OPTION;

typedef enum
{
  USE_IMPORTANCE = 0
} IMPORTANCE_OPTION;

typedef enum
{
  STEP,
  TRANSCOSINE
} DC_ACCEPTPDFTYPE;

/**** Photon map options ****/

class CPmapState // : public COptions
{
  // Variables
public:

  // UI Options
  int doGlobalMap;
  long gpaths_per_iteration;
  int precomputeGIrradiance;

  int doCausticMap;
  long cpaths_per_iteration;

  int renderImage;

  int reconGPhotons;
  int reconCPhotons;
  int reconIPhotons;
  int distribPhotons;

  int doStats;

  int balanceKDTree;

  int usePhotonMapSampler;

  DENSITY_CONTROL_OPTION densityControl;
  IMPORTANCE_OPTION importanceOption;


  DC_ACCEPTPDFTYPE acceptPdfType;

  float constantRD;

  float minimumImpRD;
  int doImportanceMap;
  long ipaths_per_iteration;

  float cImpScale;
  float gImpScale;


  int cornerScatter;
  float gThreshold;

  float falseColMax;
  int falseColLog;
  int falseColMono;

  RADRETURN_OPTION radianceReturn;
  int returnCImportance;

  /*
    int minimumEyePathDepth;
    int maximumEyePathDepth;
  */

  int minimumLightPathDepth;
  int maximumLightPathDepth;
  int maxCombinedLength;


  // Other (changing) state options

  int iteration_nr;
  int g_iteration_nr;
  int c_iteration_nr;
  int i_iteration_nr;

  long total_cpaths, total_gpaths, total_ipaths;

  int runstop_nr; /* Number of 'external iterations'. This is
		     different from iteration_nr only when 
		     statistics are gathered. */
  long total_rays;

  float cpu_secs;	/* for counting computing times */
  clock_t lastclock;	/* " */
  int wake_up;		/* check this value at safe points during the
			 * computations in order to react on user input
			 * if there is any (call CheckForEvents()) */

  CScreenBuffer *rcScreen;

  // Methods

  CPmapState();
  virtual void Defaults(void);
};  


extern CPmapState pmapstate;

extern void ParsePmapOptions(int *argc, char **argv);
extern void PrintPmapOptions(FILE *fp);


#endif /* _PMAPOPTIONS_H_ */
