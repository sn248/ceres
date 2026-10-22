test_that("covariance.cpp matches vcov() of an nls() fit", {
    ex <- source_example("covariance.cpp")
    set.seed(1)
    x <- seq(0, 5, length.out = 200)
    y <- exp(0.3 * x + 0.1) + rnorm(length(x), sd = 0.2)

    fit <- ex$fit_exponential_vcov(x, y)
    nfit <- nls(y ~ exp(m * x + c), start = list(m = 0.2, c = 0))
    expect_true(fit$converged)
    expect_equal(fit$coefficients, coef(nfit), tolerance = 1e-3)
    expect_equal(fit$vcov, vcov(nfit), tolerance = 1e-4)
    expect_equal(fit$std_error, sqrt(diag(vcov(nfit))), tolerance = 1e-4)
    expect_equal(fit$sigma, sigma(nfit), tolerance = 1e-4)
})
