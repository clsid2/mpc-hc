dnl                                               -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.

AC_INIT([opencore-amr], [0.1.6], [http://sourceforge.net/projects/opencore-amr/])
AC_CONFIG_AUX_DIR(.)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([tar-ustar foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE

dnl Various options for configure
AC_ARG_ENABLE([compile-c],
            [AS_HELP_STRING([--enable-compile-c],
               [enable compiling as C program (default is yes)])],
            [compile_as_c=$enableval],
            [compile_as_c=yes])
AC_ARG_ENABLE([gcc-armv5],
            [AS_HELP_STRING([--enable-gcc-armv5],
               [enable GCC specific ARMv5 assembler (default is no)])],
            [gcc_armv5=$enableval], [gcc_armv5=no])
AC_ARG_ENABLE([amrnb-encoder],
            [AS_HELP_STRING([--enable-amrnb-encoder],
               [enable AMR-NB encoder (default is yes)])],
            [amrnb_encoder=$enableval], [amrnb_encoder=yes])
AC_ARG_ENABLE([amrnb-decoder],
            [AS_HELP_STRING([--enable-amrnb-decoder],
               [enable AMR-NB decoder (default is yes)])],
            [amrnb_decoder=$enableval], [amrnb_decoder=yes])
AC_ARG_ENABLE([examples],
            [AS_HELP_STRING([--enable-examples],
               [enable example encoding/decoding programs (default is no)])],
            [examples=$enableval], [examples=no])

dnl Automake conditionals to set
AM_CONDITIONAL(COMPILE_AS_C, test x$compile_as_c = xyes)
AM_CONDITIONAL(GCC_ARMV5, test x$gcc_armv5 = xyes)
AM_CONDITIONAL(AMRNB_ENCODER, test x$amrnb_encoder = xyes)
AM_CONDITIONAL(AMRNB_DECODER, test x$amrnb_decoder = xyes)
AM_CONDITIONAL(EXAMPLES, test x$examples = xyes)

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl Setup for libtool
LT_INIT

dnl Checks for libraries.
AC_CHECK_LIB([m], [main])

dnl Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

dnl Checks for library functions.
AC_CHECK_FUNCS([memset])

dnl OpenCORE AMR soname version to use
dnl goes by ‘current[:revision[:age]]’ with the soname ending up as
dnl current.age.revision.
OPENCORE_AMRNB_VERSION=0:5:0
OPENCORE_AMRWB_VERSION=0:5:0
AC_SUBST(OPENCORE_AMRNB_VERSION)
AC_SUBST(OPENCORE_AMRWB_VERSION)

AC_CONFIG_FILES([Makefile
                 amrnb/Makefile
                 amrwb/Makefile
                 test/Makefile
                 amrnb/opencore-amrnb.pc
                 amrwb/opencore-amrwb.pc])
AC_OUTPUT
