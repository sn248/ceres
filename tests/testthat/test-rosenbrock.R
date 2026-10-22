test_that("rosenbrock.cpp finds the minimum", {
    ex <- source_example("rosenbrock.cpp")
    for (method in c("auto", "numeric", "analytic")) {
        fit <- ex$minimize_rosenbrock(c(-1.2, 1), method = method)
        expect_true(fit$converged, label = method)
        expect_equal(fit$par, c(1, 1), tolerance = 1e-6, label = method)
        expect_lt(fit$value, 1e-10)
    }
    for (direction in c("BFGS", "NONLINEAR_CONJUGATE_GRADIENT")) {
        fit <- ex$minimize_rosenbrock(c(-1.2, 1), direction = direction)
        expect_equal(fit$par, c(1, 1), tolerance = 1e-4, label = direction)
    }
    expect_error(ex$minimize_rosenbrock(c(-1.2, 1), direction = "foo"), "direction")
    expect_error(ex$minimize_rosenbrock(1), "length 2")
})
