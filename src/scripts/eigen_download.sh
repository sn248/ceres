#! /bin/sh

RSCRIPT_BIN=$1

# Uncompress ceres-solver source
${RSCRIPT_BIN} -e "utils::untar(tarfile = 'eigen-3.4.0.tar.gz')"
mv eigen-3.4.0 eigen-src