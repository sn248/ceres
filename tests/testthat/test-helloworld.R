test_that("ceres_helloworld() solves the tutorial problem", {
    expect_output(ceres_helloworld(0.5), "x : 0.5 -> 10")
})
