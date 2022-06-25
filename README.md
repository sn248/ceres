# `ceres`

`ceres` is an `R` package. `ceres` is an interface to the [Ceres Solver](http://ceres-solver.org/), an open source `C++` library for modeling and solving large non-linear, least squares optimization problems by Google. Ceres solver has been used in production at Google since 2010.

## Installing `ceres`
`ceres` package is not on `CRAN` yet. It can be installed in `R` from github using the following command in the `R` console.

```
devtools::install_github('sn248/ceres')
```

## How to use `ceres`
Following the example provided by [Ceres Solver](http://ceres-solver.org/), let's solve the simple optimization problem of finding the minima of the function
$$ f(x) = (10 - x)^{2}$$
The minima of this function is 0 (at $x = 10$).

See the documentation [here](hhttp://ceres-solver.org/nnls_tutorial.html#hello-world) to see the `C++` implementation of this problem and it's solution. 

Using the example code from the link above, the same problem can be solved in `R` using the code below. Paste this code in a file and save with `.cpp` extension. Use `Rcpp::sourceCpp()` function (with `<filename>`) as the argument of the `Source`  button in `RStudio` to generate a function `ceres_helloworld` in the `R` environment.

```
#include <ceres.h>

// [[Rcpp::depends(RcppEigen, ceres)]]

#include <ceres/ceres.h>
// #include <glog/logging.h>

using namespace Rcpp;

using ceres::AutoDiffCostFunction;
using ceres::CostFunction;
using ceres::Problem;
using ceres::Solve;
using ceres::Solver;

// A templated cost functor that implements the residual r = 10 -
// x. The method operator() is templated so that we can then use an
// automatic differentiation wrapper around it to generate its
// derivatives.
struct CostFunctor {
	template <typename T>
	bool operator()(const T* const x, T* residual) const {
		residual[0] = 10.0 - x[0];
		return true;
	}
};

// [[Rcpp::export]]
int ceres_helloworld(double x){
	// int main(int argc, char** argv) {
	//google::InitGoogleLogging(argv[0]);
	
	// The variable to solve for with its initial value. It will be
	// mutated in place by the solver.
	// double x = xin;
	const double initial_x = x;
	
	// Build the problem.
	ceres::Problem problem;
	
	// Set up the only cost function (also known as residual). This uses
	// auto-differentiation to obtain the derivative (jacobian).
	CostFunction* cost_function =
		new AutoDiffCostFunction<CostFunctor, 1, 1>(new CostFunctor);
	problem.AddResidualBlock(cost_function, nullptr, &x);
	
	// Run the solver!
	Solver::Options options;
	options.minimizer_progress_to_stdout = true;
	Solver::Summary summary;
	Solve(options, &problem, &summary);
	
	Rcpp::Rcout << summary.BriefReport() << "\n";
	Rcpp::Rcout << "x : " << initial_x << " -> " << x << "\n";
	return 0;
}

```
**NOTE** - Compilation of this code will give a list of `Eigen` related warnings in `R`, so they can be neglected.

Finally, the `ceres_helloworld` function can be called in `R` using a numeric argument, e.g.,
```
ceres_helloworld(0.5)
```

which gives the following output in the `R` console:
```
> ceres_helloworld(0.5)
iter      cost      cost_change  |gradient|   |step|    tr_ratio  tr_radius  ls_iter  iter_time  total_time
   0  4.512500e+01    0.00e+00    9.50e+00   0.00e+00   0.00e+00  1.00e+04        0    1.72e-05    3.70e-05
   1  4.511598e-07    4.51e+01    9.50e-04   9.50e+00   1.00e+00  3.00e+04        1    1.50e-05    7.49e-05
   2  5.012552e-16    4.51e-07    3.17e-08   9.50e-04   1.00e+00  9.00e+04        1    5.96e-06    8.68e-05
Ceres Solver Report: Iterations: 3, Initial cost: 4.512500e+01, Final cost: 5.012552e-16, Termination: CONVERGENCE
x : 0.5 -> 10
[1] 0
```

which is same answer that we get using the `C++` library (see [here](http://ceres-solver.org/nnls_tutorial.html#hello-world)).
