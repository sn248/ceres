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
// Author: keir@google.com (Keir Mierle)
//
// Modified for the R package ceres from examples/circle_fit.cc in Ceres Solver 2.2.0.

// Fit a circle to noisy points, optionally with a robust loss.
//
// For each point (x_i, y_i) the residual is r^2 - (x_i - a)^2 - (y_i - b)^2,
// where (a, b) is the centre and r the radius. To keep the radius
// positive it is parameterised as r = m^2 and Ceres solves for m. With
// 'robust_threshold' > 0 the residuals go through a Cauchy loss so that
// outlying points have little influence on the fit.
// Adapted from examples/circle_fit.cc in Ceres Solver.
//
// Usage from R:
//   Rcpp::sourceCpp(system.file("examples", "circle_fit.cpp", package = "ceres"))
//   set.seed(1)
//   theta <- runif(200, 0, 2 * pi)
//   x <- 2 + 3 * cos(theta) + rnorm(200, sd = 0.1)
//   y <- -1 + 3 * sin(theta) + rnorm(200, sd = 0.1)
//   x[1:20] <- runif(20, -2, 6); y[1:20] <- runif(20, -5, 3)  # outliers
//   fit_circle(x, y)
//   fit <- fit_circle(x, y, robust_threshold = 1)
//   plot(x, y, asp = 1)
//   symbols(fit$x, fit$y, circles = fit$r, inches = FALSE, add = TRUE, fg = "red")

// [[Rcpp::depends(ceres)]]

#include <ceres.h>  // Rcpp and RcppEigen

#include <cmath>
#include <vector>

#include <ceres/ceres.h>

struct DistanceFromCircleCost {
    DistanceFromCircleCost(double xx, double yy) : xx_(xx), yy_(yy) {}

    template <typename T>
    bool operator()(const T* const x, const T* const y,
                    const T* const m,  // r = m^2
                    T* residual) const {
        const T r = m[0] * m[0];
        const T xp = xx_ - x[0];
        const T yp = yy_ - y[0];
        // Comparing squared distances avoids a sqrt and works better in
        // practice than r - sqrt(xp^2 + yp^2).
        residual[0] = r * r - xp * xp - yp * yp;
        return true;
    }

private:
    const double xx_;
    const double yy_;
};

// [[Rcpp::export]]
Rcpp::List fit_circle(Rcpp::NumericVector x, Rcpp::NumericVector y,
                      double robust_threshold = 0.0, bool verbose = false) {
    const int n = x.size();
    if (n != y.size()) Rcpp::stop("'x' and 'y' must have the same length");
    if (n < 3) Rcpp::stop("need at least three points");

    const std::vector<double> xs(x.begin(), x.end());
    const std::vector<double> ys(y.begin(), y.end());

    // Start from the centroid and the mean distance to it.
    double cx = 0.0, cy = 0.0;
    for (int i = 0; i < n; ++i) {
        cx += xs[i];
        cy += ys[i];
    }
    cx /= n;
    cy /= n;
    double r = 0.0;
    for (int i = 0; i < n; ++i) r += std::hypot(xs[i] - cx, ys[i] - cy);
    r /= n;
    double m = std::sqrt(r);

    ceres::Problem problem;
    for (int i = 0; i < n; ++i) {
        ceres::LossFunction* loss = nullptr;
        if (robust_threshold > 0.0) loss = new ceres::CauchyLoss(robust_threshold);
        problem.AddResidualBlock(
            new ceres::AutoDiffCostFunction<DistanceFromCircleCost, 1, 1, 1, 1>(
                new DistanceFromCircleCost(xs[i], ys[i])),
            loss, &cx, &cy, &m);
    }

    ceres::Solver::Options options;
    options.max_num_iterations = 500;
    options.linear_solver_type = ceres::DENSE_QR;
    options.minimizer_progress_to_stdout = verbose;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    return Rcpp::List::create(
        Rcpp::Named("x") = cx,
        Rcpp::Named("y") = cy,
        Rcpp::Named("r") = m * m,
        Rcpp::Named("converged") = summary.termination_type == ceres::CONVERGENCE,
        Rcpp::Named("final_cost") = summary.final_cost,
        Rcpp::Named("iterations") = static_cast<int>(summary.iterations.size()),
        Rcpp::Named("report") = summary.BriefReport());
}
