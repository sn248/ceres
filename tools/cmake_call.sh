#! /bin/sh

: ${R_HOME=$(R RHOME)}
RSCRIPT_BIN=${R_HOME}/bin/Rscript
NCORES=`${RSCRIPT_BIN} -e "cat(min(2, parallel::detectCores(logical = FALSE)))"`

cd src

#### CMAKE CONFIGURATION ####
. ./scripts/cmake_config.sh

# compile gflags from source ###################################################
sh ./scripts/gflags_download.sh ${RSCRIPT_BIN}
dot() { file=$1; shift; . "$file"; }
dot ./scripts/r_config.sh ""
${RSCRIPT_BIN} --vanilla -e 'getRversion() > "4.0.0"' | grep TRUE > /dev/null
if [ $? -eq 0 ]; then
  CMAKE_ADD_AR="-D CMAKE_AR=${AR}"
  CMAKE_ADD_RANLIB="-D CMAKE_RANLIB=${RANLIB}"
else
  CMAKE_ADD_AR=""
  CMAKE_ADD_RANLIB=""
fi
mkdir gflags-build
mkdir gflags
cd gflags-build
${CMAKE_BIN} \
    -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_INSTALL_PREFIX=../gflags \
  ${CMAKE_ADD_AR} ${CMAKE_ADD_RANLIB} ../gflags-src
make -j${NCORES}
make install
cd ..
##mv gflags/lib* gflags/lib

# compile glog from source #####################################################
sh ./scripts/glog_download.sh ${RSCRIPT_BIN}
dot() { file=$1; shift; . "$file"; }
dot ./scripts/r_config.sh ""
${RSCRIPT_BIN} --vanilla -e 'getRversion() > "4.0.0"' | grep TRUE > /dev/null
if [ $? -eq 0 ]; then
  CMAKE_ADD_AR="-D CMAKE_AR=${AR}"
  CMAKE_ADD_RANLIB="-D CMAKE_RANLIB=${RANLIB}"
else
  CMAKE_ADD_AR=""
  CMAKE_ADD_RANLIB=""
fi
mkdir glog-build
mkdir glog
cd glog-build
${CMAKE_BIN} \
    -D BUILD_SHARED_LIBS=OFF \
    -D BUILD_TESTING=OFF \
    -D CMAKE_INSTALL_PREFIX=../glog \
    -D WITH_GMOCK=OFF \
    -D WITH_GTEST=OFF \
    -D WITH_UNWIND=OFF \
    -D gflags_DIR=../gflags/lib/cmake/gflags \
  ${CMAKE_ADD_AR} ${CMAKE_ADD_RANLIB} ../glog-src
# ${CMAKE_BIN} -S . -B glog-build -G  "Unix Makefiles"
make -j${NCORES}
make install
cd ..
##mv glog/lib* glog/lib

# Compile ceres from source ####################################################
sh ./scripts/ceres_download.sh ${RSCRIPT_BIN}
dot() { file=$1; shift; . "$file"; }
dot ./scripts/r_config.sh ""
${RSCRIPT_BIN} --vanilla -e 'getRversion() > "4.0.0"' | grep TRUE > /dev/null
if [ $? -eq 0 ]; then
  CMAKE_ADD_AR="-D CMAKE_AR=${AR}"
  CMAKE_ADD_RANLIB="-D CMAKE_RANLIB=${RANLIB}"
else
  CMAKE_ADD_AR=""
  CMAKE_ADD_RANLIB=""
fi
mkdir ceres-build
mkdir ceres
cd ceres-build
${CMAKE_BIN} \
  -D BUILD_SHARED_LIBS=OFF \
  -D CMAKE_INSTALL_PREFIX=../ceres \
  -D CMAKE_BUILD_TYPE=Release \
  -D CXSPARSE=OFF \
  -D ACCELERATESPARSE=OFF \
  -D EIGENSPARSE=OFF \
  -D SUITESPARSE=OFF \
  -D GFLAGS=ON \
  -D MINIGLOG=OFF \
  -D SCHUR_SPECIALIZATIONS=OFF \
  -D BUILD_DOCUMENTATION=OFF \
  -D BUILD_EXAMPLES=OFF \
  -D BUILD_BENCHMARKS=OFF \
  -D BUILD_TESTING=OFF \
  -D gflags_DIR=../gflags/lib/cmake/gflags \
  -D glog_DIR=../glog/lib/cmake/glog \
  ${CMAKE_ADD_AR} ${CMAKE_ADD_RANLIB} ../ceres-src
make -j${NCORES}
make install
cd ..
##mv ceres/lib* ceres/lib

# Cleanup
sh ./scripts/ceres_cleanup.sh