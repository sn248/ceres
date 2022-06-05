#! /bin/sh

: ${R_HOME=$(R RHOME)}
RSCRIPT_BIN=${R_HOME}/bin/Rscript
NCORES=`${RSCRIPT_BIN} -e "cat(min(2, parallel::detectCores(logical = FALSE)))"`

cd src

#### CMAKE CONFIGURATION ####
. ./scripts/cmake_config.sh

# Compile ceres from source
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
###  -D BUILD_SHARED_LIBS=OFF \
###  -D CMAKE_BUILD_TYPE=Release \
###  -D CMAKE_INSTALL_PREFIX=../nlopt \
###  -D NLOPT_CXX=ON \
###  -D NLOPT_GUILE=OFF \
###  -D NLOPT_MATLAB=OFF \
###  -D NLOPT_OCTAVE=OFF \
###  -D NLOPT_PYTHON=OFF \
###  -D NLOPT_SWIG=OFF \
###  -D NLOPT_TESTS=OFF \
###  ${CMAKE_ADD_AR} ${CMAKE_ADD_RANLIB} ../ceres-src
  -D CMAKE_BUILD_TYPE=Release \
  -D CXSPARSE=OFF \
  -D ACCELERATESPARSE=OFF \
  -D EIGENSPARSE=OFF \
  -D SUITESPARSE=OFF \
  -D GFLAGS=OFF \
  -D SCHUR_SPECIALIZATIONS=OFF \
  -D BUILD_DOCUMENTATION=OFF \
  -D BUILD_EXAMPLES=OFF \
  -D BUILD_BENCHMARKS=OFF \
  -D BUILD_TESTING=OFF \
  ${CMAKE_ADD_AR} ${CMAKE_ADD_RANLIB} ../ceres-src
make -j${NCORES}
make install
cd ..
mv ceres/lib* ceres/lib

# Cleanup
sh ./scripts/ceres_cleanup.sh