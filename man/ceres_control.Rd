\name{ceres_control}
\alias{ceres_control}
\title{
  Solver options for ceres_nls
}
\description{
  Options for \code{\link{ceres_nls}}. The defaults of the iteration limit and
  tolerances are those of Ceres Solver.
}
\usage{
ceres_control(max_iter = 50, function_tol = 1e-6, gradient_tol = 1e-10,
              parameter_tol = 1e-8, method = c("lm", "dogleg"),
              diff = c("central", "forward"), trace = FALSE)
}
\arguments{
  \item{max_iter}{the maximum number of iterations.}
  \item{function_tol}{stop when the relative change in the cost is below this.}
  \item{gradient_tol}{stop when the largest gradient element is below this.}
  \item{parameter_tol}{stop when the relative step size is below this.}
  \item{method}{the trust region strategy: Levenberg-Marquardt or dogleg.}
  \item{diff}{the finite differences used when \code{ceres_nls()} has no
    \code{jac}. \code{"central"} is more accurate and \code{"forward"} needs
    fewer calls of \code{fn}. Next to a bound, one-sided differences are used
    in either case.}
  \item{trace}{if \code{TRUE}, print the progress of the solver.}
}
\value{
  A list of class \code{"ceres_control"}.
}
\seealso{
  \code{\link{ceres_nls}}
}
\examples{
ceres_control(max_iter = 200, method = "dogleg")$max_iter
}
\keyword{ optimize }
