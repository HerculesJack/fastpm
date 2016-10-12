#include <string.h>

#include <mpi.h>
#include <pfft.h>

#include <fastpm/libfastpm.h>
#include <fastpm/logging.h>

#include "pmpfft.h"

#define HAS(a, b) ((a & b) != 0)

void fastpm_store_get_position(FastPMStore * p, ptrdiff_t index, double pos[3])
{
    pos[0] = p->x[index][0];
    pos[1] = p->x[index][1];
    pos[2] = p->x[index][2];
}

void fastpm_store_get_lagrangian_position(FastPMStore * p, ptrdiff_t index, double pos[3])
{
    pos[0] = p->q[index][0];
    pos[1] = p->q[index][1];
    pos[2] = p->q[index][2];
}

static size_t pack(FastPMStore * p, ptrdiff_t index, void * buf, int flags) {
    #define DISPATCH(f, field) \
    if(HAS(flags, f)) { \
        if(p->field) { \
            if(ptr) memcpy(&ptr[s], &p->field[index], sizeof(p->field[0])); \
            s += sizeof(p->field[0]); \
            flags &= ~f; \
        } \
    }
    #define DISPATCHC(f, field, c) \
    if(HAS(flags, f)) { \
        if(p->field) { \
            if(ptr) memcpy(&ptr[s], &p->field[index][c], sizeof(p->field[0][0])); \
            s += sizeof(p->field[0][0]); \
            flags &= ~f; \
        } \
    }

    size_t s = 0;
    char * ptr = (char*) buf;
    DISPATCH(PACK_POS, x)
    DISPATCH(PACK_VEL, v)
    DISPATCH(PACK_ID, id)
    DISPATCH(PACK_DX1, dx1)
    DISPATCH(PACK_DX2, dx2)
    DISPATCH(PACK_Q, q)
    DISPATCH(PACK_ACC, acc)

    /* components */
    DISPATCHC(PACK_ACC_X, acc, 0)
    DISPATCHC(PACK_ACC_Y, acc, 1)
    DISPATCHC(PACK_ACC_Z, acc, 2)
    DISPATCHC(PACK_DX1_X, dx1, 0)
    DISPATCHC(PACK_DX1_Y, dx1, 1)
    DISPATCHC(PACK_DX1_Z, dx1, 2)
    DISPATCHC(PACK_DX2_X, dx2, 0)
    DISPATCHC(PACK_DX2_Y, dx2, 1)
    DISPATCHC(PACK_DX2_Z, dx2, 2)

    #undef DISPATCH
    #undef DISPATCHC
    if(flags != 0) {
        fastpm_raise(-1, "Runtime Error, unknown packing field.\n");
    }
    return s;
}
static void unpack(FastPMStore * p, ptrdiff_t index, void * buf, int flags) {
    size_t s = 0;
    char * ptr = (char*) buf;

    #define DISPATCH(f, field) \
    if(HAS(flags, f)) { \
        if(p->field) { \
            memcpy(&p->field[index], &ptr[s], sizeof(p->field[0])); \
            s += sizeof(p->field[0]); \
            flags &= ~f; \
        } \
    }
    #define DISPATCHC(f, field, c) \
    if(HAS(flags, f)) { \
        if(p->field) { \
            memcpy(&p->field[index][c], &ptr[s], sizeof(p->field[0][0])); \
            s += sizeof(p->field[0][0]); \
            flags &= ~f; \
        } \
    }
    DISPATCH(PACK_POS, x)
    DISPATCH(PACK_VEL, v)
    DISPATCH(PACK_ID, id)
    DISPATCH(PACK_DX1, dx1)
    DISPATCH(PACK_DX2, dx2)
    DISPATCH(PACK_Q, q)
    DISPATCH(PACK_ACC, acc)
    DISPATCHC(PACK_ACC_X, acc, 0)
    DISPATCHC(PACK_ACC_Y, acc, 1)
    DISPATCHC(PACK_ACC_Z, acc, 2)
    DISPATCHC(PACK_DX1_X, dx1, 0)
    DISPATCHC(PACK_DX1_Y, dx1, 1)
    DISPATCHC(PACK_DX1_Z, dx1, 2)
    DISPATCHC(PACK_DX2_X, dx2, 0)
    DISPATCHC(PACK_DX2_Y, dx2, 1)
    DISPATCHC(PACK_DX2_Z, dx2, 2)
    #undef DISPATCH
    #undef DISPATCHC
    if(flags != 0) {
        fastpm_raise(-1, "Runtime Error, unknown unpacking field.\n");
    }
}
static void reduce(FastPMStore * p, ptrdiff_t index, void * buf, int flags) {
    size_t s = 0;
    char * ptr = (char*) buf;

    #define DISPATCHC(f, field, i) \
    if(HAS(flags, f)) { \
        if(p->field) { \
            p->field[index][i] += * ((__typeof__(p->field[index][i])*) &ptr[s]); \
            s += sizeof(p->field[index][i]); \
            flags &= ~f; \
        } \
    }
    DISPATCHC(PACK_ACC_X, acc, 0);
    DISPATCHC(PACK_ACC_Y, acc, 1);
    DISPATCHC(PACK_ACC_Z, acc, 2);
    DISPATCHC(PACK_DX1_X, dx1, 0)
    DISPATCHC(PACK_DX1_Y, dx1, 1)
    DISPATCHC(PACK_DX1_Z, dx1, 2)
    DISPATCHC(PACK_DX2_X, dx2, 0)
    DISPATCHC(PACK_DX2_Y, dx2, 1)
    DISPATCHC(PACK_DX2_Z, dx2, 2)
    #undef DISPATCHC
    if(flags != 0) {
        fastpm_raise(-1, "Runtime Error, unknown unpacking field.\n");
    }
}
static double to_double(FastPMStore * p, ptrdiff_t index, int flags) {
    double rt = 0.;
    #define DISPATCHC(f, field, i) \
    if(HAS(flags, f)) { \
        if(p->field) { \
            rt = p->field[index][i]; \
            flags &= ~f; \
            goto byebye; \
        } \
    }
    DISPATCHC(PACK_ACC_X, acc, 0);
    DISPATCHC(PACK_ACC_Y, acc, 1);
    DISPATCHC(PACK_ACC_Z, acc, 2);
    DISPATCHC(PACK_DX1_X, dx1, 0)
    DISPATCHC(PACK_DX1_Y, dx1, 1)
    DISPATCHC(PACK_DX1_Z, dx1, 2)
    DISPATCHC(PACK_DX2_X, dx2, 0)
    DISPATCHC(PACK_DX2_Y, dx2, 1)
    DISPATCHC(PACK_DX2_Z, dx2, 2)
    #undef DISPATCHC
byebye:
    if(flags != 0) {
        fastpm_raise(-1, "Runtime Error, unknown unpacking field.\n");
        return 0;
    } else {
        return rt;
    }
}
static void from_double(FastPMStore * p, ptrdiff_t index, int flags, double value) {
    #define DISPATCHC(f, field, i) \
    if(HAS(flags, f)) { \
        if(p->field) { \
            p->field[index][i] = value; \
            flags &= ~f; \
            goto byebye; \
        } \
    }
    DISPATCHC(PACK_ACC_X, acc, 0);
    DISPATCHC(PACK_ACC_Y, acc, 1);
    DISPATCHC(PACK_ACC_Z, acc, 2);
    DISPATCHC(PACK_DX1_X, dx1, 0)
    DISPATCHC(PACK_DX1_Y, dx1, 1)
    DISPATCHC(PACK_DX1_Z, dx1, 2)
    DISPATCHC(PACK_DX2_X, dx2, 0)
    DISPATCHC(PACK_DX2_Y, dx2, 1)
    DISPATCHC(PACK_DX2_Z, dx2, 2)
    #undef DISPATCHC
byebye:
    if(flags != 0) {
        fastpm_raise(-1, "Runtime Error, unknown unpacking field.\n");
    }
}

void
fastpm_store_init(FastPMStore * p)
{
    memset(p, 0, sizeof(p[0]));
    p->mem = _libfastpm_get_gmem();
    p->pack = pack;
    p->unpack = unpack;
    p->reduce = reduce;
    p->get_position = fastpm_store_get_position;
    p->to_double = to_double;
    p->from_double = from_double;
}

void 
fastpm_store_alloc(FastPMStore * p, size_t np_upper, int attributes) 
{
    fastpm_store_init(p);

    p->np = 0; 
    p->np_upper = np_upper;
    p->attributes = attributes;

    if(attributes & PACK_Q)
        p->q = fastpm_memory_alloc(p->mem, sizeof(p->q[0]) * np_upper, FASTPM_MEMORY_HEAP);
    else
        p->q = NULL;

    if(attributes & PACK_POS)
        p->x = fastpm_memory_alloc(p->mem, sizeof(p->x[0]) * np_upper, FASTPM_MEMORY_HEAP);
    else
        p->x = NULL;


    if(attributes & PACK_VEL)
        p->v = fastpm_memory_alloc(p->mem, sizeof(p->v[0]) * np_upper, FASTPM_MEMORY_HEAP);
    else
        p->v = NULL;

    if(attributes & PACK_ID)
        p->id = fastpm_memory_alloc(p->mem, sizeof(p->id[0]) * np_upper, FASTPM_MEMORY_HEAP);
    else
        p->id = NULL;

    if(attributes & PACK_ACC)
        p->acc = fastpm_memory_alloc(p->mem, sizeof(p->acc[0]) * np_upper, FASTPM_MEMORY_HEAP);
    else
        p->acc = NULL;

    if(attributes & PACK_DX1)
        p->dx1 = fastpm_memory_alloc(p->mem, sizeof(p->dx1[0]) * np_upper, FASTPM_MEMORY_HEAP);
    else
        p->dx1 = NULL;

    if(attributes & PACK_DX2)
        p->dx2 = fastpm_memory_alloc(p->mem, sizeof(p->dx2[0]) * np_upper, FASTPM_MEMORY_HEAP);
    else
        p->dx2 = NULL;

    if(attributes & PACK_AEMIT)
        p->aemit = fastpm_memory_alloc(p->mem, sizeof(p->aemit[0]) * np_upper, FASTPM_MEMORY_HEAP);
    else
        p->aemit = NULL;
};

size_t 
fastpm_store_alloc_evenly(FastPMStore * p, size_t np_total, int attributes, double alloc_factor, MPI_Comm comm) 
{
    /* allocate for np_total cross all */
    int NTask;
    MPI_Comm_size(comm, &NTask);
    fastpm_store_alloc(p, (size_t)(1.0 * np_total / NTask * alloc_factor), attributes);
    return 0;
}

size_t
fastpm_store_get_np_total(FastPMStore * p, MPI_Comm comm)
{
    long long np = p->np;
    MPI_Allreduce(MPI_IN_PLACE, &np, 1, MPI_LONG_LONG, MPI_SUM, comm);
    return np;
}

void 
fastpm_store_destroy(FastPMStore * p) 
{
    if(p->aemit) fastpm_memory_free(p->mem, p->aemit);
    if(p->dx2) fastpm_memory_free(p->mem, p->dx2);
    if(p->dx1) fastpm_memory_free(p->mem, p->dx1);
    if(p->acc) fastpm_memory_free(p->mem, p->acc);
    if(p->id) fastpm_memory_free(p->mem, p->id);
    if(p->v) fastpm_memory_free(p->mem, p->v);
    if(p->x) fastpm_memory_free(p->mem, p->x);
    if(p->q) fastpm_memory_free(p->mem, p->q);
}

void fastpm_store_read(FastPMStore * p, char * datasource) {
    /* parse data soure and read */
}

void fastpm_store_write(FastPMStore * p, char * datasource) {
    /* parse data soure and write */
}

static void permute(void * data, int np, size_t elsize, int * ind) {
    void * tmp = malloc(elsize * np);
    if(!tmp) {
        fastpm_raise(-1, "No memory for permuting\n");
    }
    int i;
    for(i = 0; i < np; i ++) {
        memcpy(((char*) tmp) + i * elsize, ((char*) data) + ind[i] * elsize, elsize);
    }
    memcpy(data, tmp, np * elsize);
    free(tmp);
}

static void fastpm_store_permute(FastPMStore * p, int * ind) {
    if(p->x)
        permute(p->x, p->np, sizeof(p->x[0]), ind);
    if(p->v)
        permute(p->v, p->np, sizeof(p->v[0]), ind);
    if(p->id)
        permute(p->id, p->np, sizeof(p->id[0]), ind);
    if(p->q)
        permute(p->q, p->np, sizeof(p->q[0]), ind);
    if(p->acc)
        permute(p->acc, p->np, sizeof(p->acc[0]), ind);
    if(p->dx1)
        permute(p->dx1, p->np, sizeof(p->dx1[0]), ind);
    if(p->dx2)
        permute(p->dx2, p->np, sizeof(p->dx2[0]), ind);
}

void 
fastpm_store_wrap(FastPMStore * p, double BoxSize[3])
{
    int i;
    int d;
    for(i = 0; i < p->np; i ++) {
        for(d = 0; d < 3; d ++) {
            while(p->x[i][d] < 0) p->x[i][d] += BoxSize[d];
            while(p->x[i][d] >= BoxSize[d]) p->x[i][d] -= BoxSize[d];
        }
    } 
}

void fastpm_store_decompose(FastPMStore * p, fastpm_store_target_func target_func, void * data, MPI_Comm comm) {
    int * target = fastpm_memory_alloc(p->mem, sizeof(int) * p->np, FASTPM_MEMORY_HEAP);
    int NTask, ThisTask;

    MPI_Comm_rank(comm, &ThisTask);
    MPI_Comm_size(comm, &NTask);

    ptrdiff_t i;
    for(i = 0; i < p->np; i ++) {
        target[i] = target_func(p, i, data);
        if(ThisTask == target[i]) {
            target[i] = 0;
        } else {
            target[i] ++;
        }
    }
    /* do a bincount; offset by -1 because -1 is for self */
    int * count = calloc(NTask + 1, sizeof(int));
    int * offsets = calloc(NTask + 1, sizeof(int));
    int * sendcount = count + 1;
    int * recvcount = malloc(sizeof(int) * (NTask));
    int * recvoffset = malloc(sizeof(int) * (NTask));
    int * sendoffset = malloc(sizeof(int) * (NTask));

    for(i = 0; i < p->np; i ++) {
        count[target[i]] ++;
    }
    cumsum(offsets, count, NTask + 1);

    int * arg = fastpm_memory_alloc(p->mem, sizeof(int) * p->np, FASTPM_MEMORY_HEAP);
    for(i = 0; i < p->np; i ++) {
        int offset = offsets[target[i]] ++;
        arg[offset] = i;
    }
    fastpm_store_permute(p, arg);

    fastpm_memory_free(p->mem, arg);
    fastpm_memory_free(p->mem, target);

    MPI_Alltoall(sendcount, 1, MPI_INT, 
                 recvcount, 1, MPI_INT, 
                 comm);

    size_t Nsend = cumsum(sendoffset, sendcount, NTask);
    size_t Nrecv = cumsum(recvoffset, recvcount, NTask);
    size_t elsize = p->pack(p, 0, NULL, p->attributes);

    void * send_buffer = fastpm_memory_alloc(p->mem, elsize * Nsend, FASTPM_MEMORY_HEAP);
    void * recv_buffer = fastpm_memory_alloc(p->mem, elsize * Nrecv, FASTPM_MEMORY_HEAP);

    p->np -= Nsend;
    for(i = 0; i < Nsend; i ++) {
        p->pack(p, i + p->np, (char*) send_buffer + i * elsize, p->attributes);
    }


    MPI_Datatype PTYPE;
    MPI_Type_contiguous(elsize, MPI_BYTE, &PTYPE);
    MPI_Type_commit(&PTYPE);
    MPI_Alltoallv_sparse(
            send_buffer, sendcount, sendoffset, PTYPE,
            recv_buffer, recvcount, recvoffset, PTYPE,
            comm);
    MPI_Type_free(&PTYPE);

    for(i = 0; i < Nrecv; i ++) {
        p->unpack(p, i + p->np, (char*) recv_buffer + i * elsize, p->attributes);
    }

    p->np += Nrecv;

    fastpm_memory_free(p->mem, recv_buffer);
    fastpm_memory_free(p->mem, send_buffer);

    free(recvcount);
    free(recvoffset);
    free(sendoffset);
    free(offsets);
    free(count);
}

void 
fastpm_store_set_lagrangian_position(FastPMStore * p, PM * pm, double shift[3], int Nc[3])
{
    /* fill p with a uniform grid, respecting domain given by pm. use a subsample ratio. 
     * (every subsample grid points) */

    int d;
    p->np = 1;
    for(d = 0; d < 3; d++) {
        int start = pm->IRegion.start[d] * Nc[d] / pm->Nmesh[d];
        int end = (pm->IRegion.start[d] + pm->IRegion.size[d]) * Nc[d] / pm->Nmesh[d];
        p->np *= end - start;
    }
    if(p->np > p->np_upper) {
        fastpm_raise(-1, "Need %td particles; %td allocated\n", p->np, p->np_upper);
    }
    ptrdiff_t ptr = 0;

    PMXIter iter;
    for(pm_xiter_init(pm, &iter);
       !pm_xiter_stop(&iter);
        pm_xiter_next(&iter)){
        uint64_t id;
        ptrdiff_t pabs_start[3];
        ptrdiff_t pabs_end[3];
        ptrdiff_t ii, jj, kk;

        for(d = 0; d < 3; d ++) {
            pabs_start[d] = iter.iabs[d] * Nc[d] / pm->Nmesh[d];
            pabs_end[d] = (iter.iabs[d] + 1) * Nc[d] / pm->Nmesh[d];
        }
        for(ii = pabs_start[0]; ii < pabs_end[0]; ii ++)
        for(jj = pabs_start[1]; jj < pabs_end[1]; jj ++)
        for(kk = pabs_start[2]; kk < pabs_end[2]; kk ++) {
            ptrdiff_t pabs[3] = {ii, jj, kk};

            id = ii * Nc[1] * Nc[2] + jj * Nc[2] + kk;

            for(d = 0; d < 3; d ++) {
                p->x[ptr][d] = pabs[d] * (pm->BoxSize[d] / Nc[d]) + shift[d];

                p->id[ptr]  = id;

                /* set q if it is allocated. */
                if(p->q) p->q[ptr][d] = p->x[ptr][d];
            }
            ptr ++;
        }
    }
    if(ptr != p->np) {
        fastpm_raise(-1, "This is an internal error, particle number mismatched with grid. %td != %td, allocsize=%td, shape=(%td %td %td)\n", 
            ptr, p->np, pm->allocsize,
            pm->IRegion.size[0],
            pm->IRegion.size[1],
            pm->IRegion.size[2]
            );
    }
    p->a_x = p->a_v = 0.;
}

void 
fastpm_store_summary(FastPMStore * p, double dx1[3], double dx2[3], MPI_Comm comm) 
{

    int d;
    for(d = 0; d < 3; d ++) {
        dx1[d] = 0;
        dx2[d] = 0;
    }
    ptrdiff_t i;

#pragma omp parallel for
    for(i = 0; i < p->np; i ++) {
        int d;
        for(d =0; d < 3; d++) {
#pragma omp atomic
            dx1[d] += p->dx1[i][d] * p->dx1[i][d];
#pragma omp atomic
            dx2[d] += p->dx2[i][d] * p->dx2[i][d];
        } 
    }
    uint64_t Ntot = p->np;

    MPI_Allreduce(MPI_IN_PLACE, dx1, 3, MPI_DOUBLE, MPI_SUM, comm);
    MPI_Allreduce(MPI_IN_PLACE, dx2, 3, MPI_DOUBLE, MPI_SUM, comm);
    MPI_Allreduce(MPI_IN_PLACE, &Ntot,   1, MPI_LONG,  MPI_SUM, comm);
    for(d =0; d < 3; d++) {
        dx1[d] /= Ntot;
        dx1[d] = sqrt(dx1[d]);
        dx2[d] /= Ntot;
        dx2[d] = sqrt(dx2[d]);
    }

}

void
fastpm_store_copy(FastPMStore * p, FastPMStore * po)
{
    if(p->np > po->np_upper) {
        fastpm_raise(-1, "Not enough storage in target FastPMStore: asking for %td but has %td\n", p->np, po->np_upper);
    }
    if(po->x) memcpy(po->x, p->x, sizeof(p->x[0][0]) * 3 * p->np);
    if(po->q) memcpy(po->q, p->q, sizeof(p->q[0][0]) * 3 * p->np);
    if(po->v) memcpy(po->v, p->v, sizeof(p->v[0][0]) * 3 * p->np);
    if(po->acc) memcpy(po->acc, p->acc, sizeof(p->acc[0][0]) * 3 * p->np);
    if(po->dx1) memcpy(po->dx1, p->dx1, sizeof(p->dx1[0][0]) * 3 * p->np);
    if(po->dx2) memcpy(po->dx2, p->dx2, sizeof(p->dx2[0][0]) * 3 * p->np);
    if(po->id) memcpy(po->id, p->id, sizeof(p->id[0]) * p->np);

    po->np = p->np;
    po->a_x = p->a_x;
    po->a_v = p->a_v;

}

void
fastpm_store_create_subsample(FastPMStore * po, FastPMStore * p, int mod, int nc)
{
    ptrdiff_t i;
    ptrdiff_t j;
    j = 0;

    for(i = 0; i < p->np; i ++) {
        uint64_t id = p->id[i];
//        double r = fastpm_utils_get_random(id);
//        if(id % mod != 0) continue;
//        if(r * mod > 1) continue;
        if((id % nc) % mod != 0) continue;
        id /= nc;
        if((id % nc) % mod != 0) continue;
        id /= nc;
        if((id % nc) % mod != 0) continue;

        if(po->x) memcpy(po->x[j], p->x[i], sizeof(p->x[0][0]) * 3);
        if(po->q) memcpy(po->q[j], p->q[i], sizeof(p->q[0][0]) * 3);
        if(po->v) memcpy(po->v[j], p->v[i], sizeof(p->v[0][0]) * 3);
        if(po->acc) memcpy(po->acc[j], p->acc[i], sizeof(p->acc[0][0]) * 3);
        if(po->dx1) memcpy(po->dx1[j], p->dx1[i], sizeof(p->dx1[0][0]) * 3);
        if(po->dx2) memcpy(po->dx2[j], p->dx2[i], sizeof(p->dx2[0][0]) * 3);
        if(po->id) po->id[j] = p->id[i];
        j ++;
    }
    po->np = j; 
    po->a_x = p->a_x;
    po->a_v = p->a_v;
}

