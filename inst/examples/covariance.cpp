// Standard errors of a non-linear least squares fit with ceres::Covariance.
//
// Fits y = exp(m * x + c), then computes the covariance of (m, c) from
// the Jacobian at the solution, (J'J)^-1. Ceres assumes the residuals are
// already scaled by their standard deviation, so for unweighted data the
// result is multiplied by the residual variance
//
//   sigma^2 = sum(residual^2) / (n - p) = 2 * final_cost / (n - p)
//
// which gives the same answer as vcov() on an nls() fit. The DENSE_SVD
// algorithm is used because it also copes with rank-deficient Jacobians
// (set 'null_space_rank' to -1 to get the pseudo-inverse).
//
// Usage from R:
//   Rcpp::sourceCpp(system.file("examples", "covariance.cpp", package = "ceres"))
//   set.seed(1)
//   x <- seq(0, 5, length.out = 200)
//   y <- exp(0.3 * x + 0.1) + rnorm(length(x), sd = 0.2)
//   fit <- fit_exponential_vcov(x, y)
//   fit$coefficients; fit$std_error; fit$vcov
//
//   # Same as
//   nfit <- nls(y ~ exp(m * x + c), start = list(m = 0, c = 0))
//   summary(nfit)$coefficients; vcov(nfit)

// [[Rcpp::depends(ceres)]]

#include <ceres.h>  // Rcpp and RcppEigen

#include <cmath>
#include <utility>
#include <vector>

#include <ceres/ceres.h>

// The two parameters form a single block, beta = (m, c), so the
// covariance comes back as one 2 x 2 matrix.
struct ExponentialResidual {
    ExponentialResidual(double x, double y) : x_(x), y_(y) {}

    template <typename T>
    bool operator()(const T* const beta, T* residual) const {
        residual[0] = y_ - exp(beta[0] * x_ + beta[1]);
        return true;
    }

private:
    const double x_;
    const double y_;
};

// [[Rcpp::export]]
Rcpp::List fit_exponential_vcov(Rcpp::NumericVector x, Rcpp::NumericVector y,
                                double m = 0.0, double c = 0.0,
                                int null_space_rank = 0, bool verbose = false) {
    const int n = x.size();
    const int p = 2;
    if (n != y.size()) Rcpp::stop("'x' and 'y' must have the same length");
    if (n <= p) Rcpp::stop("need more observations than parameters");

    const std::vector<double> xs(x.begin(), x.end());
    const std::vector<double> ys(y.begin(), y.end());
    double beta[2] = {m, c};

    ceres::Problem problem;
    for (int i = 0; i < n; ++i) {
        problem.AddResidualBlock(
            new ceres::AutoDiffCostFunction<ExponentialResidual, 1, 2>(
                new ExponentialResidual(xs[i], ys[i])),
            nullptr, beta);
    }

    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_QR;
    options.minimizer_progress_to_stdout = verbose;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    ceres::Covariance::Options cov_options;
    cov_options.algorithm_type = ceres::DENSE_SVD;
    cov_options.null_space_rank = null_space_rank;
    ceres::Covariance covariance(cov_options);

    std::vector<std::pair<const double*, const double*>> blocks;
    blocks.push_back(std::make_pair(beta, beta));
    if (!covariance.Compute(blocks, &problem)) {
        Rcpp::stop("covariance could not be computed (Jacobian is rank deficient?)");
    }

    double cov[p * p];  // row-major
    covariance.GetCovarianceBlock(beta, beta, cov);

    const double sigma2 = 2.0 * summary.final_cost / (n - p);
    Rcpp::NumericMatrix vcov(p, p);
    for (int i = 0; i < p; ++i)
        for (int j = 0; j < p; ++j) vcov(i, j) = sigma2 * cov[i * p + j];
    Rcpp::CharacterVector names = Rcpp::CharacterVector::create("m", "c");
    vcov.attr("dimnames") = Rcpp::List::create(names, names);

    Rcpp::NumericVector estimate = Rcpp::NumericVector::create(beta[0], beta[1]);
    Rcpp::NumericVector std_error =
        Rcpp::NumericVector::create(std::sqrt(vcov(0, 0)), std::sqrt(vcov(1, 1)));
    estimate.names() = names;
    std_error.names() = names;

    return Rcpp::List::create(
        Rcpp::Named("coefficients") = estimate,
        Rcpp::Named("std_error") = std_error,
        Rcpp::Named("vcov") = vcov,
        Rcpp::Named("sigma") = std::sqrt(sigma2),
        Rcpp::Named("converged") = summary.termination_type == ceres::CONVERGENCE,
        Rcpp::Named("final_cost") = summary.final_cost,
        Rcpp::Named("report") = summary.BriefReport());
}
