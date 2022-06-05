#! /bin/sh

RSCRIPT_BIN=$1

# Uncompress ceres-solver source
${RSCRIPT_BIN} -e "utils::untar(tarfile = 'ceres-solver-2.1.0.tar.gz')"
mv ceres-solver-2.1.0 ceres-src