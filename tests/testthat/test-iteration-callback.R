test_that("iteration_callback.cpp records the solver trace", {
    ex <- source_example("iteration_callback.cpp")
    set.seed(1)
    x <- seq(0, 5, length.out = 1000)
    y <- exp(0.3 * x + 0.1) + rnorm(length(x), sd = 0.2)

    expect_silent(fit <- ex$fit_exponential_traced(x, y, num_threads = 2))
    expect_true(fit$converged)
    expect_false(fit$interrupted)

    trace <- fit$trace
    expect_s3_class(trace, "data.frame")
    expect_named(trace, c("iteration", "m", "c", "cost",
                          "gradient_max_norm", "step_accepted"))
    expect_equal(trace$iteration, seq_len(nrow(trace)) - 1L)
    expect_equal(trace$m[1], 0)
    expect_equal(trace$m[nrow(trace)], fit$m)
    expect_equal(trace$c[nrow(trace)], fit$c)
    expect_equal(trace$cost[nrow(trace)], fit$final_cost)

    expect_output(ex$fit_exponential_traced(x, y, print = TRUE), "iter 0")
})
