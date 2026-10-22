// Powell's singular function.
//
// Minimises 0.5 * (f1^2 + f2^2 + f3^2 + f4^2) where
//
//   f1 = x1 + 10 x2
//   f2 = sqrt(5) (x3 - x4)
//   f3 = (x2 - 2 x3)^2
//   f4 = sqrt(10) (x1 - x4)^2
//
// The minimum is 0 at x = (0, 0, 0, 0). Each variable is its own
// parameter block and each residual depends on only two of them, which
// shows how a problem is assembled from small residual blocks. The
// Hessian is singular at the solution, so convergence is only linear
// and the solver stops on the gradient or function tolerance.
// Adapted from examples/powell.cc in Ceres Solver.
//
// Usage from R:
//   Rcpp::sourceCpp(system.file("examples", "powell.cpp", package = "ceres"))
//   powell(c(3, -1, 0, 1))

// [[Rcpp::depends(ceres)]]

#include <ceres.h>  // Rcpp and RcppEigen

#include <cmath>

#include <ceres/ceres.h>

struct F1 {
    template <typename T>
    bool operator()(const T* const x1, const T* const x2, T* residual) const {
        residual[0] = x1[0] + 10.0 * x2[0];
        return true;
    }
};

struct F2 {
    template <typename T>
    bool operator()(const T* const x3, const T* const x4, T* residual) const {
        residual[0] = std::sqrt(5.0) * (x3[0] - x4[0]);
        return true;
    }
};

struct F3 {
    template <typename T>
    bool operator()(const T* const x2, const T* const x3, T* residual) const {
        residual[0] = (x2[0] - 2.0 * x3[0]) * (x2[0] - 2.0 * x3[0]);
        return true;
    }
};

struct F4 {
    template <typename T>
    bool operator()(const T* const x1, const T* const x4, T* residual) const {
        residual[0] = std::sqrt(10.0) * (x1[0] - x4[0]) * (x1[0] - x4[0]);
        return true;
    }
};

// [[Rcpp::export]]
Rcpp::List powell(Rcpp::NumericVector start, bool verbose = false) {
    if (start.size() != 4) Rcpp::stop("'start' must have length 4");
    double x1 = start[0], x2 = start[1], x3 = start[2], x4 = start[3];

    ceres::Problem problem;
    problem.AddResidualBlock(new ceres::AutoDiffCostFunction<F1, 1, 1, 1>(new F1),
                             nullptr, &x1, &x2);
    problem.AddResidualBlock(new ceres::AutoDiffCostFunction<F2, 1, 1, 1>(new F2),
                             nullptr, &x3, &x4);
    problem.AddResidualBlock(new ceres::AutoDiffCostFunction<F3, 1, 1, 1>(new F3),
                             nullptr, &x2, &x3);
    problem.AddResidualBlock(new ceres::AutoDiffCostFunction<F4, 1, 1, 1>(new F4),
                             nullptr, &x1, &x4);

    ceres::Solver::Options options;
    options.max_num_iterations = 100;
    options.linear_solver_type = ceres::DENSE_QR;
    options.minimizer_progress_to_stdout = verbose;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    return Rcpp::List::create(
        Rcpp::Named("x") = Rcpp::NumericVector::create(x1, x2, x3, x4),
        Rcpp::Named("converged") = summary.termination_type == ceres::CONVERGENCE,
        Rcpp::Named("final_cost") = summary.final_cost,
        Rcpp::Named("iterations") = static_cast<int>(summary.iterations.size()),
        Rcpp::Named("report") = summary.BriefReport());
}
