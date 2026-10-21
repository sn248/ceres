## Compiler and linker flags for code that uses the Ceres Solver bundled
## with this package, either via Rcpp::sourceCpp() or from another
## package that has 'LinkingTo: ceres'.

.ceresLibDir <- function() {
    system.file("lib", package = "ceres")
}

.ceresIncludeDir <- function() {
    system.file("include", package = "ceres")
}

CxxFlags <- function(print = TRUE) {
    flags <- paste0("-I", shQuote(.ceresIncludeDir()))
    if (print) cat(flags)
    invisible(flags)
}

LdFlags <- function(print = TRUE) {
    ## link the static archive directly so a shared libceres on the
    ## system can never be picked up instead
    libs <- file.path(.ceresLibDir(), "libceres.a")
    flags <- paste(c(shQuote(libs), "-lpthread"), collapse = " ")
    if (print) cat(flags)
    invisible(flags)
}

## Plugin used by Rcpp::sourceCpp() for '// [[Rcpp::depends(ceres)]]'
inlineCxxPlugin <- function(...) {
    plugin <- Rcpp::Rcpp.plugin.maker(
        include.before = "#include <ceres.h>",
        LinkingTo = c("ceres", "RcppEigen", "Rcpp"),
        libs = LdFlags(print = FALSE),
        package = "ceres"
    )
    settings <- plugin()
    settings$env$USE_CXX17 <- "yes"
    settings
}
