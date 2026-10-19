// Robust smoothing of a noisy signal with outliers.
//
// Solves for a smooth signal s_1, ..., s_n that minimises
//
//   sum_i rho((s_i - y_i)^2) + lambda * sum_i (s_{i+1} - s_i)^2
//
// where rho is the Cauchy loss, which down-weights outliers. Each s_i is
// its own parameter block, so the Jacobian is large and sparse; this uses
// the SPARSE_NORMAL_CHOLESKY solver backed by Eigen's sparse Cholesky, with
// automatic differentiation and multi-threaded evaluation.
//
// Usage from R:
//   Rcpp::sourceCpp(system.file("examples", "robust_smoothing.cpp", package = "ceres"))
//   set.seed(1)
//   t <- seq(0, 4 * pi, length.out = 20000)
//   y <- sin(t) + rnorm(length(t), sd = 0.1)
//   out <- sample(length(t), 500)
//   y[out] <- y[out] + rnorm(500, sd = 3)
//   fit <- robust_smooth(y, lambda = 50, num_threads = 4)
//   plot(t, y, pch = ".", ylim = c(-2, 2)); lines(t, fit$s, col = "red")

// [[Rcpp::depends(ceres)]]

#include <ceres.h>  // Rcpp and RcppEigen

#include <cmath>
#include <vector>

#include <ceres/ceres.h>

struct DataResidual {
    explicit DataResidual(double y) : y_(y) {}

    template <typename T>
    bool operator()(const T* const s, T* residual) const {
        residual[0] = s[0] - y_;
        return true;
    }

private:
    const double y_;
};

struct SmoothnessResidual {
    explicit SmoothnessResidual(double weight) : weight_(weight) {}

    template <typename T>
    bool operator()(const T* const s0, const T* const s1, T* residual) const {
        residual[0] = weight_ * (s1[0] - s0[0]);
        return true;
    }

private:
    const double weight_;
};

// [[Rcpp::export]]
Rcpp::List robust_smooth(Rcpp::NumericVector y, double lambda = 10.0,
                         double outlier_scale = 0.5, int num_threads = 1,
                         bool verbose = false) {
    const int n = y.size();
    if (n < 2) Rcpp::stop("'y' needs at least two values");
    if (!ceres::IsSparseLinearAlgebraLibraryTypeAvailable(ceres::EIGEN_SPARSE))
        Rcpp::stop("this build of Ceres has no sparse linear algebra support");

    // Copy out of R before solving: cost functors run on worker threads.
    const std::vector<double> ys(y.begin(), y.end());
    std::vector<double> s(ys);  // start from the data itself

    ceres::Problem problem;
    for (int i = 0; i < n; ++i) {
        problem.AddResidualBlock(
            new ceres::AutoDiffCostFunction<DataResidual, 1, 1>(new DataResidual(ys[i])),
            new ceres::CauchyLoss(outlier_scale), &s[i]);
    }
    const double weight = std::sqrt(lambda);
    for (int i = 0; i + 1 < n; ++i) {
        problem.AddResidualBlock(
            new ceres::AutoDiffCostFunction<SmoothnessResidual, 1, 1, 1>(
                new SmoothnessResidual(weight)),
            nullptr, &s[i], &s[i + 1]);
    }

    ceres::Solver::Options options;
    options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
    options.sparse_linear_algebra_library_type = ceres::EIGEN_SPARSE;
    options.num_threads = num_threads;
    options.minimizer_progress_to_stdout = verbose;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    return Rcpp::List::create(
        Rcpp::Named("s") = Rcpp::NumericVector(s.begin(), s.end()),
        Rcpp::Named("converged") = summary.termination_type == ceres::CONVERGENCE,
        Rcpp::Named("final_cost") = summary.final_cost,
        Rcpp::Named("iterations") = static_cast<int>(summary.iterations.size()),
        Rcpp::Named("threads_used") = summary.num_threads_used,
        Rcpp::Named("report") = summary.BriefReport());
}
