## -*- mode: autoconf; autoconf-indentation: 4; -*-
##
##  ceres configure.ac
##
##  ceres -- R interface to ceres-solver
##
##  Copyright (C) 2022  Satyaprakash Nayak
##
## This file is licensed under the GPL-2 or later just like most of my
## Open Source code, and is granted an exemption (should it be needed)
## for inclusion into ceres

# require at least autoconf 2.69
AC_PREREQ([2.69])
AC_INIT([ceres],[0.0.1])

## Set R_HOME, respecting an environment variable if set
: ${R_HOME=$(R RHOME)}
if test -z "${R_HOME}"; then
   AC_MSG_ERROR([Could not determine R_HOME.])
fi

CXX=`"${R_HOME}/bin/R" CMD config CXX`
if test -z "${CXX}"; then
    AC_MSG_ERROR([No C++ compiler available])
fi
CXXFLAGS=`"${R_HOME}/bin/R" CMD config CXXFLAGS`
CPPFLAGS=`"${R_HOME}/bin/R" CMD config CPPFLAGS`
CXX17FLAGS=`"${R_HOME}/bin/R" CMD config CXX17FLAGS`
AC_LANG(C++)
AC_REQUIRE_CPP
AC_PROG_CXX

## Default to build from source
need_to_build="yes"

##### But: Can we use pkg-config?
###AC_PATH_PROG(have_pkg_config, pkg-config, no)
##### If yes, also check for whether pkg-config knows nlopt
###if test x"${have_pkg_config}" != x"no"; then
###    AC_MSG_CHECKING([if pkg-config knows NLopt])
###    if pkg-config --exists nlopt; then
###        AC_MSG_RESULT([yes])
###        ## Since nlopt has been found, test for minimal version requirement
###        AC_MSG_CHECKING([for pkg-config checking NLopt version])
###        if pkg-config --atleast-version=2.7.0 nlopt; then
###            AC_MSG_RESULT([>= 2.7.0])
###            nlopt_include=$(pkg-config --cflags nlopt)
###            nlopt_libs=$(pkg-config --libs nlopt)
###            AC_SUBST([NLOPT_INCLUDE], "${nlopt_include}")
###            AC_SUBST([NLOPT_LIBS],    "${nlopt_libs}")
###            need_to_build="no"
###        else
###            AC_MSG_RESULT([insufficient: NLopt 2.7.0 or later is preferred.])
###        fi
###    else
###        AC_MSG_RESULT([no])
###    fi
###fi

## So do we need to build
if test x"${need_to_build}" != x"no"; then
    AC_PATH_PROG(have_cmake, cmake, no)
    if test x"${have_cmake}" == x"no"; then
       . src/scripts/cmake_config.sh
       if test -z "${CMAKE_BIN}"; then
          ## also error to end configure here
          AC_MSG_ERROR([Could not find 'cmake'.])
       fi
    fi
    ## 'uname -m' on M1 give x86_64 which is ... not helping
    machine=`"${R_HOME}/bin/Rscript" -e 'cat(Sys.info()[["machine"]])'`
    AC_MSG_RESULT([using ceres via local cmake build on ${machine} ])
    tools/cmake_call.sh
    ## cmake_call.sh installs into ceres/lib, headers are copied
    ceres_include=""
    gflags_libs="-lgflags -lgflags_nothreads"
    ##gflags_libs="../inst/libgflags.a ../inst/libgflags_nothreads.a"
    ##gflags_libs="-lgflags"
    ##glog_libs="L./glog/lib/ -lglog"
    glog_libs="-lglog"
    ##glog_libs="../inst/libglog.a"
    ##ceres_libs="../inst/libceres.a"
    ceres_libs="-lceres"
    ##ceres_libs="-L./ceres/lib/ -lceres"
fi

## Now use all the values
AC_SUBST([CERES_CPPFLAGS],["$ceres_include"])
AC_SUBST([GFLAGS_LIBS],["$gflags_libs"])
AC_SUBST([GLOG_LIBS],["$glog_libs"])
AC_SUBST([CERES_LIBS],["$ceres_libs"])
AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT