#! /bin/sh

: ${R_HOME=$(R RHOME)}
RSCRIPT_BIN=${R_HOME}/bin/Rscript
NCORES=`${RSCRIPT_BIN} -e "cat(min(2, parallel::detectCores(logical = FALSE)))"`

cd src

#### CMAKE CONFIGURATION ####
. ./scripts/cmake_config.sh

# Eigen: use the headers shipped with RcppEigen ################################
# Eigen is header-only and RcppEigen is in LinkingTo, so Ceres is compiled
# against exactly the Eigen that user code sees. A minimal CMake package
# config lets Ceres' find_package(Eigen3) pick it up.
EIGEN_INCLUDE=`${RSCRIPT_BIN} -e "cat(system.file('include', package = 'RcppEigen'))"`
if test -z "${EIGEN_INCLUDE}"; then
  echo "Could not find the RcppEigen include directory." >&2
  exit 1
fi
EIGEN_VERSION=`sed -n \
  -e 's/^#define EIGEN_WORLD_VERSION \([0-9]*\).*/\1/p' \
  -e 's/^#define EIGEN_MAJOR_VERSION \([0-9]*\).*/\1/p' \
  -e 's/^#define EIGEN_MINOR_VERSION \([0-9]*\).*/\1/p' \
  "${EIGEN_INCLUDE}/Eigen/src/Core/util/Macros.h" | paste -sd. -`
mkdir -p eigen-cmake
cat > eigen-cmake/Eigen3Config.cmake <<EOF
set(Eigen3_VERSION ${EIGEN_VERSION})
set(EIGEN3_INCLUDE_DIR "${EIGEN_INCLUDE}")
if (NOT TARGET Eigen3::Eigen)
  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  set_target_properties(Eigen3::Eigen PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${EIGEN_INCLUDE}")
endif ()
EOF
cat > eigen-cmake/Eigen3ConfigVersion.cmake <<EOF
set(PACKAGE_VERSION ${EIGEN_VERSION})
if (PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else ()
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
endif ()
EOF

# Compile ceres from source ####################################################
# Uses Ceres' bundled miniglog instead of glog/gflags, which are patched by
# tools/trim_ceres.sh to report through R.
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
# CMAKE_CXX_FLAGS_RELEASE only adds -DNDEBUG so the optimisation level is
# R's (-O2) rather than CMake's -O3, which is slower to compile.
${CMAKE_BIN} \
  -D BUILD_SHARED_LIBS=OFF \
  -D CMAKE_INSTALL_PREFIX=../ceres \
  -D CMAKE_INSTALL_LIBDIR=lib \
  -D CMAKE_BUILD_TYPE=Release \
  -D CMAKE_CXX_FLAGS_RELEASE=-DNDEBUG \
  -D CMAKE_C_FLAGS_RELEASE=-DNDEBUG \
  -D USE_CUDA=OFF \
  -D ACCELERATESPARSE=OFF \
  -D EIGENSPARSE=ON \
  -D EIGENMETIS=OFF \
  -D SUITESPARSE=OFF \
  -D GFLAGS=OFF \
  -D MINIGLOG=ON \
  -D MINIGLOG_MAX_LOG_LEVEL=-1 \
  -D SCHUR_SPECIALIZATIONS=OFF \
  -D BUILD_DOCUMENTATION=OFF \
  -D BUILD_EXAMPLES=OFF \
  -D BUILD_BENCHMARKS=OFF \
  -D BUILD_TESTING=OFF \
  -D LAPACK=OFF \
  -D Eigen3_DIR=../eigen-cmake \
  ${CMAKE_ADD_AR} ${CMAKE_ADD_RANLIB} ../ceres-src || exit 1
make -j${NCORES} || exit 1
make install || exit 1
cd ..

# Cleanup
sh ./scripts/ceres_cleanup.sh
