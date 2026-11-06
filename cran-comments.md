## Test environments

* local Ubuntu 26.04, R 4.6.1, GCC 15.2.0, RcppEigen 0.3.4.0.2 (Eigen 3.4.0)

The package also installs and its tests pass against the development version
of RcppEigen (0.4.9.9-5, Eigen 5.0.1). With that version, R CMD check gives a
WARNING for two compiler warnings in the bundled Ceres sources
(-Wdeprecated-declarations and -Wstringop-overflow); neither occurs with the
current CRAN version.

## R CMD check results

0 errors | 0 warnings | 3 notes

* checking CRAN incoming feasibility ... NOTE

  New submission

  This is the first submission of 'ceres'.

* checking compilation flags used ... NOTE

  Compilation used the following non-portable flag(s):
    '-mno-omit-leaf-frame-pointer'

  This flag is not set by the package. It comes from the CXXFLAGS that
  Ubuntu's r-base package was built with (R RHOME/etc/Makeconf), and
  should not appear on CRAN's machines.

* checking HTML version of manual ... NOTE

  Skipping checking HTML validation: no command 'tidy' found.

  HTML Tidy is not installed on the test machine; this is not a problem
  with the package.

## Installation time and size

'ceres' bundles the 'Ceres Solver' 2.2.0 C++ library
(src/ceres-solver-2.2.0.tar.gz) and builds it from source with CMake at
install time, because it is not available as a system library on most
platforms. The bundled sources have already been trimmed to what the
package needs (tools/trim_ceres.sh removes the examples, tests, benchmarks,
documentation and unused generated code), and the library is built
without optional dependencies and without Schur specialisations, which
removes a large amount of template-generated code. About 115 C++ files are
compiled.

Even so, installation is slower than for most packages: on the test machine
it took 192s of CPU time (95s elapsed; the build uses at most 2 parallel make
jobs). Most of this is compiling Ceres itself, which is heavily templated and
uses Eigen. The whole of R CMD check --as-cran took 271s of CPU time (175s
elapsed).

The installed size is about 11.3Mb, of which 6.7Mb is the static library
lib/libceres.a. It is installed so that other packages can link to Ceres
through the LinkingTo/LdFlags() mechanism, which is the purpose of the
package.

Tests that compile the example files with Rcpp::sourceCpp() are skipped on
CRAN to keep the check time down; only the precompiled example runs there.

## Bundled code

The copyright holders and licenses of the bundled Ceres Solver sources (BSD
3-clause, with some files under Apache 2.0 or MIT), and the changes made to
them, are listed in inst/COPYRIGHTS. tools/trim_ceres.sh reproduces the bundled
tarball from the upstream release.
