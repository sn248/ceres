#!/bin/sh

mkdir -p ../inst/lib
cp ceres/lib/*.a ../inst/lib/

cp -R ceres/include/* ../inst/include/
# miniglog stands in for glog: put it where <glog/logging.h> is found
cp -R ceres/include/ceres/internal/miniglog/glog ../inst/include/

# CMake generates these with CRLF line endings on Windows
for f in config.h export.h; do
  f=../inst/include/ceres/internal/$f
  tr -d '\r' < "$f" > "$f.tmp" && mv "$f.tmp" "$f"
done

rm -fr eigen-cmake
rm -fr ceres-src ceres-build ceres
