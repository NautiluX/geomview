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
AC_DEFUN([GV_INIT_GEOMVIEW],
[AC_ARG_WITH(geomview,
  AC_HELP_STRING([--with-geomview=PROGRAM],
[Set PROGRAM to the name of the Geomview executable, possibly including the full
path to the executable. The installed version of Geomview is used to determine
the library, include and data directories to use. (default: autodetected)]),
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
  AC_PATH_PROGS(GEOMVIEW, ${GEOMVIEWOPT}, "not found")
  if test "${GEOMVIEW}" = "not found"; then
	AC_MSG_ERROR([Geomview binary not found. Check your installation.])
	exit 1
  fi
  moduledir=`geomview --print-geomview-emodule-dir`
  AC_SUBST(moduledir)
  AC_MSG_RESULT([Module will go into "${moduledir}/"])

  module_tcldir="${moduledir}/tcl"
  AC_SUBST(module_tcldir)
  AC_MSG_RESULT([TCL scripts will go into "${module_tcldir}/"])

  geomdatadir=`geomview --print-geomview-data-dir`
  AC_SUBST(geomdatadir)
  AC_MSG_RESULT([Data will go into "${geomdatadir}/"])
])
