#!/bin/sh

mkdir -p ../inst/lib
cp ceres/lib/*.a ../inst/lib/

cp -R ceres/include/* ../inst/include/
# miniglog stands in for glog: put it where <glog/logging.h> is found
cp -R ceres/include/ceres/internal/miniglog/glog ../inst/include/

rm -fr eigen-cmake
rm -fr ceres-src ceres-build ceres
