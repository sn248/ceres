#!/bin/sh

cp gflags/lib/*.a ../inst/
cp glog/lib/*.a ../inst/
cp ceres/lib/*.a ../inst/

cp -R ceres/include/* ../inst/include/
cp -R glog/include/* ../inst/include/
cp -R gflags/include/* ../inst/include/

rm -fr gflags-src gflags-build gflags
rm -fr glog-src glog-build glog
rm -fr ceres-src ceres-build ceres