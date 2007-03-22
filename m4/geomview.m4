dnl Copyright (C) 2007 Claus-Justus Heine
dnl  
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl 
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl 
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software 
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
dnl 
dnl As a special exception to the GNU General Public License, if you 
dnl distribute this file as part of a program that contains a configuration 
dnl script generated by Autoconf, you may include it under the same 
dnl distribution terms that you use for the rest of that program.

# Determine installation paths (includes, libraries etc.) from the
# installed version of Geomview
#
# GV_INIT_GEOMVIEW(MAJOR_OP,MAJOR,MINOR_OP,MINOR)
#
AC_DEFUN([GV_INIT_GEOMVIEW],
[
  AC_MSG_RESULT([configuring $PACKAGE_NAME $PACKAGE_VERSION])
  AC_PREREQ(2.59)
  AC_CANONICAL_TARGET([])
  AM_MAINTAINER_MODE
  AM_INIT_AUTOMAKE([dist-bzip2 1.9])
  AC_CONFIG_HEADER([config.h])

  AC_ARG_WITH(geomview,
    AC_HELP_STRING([--with-geomview=PROGRAM],
[Set PROGRAM to the name of the Geomview executable, possibly
including the full path to the executable.  If the
`--enable-geomview-query' switch is in effect then the installed
version of Geomview is used to determine the library, include and data
directories to use,. (default: autodetected)]),
  [case "${withval}" in
    no)
      AC_MSG_ERROR(["--without-geomview" or "--with-geomview=no" is not an option here])
      ;;
    yes) # simply ignore that, use auto-detection
      ;;
    *) 
      GEOMVIEWOPT=$withval
    esac],
    [GEOMVIEWOPT=geomview])
  AC_PATH_PROGS(GEOMVIEW, [${GEOMVIEWOPT}], ["'not found'"])
  if test "${GEOMVIEW}" = "not found"; then
	AC_MSG_ERROR([Geomview binary not found. Check your installation.])
	exit 1
  fi
  AC_ARG_ENABLE([local-emodule],
    AC_HELP_STRING([--enable-local-emodule],
[Install the emodule definition into `${HOME}/.geomview-example'; that
file will contain the full path to the example binary. Otherwise the
the emodule definition will be install under either
`PREFIX/libexec/geomview/' or -- if the `--enable-geomview-query'
switch has also been given -- under the location returned by `geomview
--print-emodule-dir'. (default: disabled)]),
    [case "${enableval}" in
       yes) LOCAL_EMODULE=true ;;
       no)  LOCAL_EMODULE=false ;;
       *) AC_MSG_ERROR([`--enable-local-emodule' does not take an argument])
          exit 1
          ;;
    esac],
    [LOCAL_EMODULE=false])
  AM_CONDITIONAL([LOCAL_EMODULE], [test "${LOCAL_EMODULE}" = "true"])
  AC_SUBST(LOCAL_EMODULE)
  
  AC_ARG_ENABLE([geomview-query],
    AC_HELP_STRING([--enable-geomview-query],

[Ask the installed version of the Geomview executable -- if any --
about the location of the system-wide emodule definition files
(usually `PREFIX/libexec/geomview/') by runnding the Geomview executable
with various `--print-...' switchs (see `geomview --help'). Otherwise
this package assumes the following defaults:

Geomview emodules below `PREFIX/libexec/geomview/',
Geomview headers below  `PREFIX/include/geomview/',
libgeomview below       `PREFIX/lib/geomview/',
other data below        `PREFIX/share/geomview/']),
    [case "${enableval}" in
       yes) GEOMVIEW_QUERY=true ;;
       no)  GEOMVIEW_QUERY=false ;;
       *) AC_MSG_ERROR([`--enable-geomview-query' does not take an argument])
          exit 1
          ;;
    esac],
    [GEOMVIEW_QUERY=false])
  AM_CONDITIONAL([GEOMVIEW_QUERY], [test "${GEOMVIEW_QUERY}" = "true"])
  AC_SUBST(GEOMVIEW_QUERY)
  

  #
  # optionally check for the version of Geomview
  #
  m4_if($#,4,[gvversion=`${GEOMVIEW} --version`
    AC_MSG_RESULT([Geomview version: ${gvversion}])
    gv_major=`echo $gvversion|cut -d '.' -f 1`
    gv_minor=`echo $gvversion|cut -d '.' -f 2`
    gv_rev=`echo $gvversion|cut -d '.' -f 3`
    gv_major_ok=`expr $gv_major '$1' $2`
    gv_minor_ok=`expr $gv_minor '$3' $4`
    if test "$gv_major_ok" != "1" -o "$gv_minor_ok" != "1" ; then
      AC_MSG_ERROR([

$PACKAGE requires Geomview version N.M, where N $1 $2 and M $3 $4.
Your version of Geomview seems to be $gv_major.$gv_minor.$gv_rev.

])
  fi]) dnl m4_if

  if test "${GEOMVIEW_QUERY}" = "true"; then
    #
    # get the location of everything else from the installed geomview script.
    #
    moduledir=`${GEOMVIEW} --print-geomview-emodule-dir`
    geomdatadir=`${GEOMVIEW} --print-geomview-data-dir`
    geomviewlib=`${GEOMVIEW} --print-geomview-lib`
    OOGLLIB="-L${geomviewlib} -lgeomview"
    geomviewincludes=`${GEOMVIEW} --print-geomview-include`
    AM_CONDITIONAL(HAVE_LIBGEOMVIEW,true)
    if test "${LOCAL_EMODULE}" = "true"; then
      moduledir="${libexecdir}/geomview"
      geomdatadir="${datadir}/geomview"
    fi
  else
    GEOMVIEW_CHECK_PACKAGE(libgeomview, geomview,,,
      [create.h],["'${includedir}/geomview'"],,required,enabled)
    geomviewincludes="${LIBGEOMVIEW_INCLUDE_PATH}"
    geomviewlib="${LIBGEOMVIEW_LIB_PATH}"
    OOGLLIB="${LIBGEOMVIEW_LIB}"
    moduledir="${libexecdir}/geomview"
    geomdatadir="${datadir}/geomview"
  fi

  module_tcldir="${moduledir}/tcl"
  AC_SUBST(geomviewincludes)
  AC_SUBST(geomviewlib)
  AC_SUBST(OOGLLIB)
  AC_SUBST(moduledir)
  AC_SUBST(module_tcldir)
  AC_SUBST(geomdatadir)
  #
  # Check for some general stuff
  #
  AC_REQUIRE([AC_PROG_LIBTOOL])
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_PROG_CPP])
  AC_REQUIRE([AC_PROG_INSTALL])
  AC_REQUIRE([AC_PROG_LN_S])
  AC_REQUIRE([AC_PROG_MAKE_SET])
  AC_REQUIRE([AC_PROG_RANLIB])
  #
  # check for stuff s.t. Geomview's header files can be included,
  # notably "porting"; but we also make sure that bool_t is defined,
  # and that we have some value for PATH_MAX
  #
  AC_C_INLINE
  AC_C_BIGENDIAN
  AC_C_CONST
  AC_HEADER_STDC
  AC_HEADER_STDBOOL
  AC_CHECK_HEADERS([limits.h stdlib.h sys/param.h sys/types.h stdio.h math.h])
  AC_CHECK_LIB([m],[sqrt])
  AC_CHECK_DECLS([putenv, strdup, acosh, strcasecmp, strncasecmp])
  AC_CHECK_FUNCS([bcopy bzero finite sqrt])
  AC_MSG_CHECKING([for M_PI])
  AC_LANG_PUSH([C])
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
[#if HAVE_MATH_H
# include <math.h>
#endif
],
    [double blah = M_PI;])],
    [AC_MSG_RESULT([yes])
    AC_DEFINE(HAVE_M_PI, 1, [Define to 1 if M_PI is defined])],
    [AC_MSG_RESULT([no])])
  AC_LANG_POP([C])
  AH_BOTTOM([#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif

/* make sure we have PATH_MAX */
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_SYS_PARAM
# include <sys/param.h>
#endif
#ifndef PATH_MAX
# ifndef _POSIX_PATH_MAX
#  ifndef MAXPATHLEN
#   define PATH_MAX 4096
#  else
#   define PATH_MAX MAXPATHLEN
#  endif
# else
#  define PATH_MAX _POSIX_PATH_MAX
# endif
#endif])
])

AC_DEFUN([_GV_PRINT_EXPAND],
[gv_ac_blah=`eval eval eval echo $2`
AC_MSG_RESULT([$1$2])
if ! test "${gv_ac_blah}" = "$2"; then
  AC_MSG_RESULT([$1(currently: ${gv_ac_blah})])
else
  AC_MSG_RESULT([])
fi])

AC_DEFUN([GEOMVIEW_RESULT],
[AC_MSG_RESULT([])
if test -n "${gvversion}"; then
  AC_MSG_RESULT([Geomview version: "${gvversion}"])
  AC_MSG_RESULT([])
fi
AC_MSG_RESULT([Geomview include files below])
_GV_PRINT_EXPAND([                ],[${geomviewincludes}/])
AC_MSG_RESULT([libgeomview below])
_GV_PRINT_EXPAND([                ],[${geomviewlib}/])
AC_MSG_RESULT([Module executable will go into])
_GV_PRINT_EXPAND([                ],[${moduledir}/])
AC_MSG_RESULT([TCL scripts will go into])
_GV_PRINT_EXPAND([                ],[${module_tcldir}/])
AC_MSG_RESULT([Data will go into])
_GV_PRINT_EXPAND([                ],[${geomdatadir}/])
AC_MSG_RESULT([Emodule definitions will go to])
if test "${LOCAL_EMODULE}" = "true"; then
  AC_MSG_RESULT([                ~/])
  AC_MSG_RESULT([                (currently: ${HOME}/)])
else
  _GV_PRINT_EXPAND([                ],[${moduledir}/])
fi
AC_MSG_RESULT([])])


