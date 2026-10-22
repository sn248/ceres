\name{ceres-package}
\alias{ceres-package}
\alias{ceres}
\docType{package}
\title{
  An Interface to 'Ceres Solver'
}
\description{
  Bundles the 'Ceres Solver' C++ library for non-linear least squares and
  general unconstrained optimization, built from source with Eigen from
  \pkg{RcppEigen}, and makes it available to \code{C++} code compiled
  with \pkg{Rcpp}.
}
\details{
  A \code{.cpp} file containing \code{// [[Rcpp::depends(ceres)]]} and
  \code{#include <ceres.h>} can be compiled with
  \code{\link[Rcpp]{sourceCpp}}; see the files in
  \code{system.file("examples", package = "ceres")}. Other packages can
  link to Ceres with the flags from \code{\link{CxxFlags}} and
  \code{\link{LdFlags}}.

  Ceres is built with its own minimal logging library (\sQuote{miniglog})
  in place of glog and gflags. Log messages are printed with
  \code{REprintf()}, and fatal errors (e.g. a failed \code{CHECK}) throw a
  \code{C++} exception that \pkg{Rcpp} turns into an R error.
}
\author{
Satyaprakash Nayak.

Maintainer: Satyaprakash Nayak <satyaprakash.nayak@gmail.com>
}
\references{
  Sameer Agarwal, Keir Mierle and The Ceres Solver Team.
  Ceres Solver. \url{https://github.com/ceres-solver/ceres-solver}
}
\keyword{ package }
\seealso{
  \code{\link{ceres_helloworld}}, \code{\link{CxxFlags}}
}
\examples{
ceres_helloworld(0.5)
}
