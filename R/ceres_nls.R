## Non-linear least squares with residuals computed by an R function

ceres_control <- function(max_iter = 50, function_tol = 1e-10, gradient_tol = 1e-10,
                          parameter_tol = 1e-10, method = c("lm", "dogleg"),
                          diff = c("central", "forward"), trace = FALSE) {
    is_number <- function(x) is.numeric(x) && length(x) == 1L && is.finite(x)
    if (!is_number(max_iter) || max_iter < 1 || max_iter != round(max_iter)) {
        stop("'max_iter' must be a positive whole number")
    }
    for (name in c("function_tol", "gradient_tol", "parameter_tol")) {
        value <- get(name)
        if (!is_number(value) || value < 0) stop("'", name, "' must be a non-negative number")
    }
    if (!is.logical(trace) || length(trace) != 1L || is.na(trace)) {
        stop("'trace' must be TRUE or FALSE")
    }
    structure(
        list(max_iter = as.integer(max_iter), function_tol = function_tol,
             gradient_tol = gradient_tol, parameter_tol = parameter_tol,
             method = match.arg(method), diff = match.arg(diff), trace = trace),
        class = "ceres_control")
}

## A bound of length 1, of length p, or named by (some of) the parameters
.ceres_bound <- function(bound, par, default, arg) {
    if (!is.numeric(bound) || anyNA(bound)) stop("'", arg, "' must be numeric without NAs")
    if (!is.null(names(bound))) {
        unknown <- setdiff(names(bound), names(par))
        if (is.null(names(par)) || length(unknown) > 0L || !all(nzchar(names(bound)))) {
            stop("the names of '", arg, "' must be names of 'par'")
        }
        out <- rep(default, length(par))
        out[match(names(bound), names(par))] <- bound
        return(as.double(out))
    }
    if (length(bound) == 1L) return(rep(as.double(bound), length(par)))
    if (length(bound) != length(par)) stop("'", arg, "' must have length 1 or length(par)")
    as.double(bound)
}

ceres_nls <- function(fn, par, ..., jac = NULL, lower = -Inf, upper = Inf,
                      loss = c("linear", "huber", "soft_l1", "cauchy", "arctan", "tukey"),
                      loss_scale = 1, vcov = TRUE, control = ceres_control()) {
    call <- match.call()
    fn <- match.fun(fn)
    if (!is.null(jac)) jac <- match.fun(jac)
    if (!is.numeric(par) || length(par) == 0L || !all(is.finite(par))) {
        stop("'par' must be a non-empty numeric vector of finite values")
    }
    if (!is.null(names(par)) && (anyDuplicated(names(par)) || !all(nzchar(names(par))))) {
        stop("the names of 'par' must be unique and non-empty")
    }
    loss <- match.arg(loss)
    if (!is.numeric(loss_scale) || length(loss_scale) != 1L || !is.finite(loss_scale) ||
        loss_scale <= 0) {
        stop("'loss_scale' must be a positive number")
    }
    if (!is.logical(vcov) || length(vcov) != 1L || is.na(vcov)) {
        stop("'vcov' must be TRUE or FALSE")
    }
    if (!inherits(control, "ceres_control")) stop("'control' must come from ceres_control()")

    par_names <- names(par)
    p <- length(par)
    lower <- .ceres_bound(lower, par, -Inf, "lower")
    upper <- .ceres_bound(upper, par, Inf, "upper")
    if (any(lower > upper)) stop("'lower' must not be greater than 'upper'")
    start <- as.double(par)
    if (any(start < lower | start > upper)) stop("'par' must be within 'lower' and 'upper'")

    with_names <- function(x) {
        names(x) <- par_names
        x
    }
    r0 <- fn(with_names(start), ...)
    if (!is.numeric(r0) || length(r0) == 0L) {
        stop("'fn' must return a non-empty numeric vector of residuals")
    }
    if (!all(is.finite(r0))) stop("'fn' returned non-finite residuals at 'par'")
    m <- length(r0)
    if (m < p) stop("'fn' returned ", m, " residuals for ", p, " parameters; need at least ", p)
    residual_names <- names(r0)

    fn_checked <- function(x) {
        r <- fn(with_names(x), ...)
        if (!is.numeric(r) || length(r) != m) {
            stop("'fn' must always return ", m, " residuals; it returned ", length(r),
                 call. = FALSE)
        }
        as.double(r)
    }
    jac_checked <- if (!is.null(jac)) function(x) {
        J <- jac(with_names(x), ...)
        if (!is.numeric(J) || !is.matrix(J) || nrow(J) != m || ncol(J) != p) {
            stop("'jac' must return a numeric ", m, " x ", p, " matrix", call. = FALSE)
        }
        storage.mode(J) <- "double"
        J
    }

    ## lower == upper fixes a parameter, so it is not an estimate
    fixed <- lower == upper
    n_estimated <- sum(!fixed)

    res <- ceres_nls_solve(fn_checked, jac_checked, start, m, lower, upper, loss,
                           as.double(loss_scale),
                           vcov && loss == "linear" && m > n_estimated && n_estimated > 0L,
                           unclass(control))

    estimate <- with_names(res$par)
    residuals <- res$residuals
    if (!is.null(residuals)) names(residuals) <- residual_names
    jacobian <- res$jacobian
    if (!is.null(jacobian)) dimnames(jacobian) <- list(residual_names, par_names)
    rss <- if (!is.null(residuals)) sum(residuals^2) else NA_real_

    converged <- identical(res$status, "CONVERGENCE")
    if (res$interrupted) {
        warning("solve interrupted; returning the current point", call. = FALSE)
    } else if (!converged) {
        warning("ceres_nls() did not converge (", res$status, "): ", res$message, call. = FALSE)
    }

    at_bound <- abs(res$par - lower) <= 1e-8 * (1 + abs(res$par)) |
        abs(upper - res$par) <= 1e-8 * (1 + abs(res$par))
    at_bound[(!is.finite(lower) & !is.finite(upper)) | fixed] <- FALSE

    vcov_matrix <- NULL
    if (vcov && !res$interrupted) {
        if (loss != "linear") {
            message("'vcov' is not available with a robust loss: the residual variance ",
                    "estimate assumes the linear loss")
        } else if (n_estimated == 0L) {
            message("'vcov' is not available: all parameters are fixed")
        } else if (m == n_estimated) {
            message("'vcov' is not available: there are no residual degrees of freedom")
        } else if (is.null(res$unscaled_vcov)) {
            warning("'vcov' could not be computed; the Jacobian may be rank deficient",
                    call. = FALSE)
        } else {
            vcov_matrix <- res$unscaled_vcov * rss / (m - n_estimated)
            dimnames(vcov_matrix) <- list(par_names, par_names)
            if (any(at_bound)) {
                warning("'vcov' is unreliable: parameter(s) ",
                        paste(if (is.null(par_names)) which(at_bound) else par_names[at_bound],
                              collapse = ", "),
                        " at a bound", call. = FALSE)
            }
        }
    }

    structure(
        list(par = estimate, residuals = residuals, jacobian = jacobian, vcov = vcov_matrix,
             cost = res$cost, initial_cost = res$initial_cost, rss = rss,
             converged = converged, status = res$status, message = res$message,
             iterations = res$iterations,
             n_evals = c(fn = res$fn_calls + 1L, jac = res$jac_calls),
             time = res$time, trace = res$trace, at_bound = with_names(at_bound),
             fixed = with_names(fixed),
             loss = loss, loss_scale = loss_scale, n = m, call = call),
        class = "ceres_nls")
}

coef.ceres_nls <- function(object, ...) object$par

residuals.ceres_nls <- function(object, ...) object$residuals

vcov.ceres_nls <- function(object, ...) object$vcov

deviance.ceres_nls <- function(object, ...) object$rss

df.residual.ceres_nls <- function(object, ...) object$n - sum(!object$fixed)

print.ceres_nls <- function(x, digits = max(3L, getOption("digits") - 3L), ...) {
    cat("Non-linear least squares fit by Ceres Solver\n\n")
    print(format(x$par, digits = digits), quote = FALSE)
    cat("\nLoss: ", x$loss, if (x$loss != "linear") paste0(" (scale ", format(x$loss_scale), ")"),
        "\nResidual sum of squares: ", format(x$rss, digits = digits),
        " on ", df.residual(x), " degrees of freedom",
        "\nStatus: ", x$status, " after ", x$iterations, " iterations\n", sep = "")
    invisible(x)
}

summary.ceres_nls <- function(object, ...) {
    est <- object$par
    df <- df.residual(object)
    coefficients <- if (!is.null(object$vcov)) {
        se <- sqrt(diag(object$vcov))
        tval <- ifelse(object$fixed, NA_real_, est / se)
        cbind(Estimate = est, `Std. Error` = se, `t value` = tval,
              `Pr(>|t|)` = 2 * stats::pt(abs(tval), df, lower.tail = FALSE))
    } else {
        cbind(Estimate = est)
    }
    sigma <- if (object$loss == "linear" && df > 0) sqrt(object$rss / df) else NA_real_
    structure(
        list(call = object$call, coefficients = coefficients, sigma = sigma, df = df,
             rss = object$rss, cost = object$cost, loss = object$loss,
             loss_scale = object$loss_scale, converged = object$converged,
             status = object$status, message = object$message, iterations = object$iterations,
             n_evals = object$n_evals, at_bound = object$at_bound),
        class = "summary.ceres_nls")
}

print.summary.ceres_nls <- function(x, digits = max(3L, getOption("digits") - 3L), ...) {
    cat("Call:\n", paste(deparse(x$call), collapse = "\n"), "\n\nCoefficients:\n", sep = "")
    if (ncol(x$coefficients) > 1L) {
        stats::printCoefmat(x$coefficients, digits = digits, P.values = TRUE, has.Pvalue = TRUE)
    } else {
        print(x$coefficients, digits = digits)
        cat("(no standard errors: 'vcov' was not computed)\n")
    }
    if (any(x$at_bound)) cat("\nAt a bound: ", paste(names(which(x$at_bound)), collapse = ", "),
                             "\n", sep = "")
    cat("\nLoss: ", x$loss, if (x$loss != "linear") paste0(" (scale ", format(x$loss_scale), ")"),
        "\n", sep = "")
    if (!is.na(x$sigma)) {
        cat("Residual standard error: ", format(x$sigma, digits = digits), " on ", x$df,
            " degrees of freedom\n", sep = "")
    }
    cat("Residual sum of squares: ", format(x$rss, digits = digits), "\n",
        "Status: ", x$status, " after ", x$iterations, " iterations (", x$n_evals[["fn"]],
        " fn calls, ", x$n_evals[["jac"]], " jac calls)\n", sep = "")
    invisible(x)
}
