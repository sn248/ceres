// Backend of ceres_nls(): non-linear least squares with residuals computed
// by an R function.
//
// Every observation gets its own residual block, because Ceres applies a loss
// function to the squared norm of a block: with one block of m residuals a
// robust loss would act on the total and not down-weight individual outliers.
// The blocks read their row from a shared ResidualEvaluator, which caches the
// last point so that R is called once per point and not once per block.

#include <RcppEigen.h>

#include <ceres/ceres.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

// Computes the residuals and Jacobian of a problem at a point. ceres_nls()
// only needs RFunctionEvaluator; compiled residuals can implement this too.
class ResidualEvaluator {
public:
    ResidualEvaluator(int m, int p) : m_(m), p_(p) {}
    virtual ~ResidualEvaluator() {}

    int num_residuals() const { return m_; }
    int num_parameters() const { return p_; }

    // Both return false if the value could not be computed at this point.
    // The results stay valid until the next call with a different point.
    virtual bool residuals(const double* par, const double** r) = 0;
    // Column-major, m x p.
    virtual bool jacobian(const double* par, const double** jac) = 0;

private:
    const int m_;
    const int p_;
};

bool all_finite(const double* x, int n) {
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(x[i])) return false;
    }
    return true;
}

// Calls back into R. Errors and interrupts must not propagate through Ceres'
// stack frames, so they are stored and rethrown after Solve() has returned.
class RFunctionEvaluator : public ResidualEvaluator {
public:
    RFunctionEvaluator(int m, int p, Rcpp::Function fn, Rcpp::Nullable<Rcpp::Function> jac,
                       const std::vector<double>& lower, const std::vector<double>& upper,
                       bool central)
        : ResidualEvaluator(m, p), fn_(fn), jac_(jac), lower_(lower), upper_(upper),
          central_(central), r_par_(p), r_(m), j_par_(p), j_(static_cast<std::size_t>(m) * p),
          work_plus_(m), work_minus_(m) {}

    bool residuals(const double* par, const double** r) override {
        if (failed()) return false;
        if (!have_r_ || !same_point(par, r_par_)) {
            have_r_ = false;
            if (!call_fn(par, r_.data())) return false;
            std::copy(par, par + num_parameters(), r_par_.begin());
            have_r_ = true;
        }
        *r = r_.data();
        return true;
    }

    bool jacobian(const double* par, const double** jac) override {
        if (failed()) return false;
        if (!have_j_ || !same_point(par, j_par_)) {
            have_j_ = false;
            const bool ok = jac_.isNull() ? numeric_jacobian(par) : call_jac(par);
            if (!ok) return false;
            std::copy(par, par + num_parameters(), j_par_.begin());
            have_j_ = true;
        }
        *jac = j_.data();
        return true;
    }

    bool failed() const { return static_cast<bool>(error_); }
    void rethrow_if_failed() const {
        if (error_) std::rethrow_exception(error_);
    }

    int fn_calls() const { return fn_calls_; }
    int jac_calls() const { return jac_calls_; }

private:
    bool same_point(const double* par, const std::vector<double>& cached) const {
        return std::memcmp(par, cached.data(), sizeof(double) * num_parameters()) == 0;
    }

    Rcpp::NumericVector as_r_vector(const double* par) const {
        return Rcpp::NumericVector(par, par + num_parameters());
    }

    bool call_fn(const double* par, double* out) {
        try {
            ++fn_calls_;
            // ceres_nls() wraps fn so that it returns a double vector of length m
            Rcpp::NumericVector value = fn_(as_r_vector(par));
            std::copy(value.begin(), value.end(), out);
        } catch (...) {
            error_ = std::current_exception();
            return false;
        }
        return all_finite(out, num_residuals());
    }

    bool call_jac(const double* par) {
        try {
            ++jac_calls_;
            Rcpp::Function jac(jac_);
            // ceres_nls() wraps jac so that it returns a double m x p matrix
            Rcpp::NumericVector value = jac(as_r_vector(par));
            std::copy(value.begin(), value.end(), j_.begin());
        } catch (...) {
            error_ = std::current_exception();
            return false;
        }
        return all_finite(j_.data(), static_cast<int>(j_.size()));
    }

    // Finite differences that never evaluate fn outside the bounds: central
    // where both sides fit, one-sided towards the interior otherwise.
    bool numeric_jacobian(const double* par) {
        const int m = num_residuals();
        const int p = num_parameters();
        const double eps = std::numeric_limits<double>::epsilon();
        const double h_central = std::cbrt(eps);
        const double h_forward = std::sqrt(eps);

        const double* base = nullptr;
        std::vector<double> x(par, par + p);

        for (int j = 0; j < p; ++j) {
            double* column = j_.data() + static_cast<std::size_t>(m) * j;
            const double scale = std::max(std::fabs(par[j]), 1.0);
            const double room_up = upper_[j] - par[j];
            const double room_down = par[j] - lower_[j];
            const double h2 = h_central * scale;

            if (central_ && room_up >= h2 && room_down >= h2) {
                x[j] = par[j] + h2;
                if (!call_fn(x.data(), work_plus_.data())) return false;
                x[j] = par[j] - h2;
                if (!call_fn(x.data(), work_minus_.data())) return false;
                x[j] = par[j];
                // the actual spacing, after rounding of par[j] +/- h2
                const double spacing = (par[j] + h2) - (par[j] - h2);
                for (int i = 0; i < m; ++i) {
                    column[i] = (work_plus_[i] - work_minus_[i]) / spacing;
                }
                continue;
            }

            // lower == upper: the parameter cannot move
            if (room_up <= 0.0 && room_down <= 0.0) {
                std::fill(column, column + m, 0.0);
                continue;
            }

            if (base == nullptr && !residuals(par, &base)) return false;

            const double h1 = h_forward * scale;
            const double step = room_up >= room_down ? std::min(h1, room_up)
                                                     : -std::min(h1, room_down);
            x[j] = par[j] + step;
            if (!call_fn(x.data(), work_plus_.data())) return false;
            x[j] = par[j];
            const double spacing = (par[j] + step) - par[j];
            for (int i = 0; i < m; ++i) {
                column[i] = (work_plus_[i] - base[i]) / spacing;
            }
        }
        return true;
    }

    Rcpp::Function fn_;
    Rcpp::Nullable<Rcpp::Function> jac_;
    const std::vector<double> lower_;
    const std::vector<double> upper_;
    const bool central_;

    bool have_r_ = false;
    std::vector<double> r_par_;
    std::vector<double> r_;
    bool have_j_ = false;
    std::vector<double> j_par_;
    std::vector<double> j_;
    std::vector<double> work_plus_;
    std::vector<double> work_minus_;

    std::exception_ptr error_;
    int fn_calls_ = 0;
    int jac_calls_ = 0;
};

// The residual block of observation i.
class ObservationResidual final : public ceres::CostFunction {
public:
    ObservationResidual(ResidualEvaluator* evaluator, int i) : evaluator_(evaluator), i_(i) {
        set_num_residuals(1);
        mutable_parameter_block_sizes()->push_back(evaluator->num_parameters());
    }

    bool Evaluate(double const* const* parameters, double* residuals,
                  double** jacobians) const override {
        const double* r = nullptr;
        if (!evaluator_->residuals(parameters[0], &r)) return false;
        residuals[0] = r[i_];
        if (jacobians != nullptr && jacobians[0] != nullptr) {
            const double* jac = nullptr;
            if (!evaluator_->jacobian(parameters[0], &jac)) return false;
            const int m = evaluator_->num_residuals();
            const int p = evaluator_->num_parameters();
            for (int j = 0; j < p; ++j) {
                jacobians[0][j] = jac[static_cast<std::size_t>(m) * j + i_];
            }
        }
        return true;
    }

private:
    ResidualEvaluator* evaluator_;
    const int i_;
};

void check_interrupt_fn(void* /*unused*/) {
    R_CheckUserInterrupt();
}

// Ends the solve after an error in the R functions, or on an interrupt.
// R_ToplevelExec() contains the jump of R_CheckUserInterrupt().
class StopCallback final : public ceres::IterationCallback {
public:
    explicit StopCallback(const RFunctionEvaluator* evaluator) : evaluator_(evaluator) {}

    ceres::CallbackReturnType operator()(const ceres::IterationSummary& /*summary*/) override {
        if (evaluator_->failed()) return ceres::SOLVER_ABORT;
        if (R_ToplevelExec(check_interrupt_fn, nullptr) == FALSE) {
            interrupted = true;
            return ceres::SOLVER_ABORT;
        }
        return ceres::SOLVER_CONTINUE;
    }

    bool interrupted = false;

private:
    const RFunctionEvaluator* evaluator_;
};

ceres::LossFunction* make_loss(const std::string& loss, double scale) {
    if (loss == "linear") return nullptr;
    if (loss == "huber") return new ceres::HuberLoss(scale);
    if (loss == "soft_l1") return new ceres::SoftLOneLoss(scale);
    if (loss == "cauchy") return new ceres::CauchyLoss(scale);
    if (loss == "arctan") return new ceres::ArctanLoss(scale);
    if (loss == "tukey") return new ceres::TukeyLoss(scale);
    Rcpp::stop("unknown loss '%s'", loss);
}

typedef std::vector<std::pair<int, ceres::IterationSummary>> IterationRows;

Rcpp::DataFrame make_trace(const IterationRows& rows) {
    const std::size_t n = rows.size();
    Rcpp::IntegerVector stage(n), iteration(n);
    Rcpp::NumericVector cost(n), cost_change(n), gradient_max_norm(n), step_norm(n), radius(n);
    Rcpp::LogicalVector successful(n);
    for (std::size_t k = 0; k < n; ++k) {
        const ceres::IterationSummary& it = rows[k].second;
        stage[k] = rows[k].first;
        iteration[k] = it.iteration;
        cost[k] = it.cost;
        cost_change[k] = it.cost_change;
        gradient_max_norm[k] = it.gradient_max_norm;
        step_norm[k] = it.step_norm;
        radius[k] = it.trust_region_radius;
        successful[k] = it.step_is_successful;
    }
    return Rcpp::DataFrame::create(
        Rcpp::Named("stage") = stage, Rcpp::Named("iteration") = iteration,
        Rcpp::Named("cost") = cost, Rcpp::Named("cost_change") = cost_change,
        Rcpp::Named("gradient_max_norm") = gradient_max_norm,
        Rcpp::Named("step_norm") = step_norm, Rcpp::Named("trust_region_radius") = radius,
        Rcpp::Named("step_is_successful") = successful);
}

// The objective 0.5 * sum(rho(r^2)), and the weights rho'(r^2).
double objective(const double* r, int m, const ceres::LossFunction* loss,
                 std::vector<double>* weights) {
    double total = 0.0;
    for (int i = 0; i < m; ++i) {
        const double s = r[i] * r[i];
        double rho[3] = {s, 1.0, 0.0};
        if (loss != nullptr) loss->Evaluate(s, rho);
        total += rho[0];
        (*weights)[i] = std::max(rho[1], 0.0);
    }
    return 0.5 * total;
}

// How much a Gauss-Newton step in the free parameters is predicted to reduce
// the objective. This is independent of what the minimizer reports, and scale
// invariant, unlike a test on the size of the gradient.
double predicted_decrease(const double* r, const double* jac, const std::vector<double>& weights,
                          const std::vector<bool>& held, int m, int p) {
    std::vector<const double*> columns;
    for (int j = 0; j < p; ++j) {
        if (!held[j]) columns.push_back(jac + static_cast<std::size_t>(m) * j);
    }
    const std::size_t n = columns.size();
    if (n == 0) return 0.0;

    // The normal equations H d = -g, with H = J'WJ and g = J'Wr. Written out
    // because Eigen's decompositions make this file's debug information huge.
    std::vector<double> h(n * n);
    std::vector<double> g(n);
    for (std::size_t a = 0; a < n; ++a) {
        for (int i = 0; i < m; ++i) g[a] += weights[i] * r[i] * columns[a][i];
        for (std::size_t b = 0; b <= a; ++b) {
            for (int i = 0; i < m; ++i) h[a * n + b] += weights[i] * columns[a][i] * columns[b][i];
        }
    }

    // The Cholesky factor L, leaving out linearly dependent columns: g has no
    // component along them. With L y = g, the predicted decrease is |y|^2 / 2.
    std::vector<double> l(n * n);
    std::vector<double> y(n);
    double decrease = 0.0;
    for (std::size_t a = 0; a < n; ++a) {
        for (std::size_t b = 0; b <= a; ++b) {
            double value = h[a * n + b];
            for (std::size_t c = 0; c < b; ++c) value -= l[a * n + c] * l[b * n + c];
            if (b < a) {
                l[a * n + b] = l[b * n + b] > 0.0 ? value / l[b * n + b] : 0.0;
            } else {
                l[a * n + a] = value > 1e-12 * h[a * n + a] ? std::sqrt(value) : 0.0;
            }
        }
        if (l[a * n + a] > 0.0) {
            double value = g[a];
            for (std::size_t c = 0; c < a; ++c) value -= l[a * n + c] * y[c];
            y[a] = value / l[a * n + a];
            decrease += 0.5 * y[a] * y[a];
        }
    }
    return decrease;
}

// The gradient of the objective in parameter j, and the predicted decrease
// from moving only that parameter.
void parameter_gradient(const double* r, const double* jac, const std::vector<double>& weights,
                        int m, int j, double* gradient, double* gain) {
    const double* column = jac + static_cast<std::size_t>(m) * j;
    double g = 0.0;
    double curvature = 0.0;
    for (int i = 0; i < m; ++i) {
        g += weights[i] * r[i] * column[i];
        curvature += weights[i] * column[i] * column[i];
    }
    *gradient = g;
    *gain = curvature > 0.0 ? 0.5 * g * g / curvature : 0.0;
}

ceres::Solver::Summary solve_stage(double* x, ResidualEvaluator* evaluator,
                                   ceres::LossFunction* loss, const std::vector<double>& lo,
                                   const std::vector<double>& up, const std::vector<bool>& held,
                                   const ceres::Solver::Options& options) {
    const int m = evaluator->num_residuals();
    const int p = evaluator->num_parameters();
    ceres::Problem::Options problem_options;
    problem_options.loss_function_ownership = ceres::DO_NOT_TAKE_OWNERSHIP;
    ceres::Problem problem(problem_options);
    for (int i = 0; i < m; ++i) {
        problem.AddResidualBlock(new ObservationResidual(evaluator, i), loss, x);
    }
    std::vector<int> constant;
    for (int j = 0; j < p; ++j) {
        if (held[j]) {
            constant.push_back(j);
            continue;
        }
        if (std::isfinite(lo[j])) problem.SetParameterLowerBound(x, j, lo[j]);
        if (std::isfinite(up[j])) problem.SetParameterUpperBound(x, j, up[j]);
    }
    if (!constant.empty()) problem.SetManifold(x, new ceres::SubsetManifold(p, constant));
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);
    return summary;
}

}  // namespace

// Ceres handles a bound by projecting the step onto it. Once a parameter is at
// a bound its trust region model is poor, the radius collapses and the minimizer
// can stop, reporting convergence, while the other parameters are still far from
// their optimum. So parameters at an active bound are held there and the others
// solved again, which is an active set method around Ceres. Convergence of a
// bounded problem is decided here, by the predicted decrease at the final point.
//
// [[Rcpp::export]]
Rcpp::List ceres_nls_solve(Rcpp::Function fn, Rcpp::Nullable<Rcpp::Function> jac,
                           Rcpp::NumericVector par, int m, Rcpp::NumericVector lower,
                           Rcpp::NumericVector upper, std::string loss, double loss_scale,
                           bool compute_vcov, Rcpp::List control) {
    const int p = par.size();
    std::vector<double> x(par.begin(), par.end());
    const std::vector<double> lo(lower.begin(), lower.end());
    const std::vector<double> up(upper.begin(), upper.end());

    const std::string method = Rcpp::as<std::string>(control["method"]);
    const std::string diff = Rcpp::as<std::string>(control["diff"]);
    const bool trace = Rcpp::as<bool>(control["trace"]);
    const double function_tol = Rcpp::as<double>(control["function_tol"]);

    RFunctionEvaluator evaluator(m, p, fn, jac, lo, up, diff == "central");
    std::unique_ptr<ceres::LossFunction> loss_function(make_loss(loss, loss_scale));
    StopCallback callback(&evaluator);

    ceres::Solver::Options options;
    // R is not thread-safe. With one thread Ceres evaluates on the calling thread.
    options.num_threads = 1;
    options.linear_solver_type = ceres::DENSE_QR;
    options.trust_region_strategy_type =
        method == "dogleg" ? ceres::DOGLEG : ceres::LEVENBERG_MARQUARDT;
    options.max_num_iterations = Rcpp::as<int>(control["max_iter"]);
    options.function_tolerance = function_tol;
    options.gradient_tolerance = Rcpp::as<double>(control["gradient_tol"]);
    options.parameter_tolerance = Rcpp::as<double>(control["parameter_tol"]);
    options.minimizer_progress_to_stdout = trace;
    options.logging_type = trace ? ceres::PER_MINIMIZER_ITERATION : ceres::SILENT;
    options.callbacks.push_back(&callback);

    // lower == upper fixes a parameter. Ceres itself rejects such bounds.
    std::vector<bool> fixed(p, false);
    std::vector<bool> held(p, false);
    bool has_bounds = false;
    int num_fixed = 0;
    for (int j = 0; j < p; ++j) {
        if (lo[j] == up[j]) {
            fixed[j] = held[j] = true;
            x[j] = lo[j];
            ++num_fixed;
        } else if (std::isfinite(lo[j]) || std::isfinite(up[j])) {
            has_bounds = true;
        }
    }

    std::vector<double> weights(m);
    const double* r = nullptr;
    const double* j_values = nullptr;

    if (!evaluator.residuals(x.data(), &r)) {
        evaluator.rethrow_if_failed();
        Rcpp::stop("'fn' returned non-finite residuals at 'par'");
    }
    const double initial_cost = objective(r, m, loss_function.get(), &weights);
    // what the objective can be expected to change by when nothing is left to do
    const double decrease_tol = std::max(100.0 * function_tol, 1e-6);
    const double decrease_floor = 1e-14 * initial_cost;

    IterationRows rows;
    std::string status = "CONVERGENCE";
    std::string message = "All parameters are fixed.";
    int iterations = 0;
    double solve_time = 0.0;
    double cost = initial_cost;
    const int max_stages = 10 + 2 * p;

    for (int stage = 1; stage <= max_stages; ++stage) {
        const double cost_before = cost;

        if (num_fixed < p && std::find(held.begin(), held.end(), false) != held.end()) {
            const ceres::Solver::Summary summary = solve_stage(
                x.data(), &evaluator, loss_function.get(), lo, up, held, options);
            evaluator.rethrow_if_failed();
            for (const ceres::IterationSummary& it : summary.iterations) {
                rows.push_back(std::make_pair(stage, it));
            }
            iterations += summary.num_successful_steps + summary.num_unsuccessful_steps;
            solve_time += summary.total_time_in_seconds;
            status = ceres::TerminationTypeToString(summary.termination_type);
            message = summary.message;
            if (callback.interrupted || summary.termination_type == ceres::FAILURE) break;
        }
        if (!has_bounds) break;

        // Parameters at a bound are moved exactly onto it
        for (int j = 0; j < p; ++j) {
            if (fixed[j]) continue;
            const double margin = 1e-10 * (1.0 + std::fabs(x[j]));
            if (x[j] - lo[j] <= margin) x[j] = lo[j];
            if (up[j] - x[j] <= margin) x[j] = up[j];
        }
        if (!evaluator.residuals(x.data(), &r) || !evaluator.jacobian(x.data(), &j_values)) {
            evaluator.rethrow_if_failed();
            status = "FAILURE";
            message = "Residuals or Jacobian could not be evaluated at the solution.";
            break;
        }
        cost = objective(r, m, loss_function.get(), &weights);
        const double threshold = decrease_tol * cost + decrease_floor;

        // Hold parameters pushed against a bound, release those pulled away from it
        bool changed = false;
        for (int j = 0; j < p; ++j) {
            if (fixed[j]) continue;
            double gradient = 0.0;
            double gain = 0.0;
            parameter_gradient(r, j_values, weights, m, j, &gradient, &gain);
            const bool at_lower = x[j] == lo[j];
            const bool at_upper = x[j] == up[j];
            if (!held[j]) {
                if ((at_upper && gradient < 0.0) || (at_lower && gradient > 0.0)) {
                    held[j] = true;
                    changed = true;
                }
            } else if (gain > threshold &&
                       ((at_upper && gradient > 0.0) || (at_lower && gradient < 0.0))) {
                held[j] = false;
                changed = true;
            }
        }
        if (changed) continue;

        if (predicted_decrease(r, j_values, weights, held, m, p) <= threshold) {
            status = "CONVERGENCE";
            message = "Converged, with any parameters at a bound held there.";
            break;
        }
        // Not there yet. A new stage restarts with a fresh trust region, which is
        // only worth repeating while it makes progress.
        const bool progress = stage == 1 || cost < cost_before * (1.0 - 1e-12);
        if (!progress || stage == max_stages) {
            status = "NO_CONVERGENCE";
            message = "The solver stopped before reaching a solution of the bounded problem.";
            break;
        }
    }

    // Residuals and Jacobian at the solution
    Rcpp::RObject residuals_out = R_NilValue;
    Rcpp::RObject jacobian_out = R_NilValue;
    if (evaluator.residuals(x.data(), &r)) {
        residuals_out = Rcpp::NumericVector(r, r + m);
        cost = objective(r, m, loss_function.get(), &weights);
    }
    if (evaluator.jacobian(x.data(), &j_values)) {
        Rcpp::NumericMatrix j_out(m, p);
        std::copy(j_values, j_values + static_cast<std::size_t>(m) * p, j_out.begin());
        jacobian_out = j_out;
    }
    evaluator.rethrow_if_failed();

    // (J'J)^-1, which ceres_nls() scales by sigma^2. Only the parameters with
    // lower == upper are constant here: the others are estimates, even at a bound.
    Rcpp::RObject unscaled_out = R_NilValue;
    if (compute_vcov && !callback.interrupted && !Rf_isNull(jacobian_out)) {
        ceres::Problem problem;
        for (int i = 0; i < m; ++i) {
            problem.AddResidualBlock(new ObservationResidual(&evaluator, i), nullptr, x.data());
        }
        if (num_fixed > 0) {
            std::vector<int> constant;
            for (int j = 0; j < p; ++j) {
                if (fixed[j]) constant.push_back(j);
            }
            problem.SetManifold(x.data(), new ceres::SubsetManifold(p, constant));
        }
        ceres::Covariance::Options cov_options;
        cov_options.algorithm_type = ceres::DENSE_SVD;
        cov_options.num_threads = 1;
        ceres::Covariance covariance(cov_options);
        std::vector<std::pair<const double*, const double*>> blocks;
        blocks.push_back(std::make_pair(x.data(), x.data()));
        if (covariance.Compute(blocks, &problem)) {
            std::vector<double> cov(static_cast<std::size_t>(p) * p);
            covariance.GetCovarianceBlock(x.data(), x.data(), cov.data());
            Rcpp::NumericMatrix unscaled(p, p);
            for (int i = 0; i < p; ++i) {
                for (int j = 0; j < p; ++j) unscaled(i, j) = cov[i * p + j];
            }
            unscaled_out = unscaled;
        }
        evaluator.rethrow_if_failed();
    }

    return Rcpp::List::create(
        Rcpp::Named("par") = Rcpp::NumericVector(x.begin(), x.end()),
        Rcpp::Named("residuals") = residuals_out, Rcpp::Named("jacobian") = jacobian_out,
        Rcpp::Named("unscaled_vcov") = unscaled_out, Rcpp::Named("cost") = cost,
        Rcpp::Named("initial_cost") = initial_cost, Rcpp::Named("status") = status,
        Rcpp::Named("message") = message, Rcpp::Named("interrupted") = callback.interrupted,
        Rcpp::Named("iterations") = iterations, Rcpp::Named("fn_calls") = evaluator.fn_calls(),
        Rcpp::Named("jac_calls") = evaluator.jac_calls(), Rcpp::Named("time") = solve_time,
        Rcpp::Named("trace") = make_trace(rows));
}
