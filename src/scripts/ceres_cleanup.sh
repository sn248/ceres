#!/bin/sh

mkdir -p ../inst/lib
cp gflags/lib/*.a ../inst/lib/
cp glog/lib/*.a ../inst/lib/
cp ceres/lib/*.a ../inst/lib/

cp -R eigen/include/* ../inst/include/
cp -R glog/include/* ../inst/include/
cp -R gflags/include/* ../inst/include/
cp -R ceres/include/* ../inst/include/

rm -fr eigen-src eigen-build eigen
rm -fr glog-src glog-build glog
rm -fr gflags-src gflags-build gflags
rm -fr ceres-src ceres-build ceres
