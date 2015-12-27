typedef struct FastPMClock FastPMClock;

void fastpm_clock_in(FastPMClock * clock);
void fastpm_clock_out(FastPMClock * clock);

FastPMClock * 
fastpm_clock_find(const char * file, const char * func, const char * name);
void fastpm_clock_out_barrier(FastPMClock * clock, MPI_Comm comm);

#define CLOCK(name) FastPMClock * CLK ## name = fastpm_clock_find(__FILE__, __func__, # name);\
                    fastpm_clock_in(CLK ## name);

#define ENTER(name) fastpm_clock_in(CLK ## name)
#define LEAVE(name) fastpm_clock_out(CLK ## name)
#define LEAVEB(name, comm) fastpm_clock_out_barrier(CLK ## name, comm)

void fastpm_clock_stat(MPI_Comm comm);