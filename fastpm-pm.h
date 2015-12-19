typedef struct FastPMExtension {
    void * function;
    void * userdata;
    struct FastPMExtension * next;
} FastPMExtension;

typedef struct {
    /* input parameters */
    int nc;
    double boxsize;
    double omega_m;
    double * time_step;
    int n_time_step;

    int USE_COLA;
    int USE_NONSTDDA;
    int USE_LINEAR_THEORY;
    double nLPT;
    double K_LINEAR;

    /* Extensions */
    FastPMExtension * exts[3];

    /* internal variables */
    MPI_Comm comm;
    int ThisTask;
    int NTask;

    PMStore * p;
    PM * pm_2lpt;
    VPM * vpm_list;

    int istep;
    PM * pm;

} FastPM;

#define FASTPM_EXT_AFTER_FORCE 0
typedef int (* fastpm_ext_after_force) (FastPM * fastpm, FastPMFloat * deltak, double a_x, void * userdata);
#define FASTPM_EXT_AFTER_KICK 1
typedef int (* fastpm_ext_after_kick) (FastPM * fastpm, void * userdata);
#define FASTPM_EXT_AFTER_DRIFT 2
typedef int (* fastpm_ext_after_drift) (FastPM * fastpm, void * userdata);
#define FASTPM_EXT_MAX 3

void fastpm_init(FastPM * fastpm, 
    double alloc_factor, 
    int NprocY, 
    int UseFFTW, 
    int * pm_nc_factor, 
    int n_pm_nc_factor, 
    double * change_pm,
    MPI_Comm comm);

void 
fastpm_add_extension(FastPM * fastpm, 
    int where,
    void * function, void * userdata);

void 
fastpm_destroy(FastPM * fastpm);
void 
fastpm_prepare_ic(FastPM * fastpm, FastPMFloat * delta_k);

void
fastpm_evolve(FastPM * fastpm);

typedef int 
(*fastpm_interp_action) (FastPM * fastpm, PMStore * pout, double aout, void * userdata);

void 
fastpm_interp(FastPM * fastpm, double * aout, int nout, 
            fastpm_interp_action action, void * userdata);
