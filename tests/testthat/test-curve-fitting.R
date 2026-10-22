test_that("curve_fitting.cpp recovers the exponential model", {
    ex <- source_example("curve_fitting.cpp")
    set.seed(1)
    x <- seq(0, 5, length.out = 1000)
    y <- exp(0.3 * x + 0.1) + rnorm(length(x), sd = 0.2)

    fit <- ex$fit_exponential(x, y, num_threads = 2)
    expect_true(fit$converged)
    expect_equal(fit$m, 0.3, tolerance = 0.01)
    expect_equal(fit$c, 0.1, tolerance = 0.05)
    expect_equal(fit$threads_used, 2L)

    expect_error(ex$fit_exponential(1:3, 1:2), "same length")
})
