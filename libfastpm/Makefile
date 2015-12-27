-include ../Makefile.local
-include ../Makefile.rules

CPPFLAGS += -I ../api

DIR_PATH = $(GSL_DIR) ../depends/install

CPPFLAGS += $(foreach dir, $(DIR_PATH), -I$(dir)/include)
LDFLAGS += $(foreach dir, $(DIR_PATH), -L$(dir)/lib) 

LIBSOURCES = libfastpm.c \
    fastpm-2lpt.c \
    fastpm-pm.c \
    fastpm-steps.c \
    utils.c \
    prof.c \
    vpm.c  \
    pmpfft.c  \
    pmghosts.c  \
    pmpaint.c \
    pmstore.c \
    pmgrav.c \
    pmic.c \
    pm2lpt.c \
    cosmology.c \
    pmkiter.c \
    pmpowerspectrum.c \
    logging.c

libfastpm.a : $(LIBSOURCES:%.c=.objs/%.o)
	$(AR) rcs $@ $(LIBSOURCES:%.c=.objs/%.o) 

-include $(LIBSOURCES:%.c=.deps/%.d)

clean:
	rm -rf .objs
	rm -rf .deps
	rm -rf libfastpm.a
