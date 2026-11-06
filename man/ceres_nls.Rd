\name{ceres_nls}
\alias{ceres_nls}
\alias{coef.ceres_nls}
\alias{residuals.ceres_nls}
\alias{vcov.ceres_nls}
\alias{deviance.ceres_nls}
\alias{df.residual.ceres_nls}
\alias{print.ceres_nls}
\alias{summary.ceres_nls}
\alias{print.summary.ceres_nls}
\title{
  Non-linear least squares with Ceres Solver
}
\description{
  Minimises \eqn{\frac{1}{2}\sum_i \rho(r_i(\theta)^2)}{sum(rho(r_i(theta)^2)) / 2}
  over the parameters \eqn{\theta}{theta}, optionally within bounds, where
  the residuals \eqn{r_i}{r_i} are computed by an R function and
  \eqn{\rho}{rho} is an optional robust loss function.
}
\usage{
ceres_nls(fn, par, ..., jac = NULL, lower = -Inf, upper = Inf,
          loss = c("linear", "huber", "soft_l1", "cauchy", "arctan", "tukey"),
          loss_scale = 1, vcov = TRUE, control = ceres_control())

\method{coef}{ceres_nls}(object, ...)
\method{residuals}{ceres_nls}(object, ...)
\method{vcov}{ceres_nls}(object, ...)
\method{deviance}{ceres_nls}(object, ...)
\method{df.residual}{ceres_nls}(object, ...)
\method{print}{ceres_nls}(x, digits = max(3L, getOption("digits") - 3L), ...)
\method{summary}{ceres_nls}(object, ...)
\method{print}{summary.ceres_nls}(x, digits = max(3L, getOption("digits") - 3L), ...)
}
\arguments{
  \item{fn}{a function \code{fn(par, ...)} returning the numeric vector of
    residuals. Its length must not change between calls. Non-finite values
    during the solve make Ceres reject that step.}
  \item{par}{a numeric vector of starting values. Its names are passed on to
    \code{fn} and \code{jac} and used in the results.}
  \item{...}{further arguments passed to \code{fn} and \code{jac}. For the
    methods, ignored.}
  \item{jac}{an optional function \code{jac(par, ...)} returning the Jacobian
    of the residuals, a numeric matrix with one row per residual and one
    column per parameter. If \code{NULL}, finite differences are used, which
    never evaluate \code{fn} outside the bounds.}
  \item{lower, upper}{bounds on the parameters: a single value, a vector with
    one value per parameter, or a vector named by some of the parameters,
    in which case the others are unbounded.}
  \item{loss}{the loss function \eqn{\rho}{rho}, applied to each residual.
    With \code{"linear"} this is ordinary least squares. The others reduce
    the influence of outliers, see \sQuote{Details}.}
  \item{loss_scale}{a positive number, the scale of the loss function: roughly
    the size of residual above which it starts to reduce the influence of an
    observation. It is in the units of the residuals, so the default of 1 is
    rarely right for your data. Ignored with \code{loss = "linear"}.}
  \item{vcov}{if \code{TRUE}, compute the covariance matrix of the
    estimates. It is not available with a robust loss, see \sQuote{Details}.}
  \item{control}{options for the solver, from \code{\link{ceres_control}}.}
  \item{object, x}{an object of class \code{"ceres_nls"}, or
    \code{"summary.ceres_nls"} for its print method.}
  \item{digits}{the number of significant digits to print.}
}
\details{
  With scale \eqn{a}{a} and \eqn{s = r^2}{s = r^2}, the loss functions are
  those of Ceres Solver:
  \tabular{ll}{
    \code{"huber"} \tab \eqn{s} for \eqn{s \le a^2}{s <= a^2}, otherwise \eqn{2a\sqrt{s} - a^2}{2 a sqrt(s) - a^2} \cr
    \code{"soft_l1"} \tab \eqn{2a^2(\sqrt{1 + s/a^2} - 1)}{2 a^2 (sqrt(1 + s / a^2) - 1)} \cr
    \code{"cauchy"} \tab \eqn{a^2\log(1 + s/a^2)}{a^2 log(1 + s / a^2)} \cr
    \code{"arctan"} \tab \eqn{a\arctan(s/a)}{a atan(s / a)} \cr
    \code{"tukey"} \tab \eqn{\frac{a^2}{3}(1 - (1 - s/a^2)^3)}{a^2 / 3 (1 - (1 - s / a^2)^3)} for \eqn{s \le a^2}{s <= a^2}, otherwise \eqn{a^2/3}{a^2 / 3}
  }

  The covariance matrix is
  \eqn{\hat\sigma^2 (J^\top J)^{-1}}{sigma^2 (J'J)^-1} with
  \eqn{\hat\sigma^2}{sigma^2} the residual sum of squares divided by the
  residual degrees of freedom, as in \code{\link[stats]{nls}}. That variance
  estimate assumes the linear loss, so with a robust loss \code{vcov} is
  \code{NULL} and a message says so. It is also \code{NULL}, with a warning,
  if the Jacobian is rank deficient, and has a warning if a parameter ends up
  at a bound, where the usual standard errors are not valid.

  \code{fn} and \code{jac} are always called on the main R thread, one at a
  time. Each point costs one call of \code{fn}, plus one call of \code{jac} or
  up to \code{2 * length(par)} further calls of \code{fn} for the finite
  differences, when Ceres needs the Jacobian there. An error or interrupt in
  \code{fn} or \code{jac} stops the solve and is passed on to the caller, and
  Ceres may also print a line about the failed evaluation. If the solver does
  not converge, or an interrupt arrives between calls of \code{fn} and
  \code{jac}, there is a warning and the result has the best point found so
  far.

  The default tolerances of \code{\link{ceres_control}} can stop the solver
  earlier than \code{\link[stats]{nls}} would. For more accurate estimates,
  use smaller values of \code{function_tol} and \code{parameter_tol}.
}
\value{
  \code{ceres_nls()} returns a list of class \code{"ceres_nls"} with
  \item{par}{the estimates.}
  \item{residuals}{the residuals at the estimates.}
  \item{jacobian}{the Jacobian of the residuals at the estimates.}
  \item{vcov}{the covariance matrix of the estimates, or \code{NULL}.}
  \item{cost}{the objective at the estimates, including the loss function.}
  \item{initial_cost}{the objective at \code{par}.}
  \item{rss}{the residual sum of squares, without the loss function.}
  \item{converged}{\code{TRUE} if Ceres reports convergence.}
  \item{status, message}{the termination type and message from Ceres.}
  \item{iterations}{the number of iterations.}
  \item{n_evals}{the number of calls of \code{fn} and \code{jac}.}
  \item{time}{the time spent in the solver, in seconds.}
  \item{trace}{a data frame with one row per iteration.}
  \item{at_bound}{a logical vector, \code{TRUE} for parameters at a bound.}
  \item{loss, loss_scale, n, call}{the loss function and its scale, the number
    of residuals and the call.}
}
\seealso{
  \code{\link{ceres_control}}, \code{\link[stats]{nls}}. For large problems,
  where calling R for every evaluation is too slow, write the residuals in
  \code{C++}: see \code{\link{CxxFlags}}.
}
\examples{
set.seed(1)
x <- seq(0, 5, length.out = 100)
y <- exp(0.3 * x + 0.1) + rnorm(100, sd = 0.2)
y[c(20, 60)] <- y[c(20, 60)] + 10
resid_exp <- function(p, x, y) y - exp(p[["m"]] * x + p[["c"]])

## ordinary least squares is pulled away by the two outliers
fit <- ceres_nls(resid_exp, c(m = 0, c = 0), x = x, y = y)
summary(fit)

## a robust loss, with m constrained to be positive
robust <- ceres_nls(resid_exp, c(m = 0.2, c = 0), x = x, y = y,
                    lower = c(m = 0), loss = "cauchy", loss_scale = 0.5,
                    vcov = FALSE)
coef(robust)
}
\keyword{ optimize }
\keyword{ nonlinear }
