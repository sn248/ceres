test_that("derivatives.cpp gives the same answer for each method", {
    ex <- source_example("derivatives.cpp")
    for (method in c("auto", "numeric", "analytic")) {
        fit <- ex$hello_derivatives(0.5, method = method)
        expect_true(fit$converged, label = method)
        expect_equal(fit$x, 10, tolerance = 1e-6, label = method)
    }
    expect_error(ex$hello_derivatives(0.5, method = "foo"), "'method'")
})
