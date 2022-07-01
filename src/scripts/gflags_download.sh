#! /bin/sh

RSCRIPT_BIN=$1

# Uncompress gflags source
${RSCRIPT_BIN} -e "utils::untar(tarfile = 'gflags-2.2.2-source.tar.gz')"
mv gflags gflags-src