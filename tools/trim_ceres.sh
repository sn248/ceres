#! /bin/sh
##
## Maintainer script, not run during installation.
##
## Builds src/ceres-solver-<version>.tar.gz from an upstream Ceres Solver
## release tarball: drops everything the R build does not compile (docs,
## data, examples, tests, benchmarks, bazel files, unused generated
## specializations) and patches the remaining sources so that they print
## via R and never call abort().
##
## Usage (from the package root):
##   tools/trim_ceres.sh /path/to/ceres-solver-2.2.0.tar.gz

set -e

UPSTREAM=$1
if test -z "${UPSTREAM}" || ! test -f "${UPSTREAM}"; then
    echo "usage: tools/trim_ceres.sh /path/to/ceres-solver-X.Y.Z.tar.gz" >&2
    exit 1
fi

PKGDIR=`pwd`
WORKDIR=`mktemp -d`
trap 'rm -rf "${WORKDIR}"' EXIT

tar xzf "${UPSTREAM}" -C "${WORKDIR}"
CERES=`ls "${WORKDIR}"`
cd "${WORKDIR}/${CERES}"

## Top-level files and directories not needed by the CMake build
rm -rf .github bazel data docs examples scripts BUILD WORKSPACE \
       CITATION.cff CONTRIBUTING.md package.xml .clang-format .gitignore

## Tests, benchmarks, gtest/gmock and code generators
cd internal/ceres
rm -rf gmock gtest autodiff_benchmarks benchmarks generated_bundle_adjustment_tests
rm -f *_test.cc *_test_util.h *_test_utils.cc *_test_utils.h test_util.cc test_util.h \
      *_benchmark.cc gmock_gtest_all.cc gmock_main.cc *.py
## Only the dynamic (_d_d_d) Schur specializations are compiled because
## the package builds with SCHUR_SPECIALIZATIONS=OFF
find generated -name '*.cc' ! -name '*_d_d_d.cc' -exec rm -f {} +

## miniglog: print through R's REprintf(), report fatal errors by throwing
## a C++ exception (turned into an R error by Rcpp) instead of abort(),
## and only log warnings and errors unless MAX_LOG_LEVEL says otherwise.
perl -0pi -e '
  s{(namespace google \{)}{extern "C" void REprintf(const char*, ...);\n\n$1};
  s{~MessageLogger\(\) \{}{~MessageLogger() noexcept(false) \{};
  s{std::cerr << stream_\.str\(\);}{REprintf("%s", stream_.str().c_str());};
  s{abort\(\);}{throw std::runtime_error(stream_.str());};
  s{(#ifdef MAX_LOG_LEVEL\n#  define LOG\(n\))}{#ifndef MAX_LOG_LEVEL\n#  define MAX_LOG_LEVEL WARNING\n#endif\n\n$1};
' miniglog/glog/logging.h
perl -0pi -e 's{(#include <sstream>\n)}{$1#include <stdexcept>\n}' miniglog/glog/logging.h

## Progress output goes to the R console instead of std::cout
perl -0pi -e '
  s{(namespace ceres\b)}{extern "C" void Rprintf(const char*, ...);\n\n$1};
  s{std::cout << output << std::endl;}{Rprintf("%s\\n", output.c_str());};
' callbacks.cc
perl -0pi -e '
  s{(using ceres::Problem;)}{extern "C" void Rprintf(const char*, ...);\n\n$1};
  s{std::cout << summary\.FullReport\(\) << "\\n";}{Rprintf("%s\\n", summary.FullReport().c_str());};
' c_api.cc

## Some files use int64_t etc. without <cstdint> and rely on it arriving
## through other headers, which it does not with Rtools' gcc (file.cc)
for f in *.cc *.h; do
    if grep -q '\bu\{0,1\}int\(8\|16\|32\|64\)_t\b' "$f" && \
       ! grep -q '#include <cstdint>' "$f"; then
        perl -0pi -e 's{^(#include )}{#include <cstdint>\n$1}m' "$f"
        grep -q '#include <cstdint>' "$f"
    fi
done

## Warnings that R CMD check reports as significant on Windows:
## -Wreorder (initialise elimination_groups in declaration order) and
## -Wformat (MinGW's printf format checks reject %zd)
perl -0pi -e '
  s{(num_threads\(linear_solver_options\.num_threads\),)(\n *)(row_block_size\(linear_solver_options\.row_block_size\),\n *e_block_size\(linear_solver_options\.e_block_size\),\n *f_block_size\(linear_solver_options\.f_block_size\),)(\n *)(elimination_groups\(linear_solver_options\.elimination_groups\),)}{$1$2$5$2$3};
' preconditioner.h
perl -0pi -e '
  s{ordering of size %zd is not an independent set",\n(\s*)first_elimination_group\.size\(\)\);}{ordering of size %d is not an independent set",\n$1static_cast<int>(first_elimination_group.size()));};
' reorder_program.cc

## -Wformat with -pedantic: %p needs a void* argument
V='static_cast<const void*>'
perl -0pi -e "s{(\"%p\", )(parameter_block->mutable_user_state\(\))}{\$1${V}(\$2)}" \
    gradient_checking_cost_function.cc
perl -0pi -e "s{(\"delta_offset=%d \}\",\n(\s*))this,\n\s*user_state_,\n\s*state_,}{\$1${V}(this),\n\$2${V}(user_state_),\n\$2${V}(state_),}" \
    parameter_block.h
perl -0pi -e "
  s{(\" %p \", )(parameter_blocks\[i\])}{\$1${V}(\$2)};
  s{(Residual block to remove: %p not found.*?\n\s*)residual_block\);}{\$1${V}(residual_block));}s;
" problem_impl.cc
perl -0pi -e "
  s{(\"Parameter block values: \",\n\s*)array,}{\$1${V}(array),};
  s{(\"\\\\nParameter block values: \",\n\s*)parameters,}{\$1${V}(parameters),}g;
" program.cc

## Fail loudly if upstream changed and a patch no longer applies
test `grep -Fc "${V}" gradient_checking_cost_function.cc` -eq 1
test `grep -Fc "${V}" parameter_block.h` -eq 3
test `grep -Fc "${V}" problem_impl.cc` -eq 2
test `grep -Fc "${V}" program.cc` -eq 3
grep -q 'REprintf("%s", stream_.str().c_str());' miniglog/glog/logging.h
grep -q 'throw std::runtime_error' miniglog/glog/logging.h
grep -q 'define MAX_LOG_LEVEL WARNING' miniglog/glog/logging.h
grep -q 'Rprintf("%s\\n", output.c_str());' callbacks.cc
grep -q '^extern "C" void Rprintf' callbacks.cc
grep -q '^extern "C" void Rprintf' c_api.cc
grep -q '^extern "C" void REprintf' miniglog/glog/logging.h
grep -q 'Rprintf("%s\\n", summary.FullReport().c_str());' c_api.cc
grep -A1 'num_threads(linear_solver_options.num_threads),' preconditioner.h | \
    grep -q 'elimination_groups(linear_solver_options.elimination_groups),'
grep -q 'static_cast<int>(first_elimination_group.size())' reorder_program.cc
if grep -n '%z' *.cc *.h; then
    echo "trim_ceres.sh: %z format left (see above)" >&2
    exit 1
fi
if grep -n 'std::cout\|std::cerr\|abort()' *.cc miniglog/glog/* | grep -v ':[0-9]*: *//\|//.*\(std::c\|abort\)'; then
    echo "trim_ceres.sh: unpatched console output or abort() left (see above)" >&2
    exit 1
fi

cd "${WORKDIR}"
tar cf - "${CERES}" | gzip -9 > "${PKGDIR}/src/${CERES}.tar.gz"
ls -l "${PKGDIR}/src/${CERES}.tar.gz"
