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
