#! /bin/sh

RSCRIPT_BIN=$1

# Uncompress glog source
${RSCRIPT_BIN} -e "utils::untar(tarfile = 'glog-0.6.0-source.tar.gz')"
mv glog glog-src