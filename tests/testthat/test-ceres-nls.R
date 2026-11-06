set.seed(42)
x <- seq(0, 5, length.out = 100)
y <- exp(0.3 * x + 0.1) + rnorm(length(x), sd = 0.05)
resid_exp <- function(p, x, y) y - exp(p[["m"]] * x + p[["c"]])
jac_exp <- function(p, x, y) {
    f <- exp(p[["m"]] * x + p[["c"]])
    cbind(m = -x * f, c = -f)
}
start <- c(m = 0, c = 0)

test_that("ceres_nls() matches lm() exactly on a linear model", {
    # lm() does not iterate, so this reference does not depend on a tolerance
    fit <- ceres_nls(function(p, x, y) y - p[["a"]] - p[["b"]] * x, c(a = 0, b = 0),
                     x = x, y = y,
                     control = ceres_control(function_tol = 1e-14, parameter_tol = 1e-14))
    ref <- lm(y ~ x)

    expect_true(fit$converged)
    expect_equal(unname(coef(fit)), unname(coef(ref)), tolerance = 1e-8)
    expect_equal(deviance(fit), deviance(ref), tolerance = 1e-8)
    expect_equal(df.residual(fit), df.residual(ref))
    expect_equal(unname(residuals(fit)), unname(residuals(ref)), tolerance = 1e-6)
    expect_equal(unname(vcov(fit)), unname(vcov(ref)), tolerance = 1e-7)
})

test_that("ceres_nls() matches nls() on clean data", {
    tight <- ceres_control(function_tol = 1e-12, parameter_tol = 1e-12)
    fit <- ceres_nls(resid_exp, start, x = x, y = y, control = tight)
    # default nls() settings: tighter ones fail to converge on some platforms
    ref <- nls(y ~ exp(m * x + c), start = list(m = 0.2, c = 0))

    expect_true(fit$converged)
    expect_s3_class(fit, "ceres_nls")
    expect_named(coef(fit), c("m", "c"))
    expect_equal(coef(fit), coef(ref), tolerance = 1e-4)
    expect_equal(deviance(fit), deviance(ref), tolerance = 1e-6)
    expect_equal(df.residual(fit), df.residual(ref))
    expect_equal(unname(residuals(fit)), as.numeric(residuals(ref)), tolerance = 1e-3)
    expect_equal(vcov(fit), vcov(ref), tolerance = 1e-3)
    expect_equal(dim(fit$jacobian), c(100L, 2L))
    expect_s3_class(fit$trace, "data.frame")
    expect_gt(nrow(fit$trace), 1)
})

test_that("analytic and numeric Jacobians agree", {
    numeric_fit <- ceres_nls(resid_exp, start, x = x, y = y)
    forward_fit <- ceres_nls(resid_exp, start, x = x, y = y,
                             control = ceres_control(diff = "forward"))
    analytic_fit <- ceres_nls(resid_exp, start, jac = jac_exp, x = x, y = y)

    expect_equal(coef(numeric_fit), coef(analytic_fit), tolerance = 1e-5)
    expect_equal(coef(forward_fit), coef(analytic_fit), tolerance = 1e-5)
    expect_equal(unname(numeric_fit$jacobian), unname(analytic_fit$jacobian), tolerance = 1e-6)
    expect_gt(analytic_fit$n_evals[["jac"]], 0)
    expect_equal(numeric_fit$n_evals[["jac"]], 0)
})

test_that("fn is called once per point, not once per residual block", {
    calls <- 0
    counted <- function(p, x, y) {
        calls <<- calls + 1
        resid_exp(p, x, y)
    }
    fit <- ceres_nls(counted, start, jac = jac_exp, x = x, y = y)
    expect_equal(calls, fit$n_evals[["fn"]])
    expect_lt(calls, 3 * (nrow(fit$trace) + 2))
})

test_that("robust losses down-weight individual outliers", {
    y_out <- y
    y_out[c(10, 40, 70, 90)] <- y_out[c(10, 40, 70, 90)] + 8
    true <- c(m = 0.3, c = 0.1)

    plain <- ceres_nls(resid_exp, start, x = x, y = y_out)
    plain_error <- sum(abs(coef(plain) - true))

    for (loss in c("huber", "soft_l1", "cauchy", "arctan", "tukey")) {
        fit <- suppressMessages(
            ceres_nls(resid_exp, c(m = 0.2, c = 0.2), x = x, y = y_out, loss = loss,
                      loss_scale = 0.2))
        expect_lt(sum(abs(coef(fit) - true)), plain_error / 2, label = loss)
        expect_equal(fit$loss, loss)
        # cost includes the loss, rss does not
        expect_lt(fit$cost, fit$rss / 2)
    }
})

test_that("cost matches the loss formulas in the documentation", {
    a <- 0.7
    rho <- list(
        huber = function(s) ifelse(s <= a^2, s, 2 * a * sqrt(s) - a^2),
        soft_l1 = function(s) 2 * a^2 * (sqrt(1 + s / a^2) - 1),
        cauchy = function(s) a^2 * log(1 + s / a^2),
        arctan = function(s) a * atan(s / a),
        tukey = function(s) ifelse(s <= a^2, a^2 / 3 * (1 - (1 - s / a^2)^3), a^2 / 3)
    )
    # no iterations: the residuals are those at the starting value
    r <- c(0.1, -0.3, 0.6, -0.9, 2, 5)
    for (loss in names(rho)) {
        fit <- suppressWarnings(suppressMessages(
            ceres_nls(function(p) r + 0 * p, 1, jac = function(p) matrix(0, 6, 1),
                      loss = loss, loss_scale = a)))
        expect_equal(fit$initial_cost, 0.5 * sum(rho[[loss]](r^2)), tolerance = 1e-10,
                     label = loss)
    }
})

test_that("a robust loss gives no vcov, with a message", {
    expect_message(
        fit <- ceres_nls(resid_exp, start, x = x, y = y, loss = "huber"),
        "not available with a robust loss")
    expect_null(vcov(fit))
    expect_output(print(summary(fit)), "no standard errors")
    expect_silent(ceres_nls(resid_exp, start, x = x, y = y, loss = "huber", vcov = FALSE))
})

test_that("bounds hold and fn is never evaluated outside them", {
    lower <- c(m = 0, c = -Inf)
    upper <- c(m = 0.2, c = Inf)
    seen <- matrix(numeric(), ncol = 2)
    recording <- function(p, x, y) {
        seen <<- rbind(seen, p)
        resid_exp(p, x, y)
    }
    expect_warning(
        fit <- ceres_nls(recording, c(m = 0.1, c = 0), x = x, y = y,
                         lower = lower, upper = upper),
        "at a bound")
    expect_equal(coef(fit)[["m"]], 0.2)
    expect_true(fit$at_bound[["m"]])
    expect_false(fit$at_bound[["c"]])
    expect_true(all(seen[, 1] >= 0 & seen[, 1] <= 0.2))
    expect_gt(nrow(seen), 3)

    # starting exactly on a bound, where a central difference would step outside
    seen <- matrix(numeric(), ncol = 2)
    suppressWarnings(
        ceres_nls(recording, c(m = 0, c = 0), x = x, y = y, lower = lower, upper = upper))
    expect_true(all(seen[, 1] >= 0 & seen[, 1] <= 0.2))
})

test_that("log(p) at a lower bound does not fail", {
    z <- seq(1, 10, length.out = 30)
    w <- log(0.5 * z)
    fit <- ceres_nls(function(p) w - log(p[["a"]] * z), c(a = 1), lower = c(a = 1e-8))
    expect_equal(coef(fit)[["a"]], 0.5, tolerance = 1e-5)
})

test_that("bounds can be scalars, full vectors or named subsets", {
    a <- ceres_nls(resid_exp, start, x = x, y = y, lower = -1, upper = 1)
    b <- ceres_nls(resid_exp, start, x = x, y = y, lower = c(-1, -1), upper = c(1, 1))
    expect_equal(coef(a), coef(b))
    expect_error(ceres_nls(resid_exp, start, x = x, y = y, lower = c(z = 0)), "names of 'lower'")
    expect_error(ceres_nls(resid_exp, start, x = x, y = y, lower = c(0, 0, 0)), "length")
    expect_error(ceres_nls(resid_exp, start, x = x, y = y, lower = 1, upper = 0), "greater")
    expect_error(ceres_nls(resid_exp, start, x = x, y = y, lower = 1), "within")
})

test_that("a parameter with lower == upper stays fixed", {
    fit <- suppressWarnings(
        ceres_nls(resid_exp, c(m = 0.25, c = 0), x = x, y = y,
                  lower = c(m = 0.25), upper = c(m = 0.25)))
    expect_equal(coef(fit)[["m"]], 0.25)
})

test_that("unnamed parameters work", {
    fit <- ceres_nls(function(p) y - exp(p[1] * x + p[2]), c(0, 0))
    expect_null(names(coef(fit)))
    expect_true(fit$converged)
    expect_equal(dim(vcov(fit)), c(2L, 2L))
})

test_that("errors in fn and jac reach the caller", {
    calls <- 0
    fails_later <- function(p, x, y) {
        calls <<- calls + 1
        if (calls > 3) stop("boom from fn")
        resid_exp(p, x, y)
    }
    expect_error(ceres_nls(fails_later, start, x = x, y = y), "boom from fn")
    # no further calls into R after the error
    expect_equal(calls, 4)

    expect_error(
        ceres_nls(resid_exp, start, jac = function(p, x, y) stop("boom from jac"), x = x, y = y),
        "boom from jac")
    expect_error(ceres_nls(function(p) stop("boom at start"), start), "boom at start")
})

test_that("a working solve still works after an error", {
    expect_error(ceres_nls(function(p) stop("boom"), start))
    expect_true(ceres_nls(resid_exp, start, x = x, y = y)$converged)
})

test_that("bad return values are reported", {
    expect_error(ceres_nls(function(p) "a", start), "numeric vector")
    expect_error(ceres_nls(function(p) NA_real_ + p, start), "non-finite")
    expect_error(ceres_nls(function(p) 1, start), "1 residuals for 2 parameters")

    calls <- 0
    changes_length <- function(p) {
        calls <<- calls + 1
        if (calls > 1) rep(1, 3) else rep(1, 4)
    }
    expect_error(ceres_nls(changes_length, start), "always return 4 residuals")
    expect_error(ceres_nls(resid_exp, start, jac = function(p, x, y) diag(2), x = x, y = y),
                 "100 x 2 matrix")
})

test_that("non-finite residuals during the solve are rejected steps, not errors", {
    # log() is NaN for a trial step below zero
    z <- seq(1, 10, length.out = 30)
    w <- log(0.5 * z)
    fit <- suppressWarnings(ceres_nls(function(p) w - log(p[["a"]] * z), c(a = 50)))
    expect_true(fit$converged)
    expect_equal(coef(fit)[["a"]], 0.5, tolerance = 1e-4)
})

test_that("hitting max_iter warns and returns the best point", {
    expect_warning(
        fit <- ceres_nls(resid_exp, start, x = x, y = y, control = ceres_control(max_iter = 1)),
        "did not converge")
    expect_false(fit$converged)
    expect_equal(fit$status, "NO_CONVERGENCE")
    expect_lte(fit$cost, fit$initial_cost)
})

test_that("a rank deficient Jacobian gives no vcov, with a warning", {
    expect_warning(
        fit <- ceres_nls(function(p) y - exp(p[["a"]] + p[["b"]]), c(a = 0, b = 0)),
        "rank deficient")
    expect_null(vcov(fit))
})

test_that("m == p has no vcov", {
    expect_message(
        fit <- ceres_nls(function(p) c(p[1] - 1, p[2] - 2), c(0, 0)),
        "no residual degrees of freedom")
    expect_null(vcov(fit))
    expect_equal(unname(coef(fit)), c(1, 2), tolerance = 1e-6)
})

test_that("the dogleg method works", {
    fit <- ceres_nls(resid_exp, start, x = x, y = y, control = ceres_control(method = "dogleg"))
    expect_true(fit$converged)
    expect_equal(coef(fit)[["m"]], 0.3, tolerance = 0.02)
})

test_that("print and summary methods work", {
    fit <- ceres_nls(resid_exp, start, x = x, y = y)
    expect_output(print(fit), "Residual sum of squares")
    expect_output(print(summary(fit)), "Std. Error")
    expect_equal(colnames(summary(fit)$coefficients),
                 c("Estimate", "Std. Error", "t value", "Pr(>|t|)"))
    expect_equal(summary(fit)$coefficients[, "Std. Error"], sqrt(diag(vcov(fit))))
})

test_that("invalid arguments are rejected", {
    expect_error(ceres_nls(resid_exp, c(m = NA, c = 0)), "finite")
    expect_error(ceres_nls(resid_exp, c(a = 0, a = 0)), "unique")
    expect_error(ceres_nls(resid_exp, start, loss = "nope"))
    expect_error(ceres_nls(resid_exp, start, loss_scale = -1), "positive")
    expect_error(ceres_nls(resid_exp, start, control = list()), "ceres_control")
    expect_error(ceres_control(max_iter = 0), "max_iter")
    expect_error(ceres_control(function_tol = -1), "function_tol")
    expect_error(ceres_control(method = "nope"))
})
