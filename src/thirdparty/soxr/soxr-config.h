#if !defined soxr_config_included
#define soxr_config_included

#define AVCODEC_FOUND 0
#define AVUTIL_FOUND 0
#define WITH_PFFFT 1

#define HAVE_FENV_H 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_LRINT 1
#define HAVE_BIGENDIAN 0

#define WITH_CR32 1
#define WITH_CR32S 1
#define WITH_CR64 1
#define WITH_CR64S 1
#define WITH_VR32 1

#define WITH_HI_PREC_CLOCK 1
#define WITH_FLOAT_STD_PREC_CLOCK 0

#ifdef _DEBUG
#define WITH_DEV_TRACE 1
#else
#define WITH_DEV_TRACE 0
#endif

#endif