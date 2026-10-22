# Compile one of the files in inst/examples into a fresh environment and
# return that environment. Compiling against Ceres and Eigen is slow, so
# these tests do not run on CRAN.
source_example <- function(file) {
    skip_on_cran()
    path <- system.file("examples", file, package = "ceres")
    if (!nzchar(path)) stop("example not found: ", file)
    env <- new.env()
    Rcpp::sourceCpp(path, env = env, showOutput = FALSE)
    env
}
