typedef struct PM PM;
typedef struct PMStore PMStore;
typedef struct VPM VPM;

#ifndef FFT_PRECISION
#define FFT_PRECISION 32
#endif

#if FFT_PRECISION == 64
    typedef double FastPMFloat;
#elif FFT_PRECISION == 32
    typedef float FastPMFloat;
#else
    #error FFT_PRECISION must be 32 or 64
#endif


#include "fastpm-2lpt.h"
#include "fastpm-pm.h"

/* 
 * Allocate memory for FFT/painting in PM. 
 * */
FastPMFloat * pm_alloc(PM * pm);
void pm_free(PM * pm, FastPMFloat * buf);
void pm_assign(PM * pm, FastPMFloat * from, FastPMFloat * to);
size_t pm_size(PM * pm);
ptrdiff_t * pm_nmesh(PM * pm);
double * pm_boxsize(PM * pm);

#include "pmkiter.h"
#include "pmpowerspectrum.h"
