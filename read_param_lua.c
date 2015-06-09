#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <mpi.h>
#include "parameters.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "msg.h"

static int ThisTask;

#define DEF_READ(my_typename, c_typename, lua_typename, transform) \
c_typename read_## my_typename ## _opt (lua_State * L, const char * name, c_typename defvalue) { \
    lua_getglobal(L, name); \
    if (!lua_is ## lua_typename (L, -1)) { \
        return defvalue; \
    } \
    c_typename retvalue = transform(lua_to ## lua_typename(L, -1)); \
    lua_pop(L, 1); \
    return  retvalue; \
} \
c_typename read_## my_typename (lua_State * L, const char * name) { \
    lua_getglobal(L, name); \
    if (!lua_is ## lua_typename (L, -1)) { \
        msg_abort(1030, "Error: Parameter %s not found in the parameter file\n", name); \
    } \
    c_typename retvalue = transform(lua_to ## lua_typename(L, -1)); \
    lua_pop(L, 1); \
    return  retvalue; \
} \
c_typename * read_array_ ## my_typename(lua_State* L, const char * name, int *len) \
{ \
    lua_getglobal(L, name); \
    if(!lua_istable(L, -1)) { \
        msg_abort(1031, "Error: Parameter %s not found or not an array in the parameter file\n"); \
    } \
    const int n = luaL_len(L, -1);     \
    c_typename * array = (c_typename *) malloc(sizeof(c_typename) * n); \
    for(int i = 1; i <= n; ++i) { \
        lua_pushinteger(L, i); \
        lua_gettable(L, -2); \
        c_typename x = transform(lua_to ## lua_typename(L, -1)); \
        lua_pop(L,1); \
        array[i-1] = x; \
    } \
    lua_pop(L, 1); \
    *len = n; \
    return array; \
}

#define DEF_READ2(my_typename, c_typename, lua_typename, transform, argtype, argname) \
c_typename read_## my_typename ## _opt (lua_State * L, const char * name, c_typename defvalue, argtype argname){ \
    lua_getglobal(L, name); \
    if (!lua_is ## lua_typename (L, -1)) { \
        return defvalue; \
    } \
    c_typename retvalue = transform(lua_to ## lua_typename(L, -1), argname); \
    lua_pop(L, 1); \
    return  retvalue; \
} \
c_typename read_## my_typename (lua_State * L, const char * name, argtype argname) { \
    lua_getglobal(L, name); \
    if (!lua_is ## lua_typename (L, -1)) { \
        msg_abort(1030, "Error: Parameter %s not found in the parameter file\n", name); \
    } \
    c_typename retvalue = transform(lua_to ## lua_typename(L, -1), argname); \
    lua_pop(L, 1); \
    return  retvalue; \
} \
c_typename * read_array_ ## my_typename(lua_State* L, const char * name, int *len, argtype argname) \
{ \
    lua_getglobal(L, name); \
    if(!lua_istable(L, -1)) { \
        msg_abort(1031, "Error: Parameter %s not found or not an array in the parameter file\n"); \
    } \
    int n = luaL_len(L, -1);     \
    c_typename * array = (c_typename *) malloc(sizeof(c_typename) * n); \
    for(int i = 1; i <= n; ++i) { \
        lua_pushinteger(L, i); \
        lua_gettable(L, -2); \
        c_typename x = transform(lua_to ## lua_typename(L, -1), argname); \
        lua_pop(L,1); \
        array[i-1] = x; \
    } \
    lua_pop(L, 1); \
    *len = n; \
    return array; \
}

#define PLAIN(a) (a)
static char * _strdup(const char * str) {
    char * ret = malloc(strlen(str) + 1);
    strcpy(ret, str);
    return ret;
}
struct enum_entry {
    char * str;
    int value;
};
static int parse_enum(const char * str, struct enum_entry * enum_table) {
    struct enum_entry * p;
    for(p = enum_table; p->str; p ++) {
        if(!strcmp(str, p->str)) {
            return p->value;
        }
    }
    int n = 10;
    for(p = enum_table; p->str; p ++) {
        n += strlen(p->str) + 10;
    }
    char * options = malloc(n);
    options[0] = 0;
    for(p = enum_table; p->str; p ++) {
        if(p != enum_table)
            strcat(options, ", ");
        strcat(options, "`");
        strcat(options, p->str);
        strcat(options, "`");
    }
     
    msg_abort(9999, "value `%s` is not recognized. Options are %s \n",
        str, options);
    return 0;
}
DEF_READ(boolean, int, boolean, PLAIN)
DEF_READ(integer, int, integer, PLAIN)
DEF_READ(number, double, number, PLAIN)
DEF_READ(string, char *, string, _strdup)
DEF_READ2(enum, int, string, parse_enum, struct enum_entry *, enum_table)

int read_parameter_file(const char filename[], Parameters * param);
static void bcast_string(char** string);
static void bcast_array(void ** parray, size_t elsize, int* len);

//int read_parameters(const char filename[], Parameters* const param)
int read_parameters(const int argc, char * argv[],
        Parameters * param)
{
    if(argc < 2)
        msg_abort(1, "Error: Parameter file not specified. cola_code param.lua\n");

    char const * const filename = argv[argc-1];

    //int myrank;
    MPI_Comm_rank(MPI_COMM_WORLD, &ThisTask);
    if(ThisTask == 0) {
        int ret = read_parameter_file(filename, param);
        if(ret != 0)
            msg_abort(1001, "Error: Unable to read parameter file: %s\n", filename);
    }

    // Share parameters with other nodes
    MPI_Bcast(param, sizeof(Parameters), MPI_BYTE, 0, MPI_COMM_WORLD);

    bcast_string(&param->power_spectrum_filename);
    bcast_string(&param->measure_power_spectrum_filename);

    bcast_string(&param->snapshot_filename);
    bcast_string(&param->readic_filename);

    bcast_array((void**)&param->zout,   sizeof(double), &param->n_zout);

    return 0;
}

int read_parameter_file(const char filename[], Parameters* const param)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if(luaL_loadfile(L, filename) || lua_pcall(L, 0, 0, 0)) {
        fprintf(stderr, "%s", lua_tostring(L, -1));
        return -1;
    }

    memset(param, 0, sizeof(*param));
    param->nc = read_integer(L, "nc");
    param->boxsize = read_number(L, "boxsize");

    param->a_init = read_number(L, "a_init");
    param->a_final = read_number(L, "a_final");
    param->ntimestep = read_integer(L, "ntimestep");
    param->zout = read_array_number(L, "output_redshifts", &param->n_zout);

    param->random_seed = read_integer(L, "random_seed");
    param->nrealization = read_integer(L, "nrealization");

    param->omega_m = read_number(L, "omega_m");
    param->h = read_number(L, "h");
    param->sigma8 = read_number(L, "sigma8");

    param->pm_nc_factor1 = read_integer(L, "pm_nc_factor1");
    param->pm_nc_factor2 = read_integer(L, "pm_nc_factor2");
    param->change_pm = read_number(L, "change_pm");
    param->np_alloc_factor = read_number(L, "np_alloc_factor");
    param->loglevel = read_integer(L, "loglevel");

    // File Names and optional parameters realated
    param->readic_filename = read_string_opt(L, "readic", NULL);

    if(!param->readic_filename) {
        param->power_spectrum_filename = read_string(L, "powerspectrum");
    }

    param->measure_power_spectrum_filename = read_string_opt(L, "measure_power", NULL);

    param->snapshot_filename = read_string_opt(L, "snapshot", NULL);

    struct enum_entry table[] = {
        {"a", TIME_STEP_A},
        {"loga", TIME_STEP_LOGA},
        {"growth", TIME_STEP_GROWTH},
        {"cola", FORCE_MODE_COLA},
        {"za", FORCE_MODE_ZA},
        {"2lpt", FORCE_MODE_2LPT},
        {"pm", FORCE_MODE_PM},
        {NULL, -1},
    };

    param->force_mode = read_enum(L, "force_mode", table);
    param->time_step = read_enum(L, "time_step", table);
    param->diff_order = read_integer(L, "diff_order");
    if(param->force_mode == FORCE_MODE_COLA) {
        param->cola_stdda = read_boolean(L, "cola_stdda");
    } else {
        param->cola_stdda = 1;
    }
    lua_close(L);

    return 0;
}

void bcast_string(char ** pstring)
{
    int len;
    if(ThisTask == 0) {
        if(*pstring)
            len = strlen(*pstring);
        else
            len = 0;
    } else {
        len = 0;
    }
    MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if(len == 0) {
        *pstring = NULL;
        return;
    }

    if(ThisTask != 0) {
        *pstring = malloc( sizeof(char) * (len + 1));
    }

    MPI_Bcast(*pstring, len + 1, MPI_CHAR, 0, MPI_COMM_WORLD);
}

void bcast_array(void ** parray, size_t elsize, int* len)
{
    MPI_Bcast(len, 1, MPI_INT, 0, MPI_COMM_WORLD);

    const int n = *len;

    if(n == 0) {
        *parray = NULL;
        return;
    }

    if(ThisTask != 0) {
        *parray = malloc(elsize * n);
    }
    MPI_Datatype type;
    MPI_Type_contiguous(elsize, MPI_BYTE, &type);
    MPI_Type_commit(&type);
    MPI_Bcast(*parray, n, type, 0, MPI_COMM_WORLD);
    MPI_Type_free(&type);
}
