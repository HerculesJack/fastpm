#include <string.h>
#include "pmpfft.h"
#include "pmkiter.h"
#include "msg.h"
#include "walltime.h"

static void 
apply_force_kernel(PM * pm, FastPMFloat * from, FastPMFloat * to, int dir) 
{
    /* This is the force in fourier space. - i k[dir] / k2 */

#pragma omp parallel 
    {
        PMKIter kiter;

        for(pm_kiter_init(pm, &kiter);
            !pm_kiter_stop(&kiter);
            pm_kiter_next(&kiter)) {
            int d;
            double k_finite = kiter.fac[dir][kiter.iabs[dir]].k_finite;
            double kk_finite = 0;
            for(d = 0; d < 3; d++) {
                kk_finite += kiter.fac[d][kiter.iabs[d]].kk_finite;
            }
            ptrdiff_t ind = kiter.ind;
            /* - i k[d] / k2 */
            if(LIKELY(kk_finite > 0)) {
                to[ind + 0] =   from[ind + 1] * (k_finite / kk_finite);
                to[ind + 1] = - from[ind + 0] * (k_finite / kk_finite);
            } else {
                to[ind + 0] = 0;
                to[ind + 1] = 0;
            }
        }
    }
}

void 
pm_calculate_forces(PMStore * p, PM * pm, FastPMFloat * delta_k, double density_factor)
{
    walltime_measure("/Force/Init");

    PMGhostData * pgd = pm_ghosts_create(pm, p, PACK_POS, NULL); 

    walltime_measure("/Force/AppendGhosts");

    FastPMFloat * canvas = pm_alloc(pm);

    /* Watch out: this paints number of particles per cell. when pm_nc_factor is not 1, 
     * it is less than the density (a cell is smaller than the mean seperation between particles. 
     * We thus have to boost the density by density_factor.
     * */
    pm_paint(pm, canvas, p, p->np + pgd->nghosts, density_factor);
    walltime_measure("/Force/Paint");
    
    pm_r2c(pm, canvas, delta_k);
    walltime_measure("/Force/FFT");

    /* calculate the forces save them to p->acc */

    int d;
    ptrdiff_t i;
    int ACC[] = {PACK_ACC_X, PACK_ACC_Y, PACK_ACC_Z};
    for(d = 0; d < 3; d ++) {
        apply_force_kernel(pm, delta_k, canvas, d);
        walltime_measure("/Force/Transfer");

        pm_c2r(pm, canvas);
        walltime_measure("/Force/FFT");

#pragma omp parallel for
        for(i = 0; i < p->np + pgd->nghosts; i ++) {
            p->acc[i][d] = pm_readout_one(pm, canvas, p, i) / pm->Norm;
        }
        walltime_measure("/Force/Readout");

        pm_ghosts_reduce(pgd, ACC[d]); 
        walltime_measure("/Force/ReduceGhosts");
    }
    pm_free(pm, canvas);

    pm_ghosts_free(pgd);
    walltime_measure("/Force/Finish");

    MPI_Barrier(pm->Comm2D);
    walltime_measure("/Force/Wait");
}    

/* measure the linear scale power spectrum up to kmax, 
 * returns 1.0 if no such scale. k == 0 is skipped. */
double
pm_calculate_linear_power(PM * pm, FastPMFloat * delta_k, double kmax)
{
    double sum = 0;
    double N   = 0;

#pragma omp parallel 
    {
        PMKIter kiter;
        for(pm_kiter_init(pm, &kiter);
            !pm_kiter_stop(&kiter);
            pm_kiter_next(&kiter)) {
            int d;
            double kk = 0.;
            for(d = 0; d < 3; d++) {
                double kk1 = kiter.fac[d][kiter.iabs[d]].kk;
                if(kk1 > kmax * kmax) {
                    goto next;
                }
                kk += kk1;
            }
            ptrdiff_t ind = kiter.ind;

            double real = delta_k[ind + 0];
            double imag = delta_k[ind + 1];
            double value = real * real + imag * imag;
            double k = sqrt(kk);
            if(k > 0.01 * kmax && k < kmax) {
                #pragma omp atomic
                sum += value;
                #pragma omp atomic
                N += 1;
            }
            next:
            continue;
        }
    }

    MPI_Allreduce(MPI_IN_PLACE, &N, 1, MPI_DOUBLE, MPI_SUM, pm->Comm2D);
    MPI_Allreduce(MPI_IN_PLACE, &sum, 1, MPI_DOUBLE, MPI_SUM, pm->Comm2D);

    if(N > 1) {
        return sum / N * pm->Volume / (pm->Norm * pm->Norm);
    } else {
        return 1.0;
    }
}

