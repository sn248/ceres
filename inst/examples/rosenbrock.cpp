// General unconstrained minimisation of the Rosenbrock function
//
//   f(x, y) = (1 - x)^2 + 100 (y - x^2)^2
//
// with GradientProblem and GradientProblemSolver. Unlike the other
// examples this is not a least squares problem: Ceres only needs f and
// its gradient, and uses a line search method (L-BFGS by default), much
// like optim(method = "BFGS"). The gradient can come from automatic
// differentiation, central finite differences, or be written by hand.
// Adapted from examples/rosenbrock.cc, rosenbrock_numeric_diff.cc and
// rosenbrock_analytic_diff.cc in Ceres Solver.
//
// Usage from R:
//   Rcpp::sourceCpp(system.file("examples", "rosenbrock.cpp", package = "ceres"))
//   minimize_rosenbrock(c(-1.2, 1))
//   minimize_rosenbrock(c(-1.2, 1), method = "analytic", direction = "BFGS")
//
//   # Compare with optim()
//   fr <- function(p) (1 - p[1])^2 + 100 * (p[2] - p[1]^2)^2
//   optim(c(-1.2, 1), fr, method = "BFGS")

// [[Rcpp::depends(ceres)]]

#include <ceres.h>  // Rcpp and RcppEigen

#include <string>

#include <ceres/ceres.h>

// Templated cost, for AutoDiffFirstOrderFunction.
struct Rosenbrock {
    template <typename T>
    bool operator()(const T* parameters, T* cost) const {
        const T x = parameters[0];
        const T y = parameters[1];
        cost[0] = (1.0 - x) * (1.0 - x) + 100.0 * (y - x * x) * (y - x * x);
        return true;
    }
};

// Cost on plain doubles, for NumericDiffFirstOrderFunction.
struct RosenbrockNumeric {
    bool operator()(const double* parameters, double* cost) const {
        const double x = parameters[0];
        const double y = parameters[1];
        cost[0] = (1.0 - x) * (1.0 - x) + 100.0 * (y - x * x) * (y - x * x);
        return true;
    }
};

// Cost and hand-written gradient.
class RosenbrockAnalytic : public ceres::FirstOrderFunction {
public:
    bool Evaluate(const double* parameters, double* cost,
                  double* gradient) const override {
        const double x = parameters[0];
        const double y = parameters[1];
        cost[0] = (1.0 - x) * (1.0 - x) + 100.0 * (y - x * x) * (y - x * x);
        if (gradient != nullptr) {
            gradient[0] = -2.0 * (1.0 - x) - 200.0 * (y - x * x) * 2.0 * x;
            gradient[1] = 200.0 * (y - x * x);
        }
        return true;
    }
    int NumParameters() const override { return 2; }
};

// [[Rcpp::export]]
Rcpp::List minimize_rosenbrock(Rcpp::NumericVector start, std::string method = "auto",
                               std::string direction = "LBFGS", bool verbose = false) {
    if (start.size() != 2) Rcpp::stop("'start' must have length 2");
    double parameters[2] = {start[0], start[1]};

    ceres::FirstOrderFunction* function = nullptr;
    if (method == "auto") {
        function = new ceres::AutoDiffFirstOrderFunction<Rosenbrock, 2>(new Rosenbrock);
    } else if (method == "numeric") {
        function =
            new ceres::NumericDiffFirstOrderFunction<RosenbrockNumeric, ceres::CENTRAL, 2>(
                new RosenbrockNumeric);
    } else if (method == "analytic") {
        function = new RosenbrockAnalytic;
    } else {
        Rcpp::stop("'method' must be one of \"auto\", \"numeric\" or \"analytic\"");
    }
    ceres::GradientProblem problem(function);

    ceres::GradientProblemSolver::Options options;
    if (!ceres::StringToLineSearchDirectionType(direction,
                                                &options.line_search_direction_type)) {
        Rcpp::stop("unknown 'direction'; use \"LBFGS\", \"BFGS\", "
                   "\"NONLINEAR_CONJUGATE_GRADIENT\" or \"STEEPEST_DESCENT\"");
    }
    options.max_num_iterations = 1000;
    options.minimizer_progress_to_stdout = verbose;

    ceres::GradientProblemSolver::Summary summary;
    ceres::Solve(options, problem, parameters, &summary);

    return Rcpp::List::create(
        Rcpp::Named("par") = Rcpp::NumericVector::create(parameters[0], parameters[1]),
        Rcpp::Named("value") = summary.final_cost,
        Rcpp::Named("converged") = summary.termination_type == ceres::CONVERGENCE,
        Rcpp::Named("iterations") = static_cast<int>(summary.iterations.size()),
        Rcpp::Named("gradient_evaluations") = summary.num_gradient_evaluations,
        Rcpp::Named("report") = summary.BriefReport());
}
