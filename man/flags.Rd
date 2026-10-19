\name{CxxFlags}
\alias{CxxFlags}
\alias{LdFlags}
\alias{inlineCxxPlugin}
\title{
  Compiler and linker flags for the bundled Ceres Solver
}
\description{
  \code{CxxFlags()} and \code{LdFlags()} return the flags needed to
  compile and link \code{C++} code against the Ceres Solver library that
  is built and installed with this package. \code{inlineCxxPlugin()} is
  the plugin used by \code{\link[Rcpp]{sourceCpp}} when a source file
  contains \code{// [[Rcpp::depends(ceres)]]}.
}
\usage{
CxxFlags(print = TRUE)
LdFlags(print = TRUE)
inlineCxxPlugin(...)
}
\arguments{
  \item{print}{if \code{TRUE}, the flags are also printed with
    \code{cat()}, which is what a \code{Makevars} file needs.}
  \item{...}{ignored.}
}
\value{
  \code{CxxFlags()} and \code{LdFlags()} invisibly return the flags as a
  character string. \code{inlineCxxPlugin()} returns a list of settings
  for \code{\link[Rcpp]{sourceCpp}}.
}
\details{
  In a package that uses Ceres, add \code{ceres}, \code{RcppEigen} and
  \code{Rcpp} to \code{LinkingTo} and use the following in
  \code{src/Makevars}:
\preformatted{
CXX_STD = CXX17
PKG_LIBS = $(shell "${R_HOME}/bin/Rscript" -e "ceres::LdFlags()")
}
}
\examples{
CxxFlags()
LdFlags()
\dontrun{
Rcpp::sourceCpp(system.file("examples", "curve_fitting.cpp",
                            package = "ceres"))
}
}
\keyword{ programming }
