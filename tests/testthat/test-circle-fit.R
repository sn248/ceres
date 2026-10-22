test_that("circle_fit.cpp fits a circle, robustly with outliers", {
    ex <- source_example("circle_fit.cpp")
    set.seed(1)
    theta <- runif(200, 0, 2 * pi)
    x <- 2 + 3 * cos(theta) + rnorm(200, sd = 0.1)
    y <- -1 + 3 * sin(theta) + rnorm(200, sd = 0.1)
    x[1:20] <- runif(20, -2, 6)
    y[1:20] <- runif(20, -5, 3)

    plain <- ex$fit_circle(x, y)
    robust <- ex$fit_circle(x, y, robust_threshold = 1)
    expect_true(robust$converged)
    expect_equal(c(robust$x, robust$y, robust$r), c(2, -1, 3), tolerance = 0.02)
    expect_lt(abs(robust$r - 3), abs(plain$r - 3))

    expect_error(ex$fit_circle(1:2, 1:2), "three points")
})
