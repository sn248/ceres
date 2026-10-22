// Watch, record and interrupt a solve with an IterationCallback.
//
// Fits y = exp(m * x + c) as in curve_fitting.cpp, with a callback that
// Ceres calls after every iteration. The callback
//   - records the parameters and cost, returned to R as a data frame
//     (this needs Solver::Options::update_state_every_iteration, otherwise
//     m and c are only updated when the solve finishes). For a rejected
//     step, cost is that of the rejected trial point while m and c stay
//     at the last accepted point,
//   - optionally prints a line per iteration, and
//   - checks for a user interrupt (Ctrl-C / Esc), and if there is one
//     stops the solve cleanly and returns the best point so far.
// Callbacks run on the calling thread, so they may use Rcpp and the R
// API even when residuals are evaluated with num_threads > 1.
// Adapted from examples/iteration_callback_example.cc in Ceres Solver.
//
// Usage from R:
//   Rcpp::sourceCpp(system.file("examples", "iteration_callback.cpp", package = "ceres"))
//   set.seed(1)
//   x <- seq(0, 5, length.out = 1000)
//   y <- exp(0.3 * x + 0.1) + rnorm(length(x), sd = 0.2)
//   fit <- fit_exponential_traced(x, y, print = TRUE)
//   fit$trace
//   plot(fit$trace$m, fit$trace$c, type = "b")

// [[Rcpp::depends(ceres)]]

#include <ceres.h>  // Rcpp and RcppEigen

#include <vector>

#include <ceres/ceres.h>

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

// R_CheckUserInterrupt() longjmps out of the current function when there
// is an interrupt, skipping C++ destructors. Calling it under
// R_ToplevelExec() contains the jump and tells us whether it happened.
static void check_interrupt_fn(void*) { R_CheckUserInterrupt(); }

static bool user_interrupted() {
    return R_ToplevelExec(check_interrupt_fn, nullptr) == FALSE;
}

class TraceCallback : public ceres::IterationCallback {
public:
    TraceCallback(const double* m, const double* c, bool print)
        : m_(m), c_(c), print_(print) {}

    ceres::CallbackReturnType operator()(
        const ceres::IterationSummary& summary) override {
        iteration.push_back(summary.iteration);
        m.push_back(*m_);
        c.push_back(*c_);
        cost.push_back(summary.cost);
        gradient_max_norm.push_back(summary.gradient_max_norm);
        step_accepted.push_back(summary.step_is_successful);

        if (print_) {
            Rcpp::Rcout << "iter " << summary.iteration << "  m = " << *m_
                        << "  c = " << *c_ << "  cost = " << summary.cost << "\n";
        }
        if (user_interrupted()) {
            interrupted = true;
            return ceres::SOLVER_ABORT;
        }
        return ceres::SOLVER_CONTINUE;
    }

    std::vector<int> iteration;
    std::vector<double> m, c, cost, gradient_max_norm;
    std::vector<bool> step_accepted;
    bool interrupted = false;

private:
    const double* m_;
    const double* c_;
    const bool print_;
};

// [[Rcpp::export]]
Rcpp::List fit_exponential_traced(Rcpp::NumericVector x, Rcpp::NumericVector y,
                                  double m = 0.0, double c = 0.0,
                                  bool print = false, int num_threads = 1) {
    if (x.size() != y.size()) Rcpp::stop("'x' and 'y' must have the same length");

    const std::vector<double> xs(x.begin(), x.end());
    const std::vector<double> ys(y.begin(), y.end());

    ceres::Problem problem;
    for (size_t i = 0; i < xs.size(); ++i) {
        problem.AddResidualBlock(
            new ceres::AutoDiffCostFunction<ExponentialResidual, 1, 1, 1>(
                new ExponentialResidual(xs[i], ys[i])),
            nullptr, &m, &c);
    }

    TraceCallback callback(&m, &c, print);

    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_QR;
    options.num_threads = num_threads;
    options.update_state_every_iteration = true;
    options.callbacks.push_back(&callback);

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    if (callback.interrupted) Rcpp::warning("solve interrupted; returning the current point");

    Rcpp::DataFrame trace = Rcpp::DataFrame::create(
        Rcpp::Named("iteration") = callback.iteration,
        Rcpp::Named("m") = callback.m,
        Rcpp::Named("c") = callback.c,
        Rcpp::Named("cost") = callback.cost,
        Rcpp::Named("gradient_max_norm") = callback.gradient_max_norm,
        Rcpp::Named("step_accepted") = callback.step_accepted);

    return Rcpp::List::create(
        Rcpp::Named("m") = m,
        Rcpp::Named("c") = c,
        Rcpp::Named("converged") = summary.termination_type == ceres::CONVERGENCE,
        Rcpp::Named("interrupted") = callback.interrupted,
        Rcpp::Named("final_cost") = summary.final_cost,
        Rcpp::Named("trace") = trace,
        Rcpp::Named("report") = summary.BriefReport());
}
