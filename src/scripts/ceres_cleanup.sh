#!/bin/sh

cp gflags/lib/*.a ../inst/
cp gflags/lib/*.so ../inst/
cp glog/lib/*.a ../inst/
cp glog/lib/*.so ../inst/
cp ceres/lib/*.a ../inst/

cp -R eigen/include/* ../inst/include/
cp -R glog/include/* ../inst/include/
cp -R gflags/include/* ../inst/include/
cp -R ceres/include/* ../inst/include/

rm -fr eigen-src eigen-build eigen
rm -fr glog-src glog-build glog
rm -fr gflags-src gflags-build gflags
rm -fr ceres-src ceres-build ceres