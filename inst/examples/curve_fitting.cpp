// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: sameeragarwal@google.com (Sameer Agarwal)
//
// Modified for the R package ceres from examples/curve_fitting.cc in Ceres Solver 2.2.0.

// Fit y = exp(m * x + c) to data passed in from R.
//
// Shows automatic differentiation (AutoDiffCostFunction) and
// multi-threaded residual/Jacobian evaluation (Solver::Options::num_threads).
// Adapted from examples/curve_fitting.cc in Ceres Solver.
//
// Usage from R:
//   Rcpp::sourceCpp(system.file("examples", "curve_fitting.cpp", package = "ceres"))
//   set.seed(1)
//   x <- seq(0, 5, length.out = 10000)
//   y <- exp(0.3 * x + 0.1) + rnorm(length(x), sd = 0.2)
//   fit_exponential(x, y, num_threads = 4)

// [[Rcpp::depends(ceres)]]

#include <ceres.h>  // Rcpp and RcppEigen

#include <vector>

#include <ceres/ceres.h>

// One residual per observation. Templated on T so that Ceres can
// evaluate it with its Jet type and compute exact derivatives.
struct ExponentialResidual {
    ExponentialResidual(double x, double y) : x_(x), y_(y) {}

    template <typename T>
    bool operator()(const T* const m, const T* const c, T* residual) const {
        residual[0] = y_ - exp(m[0] * x_ + c[0]);
        return true;
    }

private:
    const double x_;
    const double y_;
};

// [[Rcpp::export]]
Rcpp::List fit_exponential(Rcpp::NumericVector x, Rcpp::NumericVector y,
                           double m = 0.0, double c = 0.0,
                           int num_threads = 1, bool verbose = false) {
    if (x.size() != y.size()) Rcpp::stop("'x' and 'y' must have the same length");

    // Cost functors are evaluated on Ceres worker threads, where R must
    // not be touched, so copy the data out of R objects first.
    const std::vector<double> xs(x.begin(), x.end());
    const std::vector<double> ys(y.begin(), y.end());

    ceres::Problem problem;
    for (size_t i = 0; i < xs.size(); ++i) {
        problem.AddResidualBlock(
            new ceres::AutoDiffCostFunction<ExponentialResidual, 1, 1, 1>(
                new ExponentialResidual(xs[i], ys[i])),
            nullptr, &m, &c);
    }

    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_QR;
    options.num_threads = num_threads;
    options.minimizer_progress_to_stdout = verbose;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    return Rcpp::List::create(
        Rcpp::Named("m") = m,
        Rcpp::Named("c") = c,
        Rcpp::Named("converged") = summary.termination_type == ceres::CONVERGENCE,
        Rcpp::Named("final_cost") = summary.final_cost,
        Rcpp::Named("iterations") = static_cast<int>(summary.iterations.size()),
        Rcpp::Named("threads_used") = summary.num_threads_used,
        Rcpp::Named("report") = summary.BriefReport());
}
