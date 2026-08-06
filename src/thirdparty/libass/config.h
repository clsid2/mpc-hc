#pragma once

#include "../../DSUtil/SharedInclude.h"

#define CONFIG_SOURCEVERSION "0.17.1"
/* ASM enabled */
#define CONFIG_ASM 1

/* targeting a 32 - or 64 - bit x86 host architecture */
#define ARCH_X86 1

/* found CoreText in System library */
/* #undef CONFIG_CORETEXT */

/* found DirectWrite */
#define CONFIG_DIRECTWRITE 1

/* found fontconfig */
/* #undef CONFIG_FONTCONFIG */

/* found freetype2 */
#define CONFIG_FREETYPE 1

/* found fribidi */
#define CONFIG_FRIBIDI 1

/* found harfbuzz-ng */
#define CONFIG_HARFBUZZ 1

/* use iconv */
#define CONFIG_ICONV 1

#define CONFIG_UNIBREAK 1

/* use small tiles */
/* #undef CONFIG_LARGE_TILES */

/* found libpng */
/* #undef CONFIG_LIBPNG */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define to 1 if you have the <iconv.h> header file. */
#define HAVE_ICONV_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strndup' function. */
/* #undef HAVE_STRNDUP */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1
