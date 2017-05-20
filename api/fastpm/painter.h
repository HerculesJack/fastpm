FASTPM_BEGIN_DECLS

typedef enum { FASTPM_PAINTER_CIC, FASTPM_PAINTER_LINEAR, FASTPM_PAINTER_QUAD, FASTPM_PAINTER_LANCZOS} FastPMPainterType;

struct FastPMPainter {
    PM * pm;

    void   (*paint)(FastPMPainter * painter, FastPMFloat * canvas, double pos[3], double weight, int diffdir);
    double (*readout)(FastPMPainter * painter, FastPMFloat * canvas, double pos[3], int diffdir);
    fastpm_kernelfunc kernel;
    fastpm_kernelfunc diff;

    int diffdir;
    int support;
    double hsupport;
    double invh;
    int left; /* offset to start the kernel, (support - 1) / 2*/
    int Npoints; /* (support) ** 3 */
    double shift;
};

void fastpm_painter_init(FastPMPainter * painter, PM * pm,
        FastPMPainterType type, int support);

void fastpm_painter_destroy(FastPMPainter * painter);

void fastpm_painter_init_diff(FastPMPainter * painter, FastPMPainter * base, int diffdir);

void
fastpm_paint_local(FastPMPainter * painter, FastPMFloat * canvas,
        FastPMStore * p, size_t size,
        fastpm_posfunc get_position, int attribute);

void
fastpm_readout_local(FastPMPainter * painter, FastPMFloat * canvas,
        FastPMStore * p, size_t size,
        fastpm_posfunc get_position, int attribute);

void
fastpm_paint(FastPMPainter * painter, FastPMFloat * canvas,
        FastPMStore * p, fastpm_posfunc get_position, int attribute);

void
fastpm_readout(FastPMPainter * painter, FastPMFloat * canvas,
        FastPMStore * p, fastpm_posfunc get_position, int attribute);

FASTPM_END_DECLS
