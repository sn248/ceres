\name{ceres_helloworld}
\alias{ceres_helloworld}
\title{
  Minimal Ceres Solver example
}
\description{
  Solves the one-parameter problem of minimising
  \eqn{\frac{1}{2}(10 - x)^2}{(10 - x)^2 / 2} with Ceres Solver, starting
  from \code{x}, and prints the solver progress and summary. It is the
  \sQuote{Hello World} example from the Ceres Solver tutorial and checks
  that the bundled library works.
}
\usage{
ceres_helloworld(x)
}
\arguments{
  \item{x}{a number, the initial value of the parameter.}
}
\value{
  \code{0}, invisibly returned by the C++ code; the function is called
  for its printed output.
}
\references{
  \url{http://ceres-solver.org/nnls_tutorial.html#hello-world}
}
\examples{
ceres_helloworld(0.5)
}
\keyword{ optimize }
