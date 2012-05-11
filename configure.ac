# intel-driver package version number
m4_define([intel_driver_major_version], [1])
m4_define([intel_driver_minor_version], [0])
m4_define([intel_driver_micro_version], [16])
m4_define([intel_driver_pre_version],   [1])
m4_define([intel_driver_version],
          [intel_driver_major_version.intel_driver_minor_version.intel_driver_micro_version])
m4_if(intel_driver_pre_version, [0], [], [
m4_append([intel_driver_version], intel_driver_pre_version, [.pre])
])

# libva minimum version requirement
m4_define([libva_version], [0.34])
m4_define([libva_package_version], [1.2.0])

# libdrm minimum version requirement
m4_define([libdrm_version], [2.4.23])

AC_PREREQ([2.57])
AC_INIT([intel_driver], [intel_driver_version], [haihao.xiang@intel.com],
        [libva-driver-intel])
AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE

AM_CONFIG_HEADER([src/config.h])

INTEL_DRIVER_MAJOR_VERSION=intel_driver_major_version
INTEL_DRIVER_MINOR_VERSION=intel_driver_minor_version
INTEL_DRIVER_MICRO_VERSION=intel_driver_micro_version
AC_DEFINE([INTEL_DRIVER_MAJOR_VERSION], [intel_driver_major_version], [Major version of the driver])
AC_DEFINE([INTEL_DRIVER_MINOR_VERSION], [intel_driver_minor_version], [Minor version of the driver])
AC_DEFINE([INTEL_DRIVER_MICRO_VERSION], [intel_driver_micro_version], [Micro version of the driver])
AC_DEFINE([INTEL_DRIVER_PRE_VERSION], [intel_driver_pre_version], [Preversion of the driver])

INTEL_DRIVER_LT_LDFLAGS="-avoid-version"
AC_SUBST(INTEL_DRIVER_LT_LDFLAGS)

dnl Use pretty build output with automake >= 1.11
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])], [
    AM_DEFAULT_VERBOSITY=1
    AC_SUBST(AM_DEFAULT_VERBOSITY)
])

AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC
AM_PROG_CC_C_O

AC_C_BIGENDIAN
AC_HEADER_STDC
AC_SYS_LARGEFILE

LIBVA_PACKAGE_VERSION=libva_package_version
AC_SUBST(LIBVA_PACKAGE_VERSION)

dnl Check for recent enough DRM
LIBDRM_VERSION=libdrm_version
PKG_CHECK_MODULES([DRM], [libdrm >= $LIBDRM_VERSION])
AC_SUBST(LIBDRM_VERSION)

dnl Check for gen4asm
PKG_CHECK_MODULES(GEN4ASM, [intel-gen4asm >= 1.2], [gen4asm=yes], [gen4asm=no])
AM_CONDITIONAL(HAVE_GEN4ASM, test x$gen4asm = xyes)
AC_PATH_PROG([GEN4ASM], [intel-gen4asm])

dnl Check for VA-API
PKG_CHECK_MODULES(LIBVA_DEPS,	  [libva >= libva_version])
PKG_CHECK_MODULES(LIBVA_X11_DEPS, [libva-x11])

VA_VERSION=`$PKG_CONFIG --modversion libva`
VA_MAJOR_VERSION=`echo "$VA_VERSION" | cut -d'.' -f1`
VA_MINOR_VERSION=`echo "$VA_VERSION" | cut -d'.' -f2`
VA_MICRO_VERSION=`echo "$VA_VERSION" | cut -d'.' -f3`
VA_VERSION_STR="$VA_VERSION"

va_full_version_int=`expr ${VA_MAJOR_VERSION:-0} "*" 1000000 + \
                          ${VA_MINOR_VERSION:-0} "*" 10000   + \
                          ${VA_MICRO_VERSION:-0} "*" 100     + \
                          0`
VA_DRIVER_INIT_FUNC="__vaDriverInit_${VA_MAJOR_VERSION}_${VA_MINOR_VERSION}"
AC_DEFINE_UNQUOTED([VA_DRIVER_INIT_FUNC], [$VA_DRIVER_INIT_FUNC],
    [Define driver entry-point])

dnl Check for VA-API drivers path
AC_MSG_CHECKING([for VA drivers path])
LIBVA_DRIVERS_PATH=`$PKG_CONFIG libva --variable driverdir`
if test -z "$LIBVA_DRIVERS_PATH"; then
    LIBVA_DRIVERS_PATH="/usr/lib/xorg/modules/drivers"
fi
AC_MSG_RESULT([$LIBVA_DRIVERS_PATH])
AC_SUBST(LIBVA_DRIVERS_PATH)

AC_OUTPUT([
    Makefile
    debian.upstream/Makefile 
    src/Makefile
    src/shaders/Makefile
    src/shaders/h264/Makefile
    src/shaders/h264/ildb/Makefile
    src/shaders/h264/mc/Makefile
    src/shaders/mpeg2/Makefile
    src/shaders/mpeg2/vld/Makefile
    src/shaders/post_processing/Makefile
    src/shaders/post_processing/gen5_6/Makefile
    src/shaders/post_processing/gen7/Makefile
    src/shaders/render/Makefile
    src/shaders/utils/Makefile
    src/shaders/vme/Makefile
])

dnl Print summary
echo
echo $PACKAGE configuration summary:
echo
echo VA-API version ................... : $VA_VERSION_STR
echo VA-API drivers path .............. : $LIBVA_DRIVERS_PATH
echo
