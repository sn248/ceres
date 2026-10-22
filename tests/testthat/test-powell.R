test_that("powell.cpp converges to the origin", {
    ex <- source_example("powell.cpp")
    fit <- ex$powell(c(3, -1, 0, 1))
    expect_true(fit$converged)
    expect_equal(fit$x, rep(0, 4), tolerance = 1e-3)
    expect_lt(fit$final_cost, 1e-10)
    expect_error(ex$powell(1:3), "length 4")
})
