test_that("robust_smoothing.cpp smooths through outliers", {
    ex <- source_example("robust_smoothing.cpp")
    set.seed(1)
    t <- seq(0, 4 * pi, length.out = 2000)
    y <- sin(t) + rnorm(length(t), sd = 0.1)
    out <- sample(length(t), 50)
    y[out] <- y[out] + rnorm(50, sd = 3)

    fit <- ex$robust_smooth(y, lambda = 50, num_threads = 2)
    expect_true(fit$converged)
    expect_length(fit$s, length(y))
    expect_lt(max(abs(fit$s - sin(t))), 0.25)
    expect_lt(mean(abs(fit$s - sin(t))), mean(abs(y - sin(t))))
})
