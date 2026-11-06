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
// Modified for the R package ceres from examples/helloworld*.cc in Ceres Solver 2.2.0.

// Three ways of giving Ceres the derivatives of a residual.
//
// Minimises 0.5 * (10 - x)^2, the "hello world" problem, with the
// derivative of r(x) = 10 - x computed by
//   "auto"     automatic differentiation (AutoDiffCostFunction),
//   "numeric"  central finite differences (NumericDiffCostFunction),
//   "analytic" a hand-written Jacobian (SizedCostFunction).
// Automatic differentiation is the recommended default: it is exact and
// usually as fast as hand-written derivatives. Numeric differentiation is
// for residuals that cannot be templated (e.g. they call into a library),
// and analytic derivatives for when you can do better than autodiff.
// Adapted from examples/helloworld.cc, helloworld_numeric_diff.cc and
// helloworld_analytic_diff.cc in Ceres Solver.
//
// Usage from R:
//   Rcpp::sourceCpp(system.file("examples", "derivatives.cpp", package = "ceres"))
//   hello_derivatives(0.5, method = "auto")
//   hello_derivatives(0.5, method = "numeric")
//   hello_derivatives(0.5, method = "analytic")

// [[Rcpp::depends(ceres)]]

#include <ceres.h>  // Rcpp and RcppEigen

#include <string>

#include <ceres/ceres.h>

// Templated on T so that it can be evaluated with Ceres' Jet type.
struct AutoDiffFunctor {
    template <typename T>
    bool operator()(const T* const x, T* residual) const {
        residual[0] = 10.0 - x[0];
        return true;
    }
};

// Plain doubles only: Ceres perturbs x and differences the results.
struct NumericDiffFunctor {
    bool operator()(const double* const x, double* residual) const {
        residual[0] = 10.0 - x[0];
        return true;
    }
};

// One residual, one parameter block of size one. Evaluate() fills in the
// residual and, when Ceres asks for it, the Jacobian d r / d x.
class AnalyticCostFunction : public ceres::SizedCostFunction<1, 1> {
public:
    bool Evaluate(double const* const* parameters, double* residuals,
                  double** jacobians) const override {
        const double x = parameters[0][0];
        residuals[0] = 10.0 - x;
        if (jacobians != nullptr && jacobians[0] != nullptr) {
            jacobians[0][0] = -1.0;
        }
        return true;
    }
};

// [[Rcpp::export]]
Rcpp::List hello_derivatives(double x = 0.5, std::string method = "auto",
                             bool verbose = false) {
    const double initial_x = x;

    ceres::CostFunction* cost_function = nullptr;
    if (method == "auto") {
        cost_function =
            new ceres::AutoDiffCostFunction<AutoDiffFunctor, 1, 1>(new AutoDiffFunctor);
    } else if (method == "numeric") {
        cost_function =
            new ceres::NumericDiffCostFunction<NumericDiffFunctor, ceres::CENTRAL, 1, 1>(
                new NumericDiffFunctor);
    } else if (method == "analytic") {
        cost_function = new AnalyticCostFunction;
    } else {
        Rcpp::stop("'method' must be one of \"auto\", \"numeric\" or \"analytic\"");
    }

    ceres::Problem problem;
    problem.AddResidualBlock(cost_function, nullptr, &x);

    ceres::Solver::Options options;
    options.minimizer_progress_to_stdout = verbose;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    return Rcpp::List::create(
        Rcpp::Named("method") = method,
        Rcpp::Named("initial_x") = initial_x,
        Rcpp::Named("x") = x,
        Rcpp::Named("converged") = summary.termination_type == ceres::CONVERGENCE,
        Rcpp::Named("final_cost") = summary.final_cost,
        Rcpp::Named("iterations") = static_cast<int>(summary.iterations.size()),
        Rcpp::Named("report") = summary.BriefReport());
}
