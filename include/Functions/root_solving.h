#ifndef ROOT_SOLVING_H
#define ROOT_SOLVING_H

#include "../Classes/Custom_types.h"
#include "../Classes/Metaparameters.h"
#include <gsl/gsl_vector.h>
#include <gsl/gsl_math.h>

struct delta_solver;

/* finds a rough interval of Npoints around the zero of function f for the parameters s */
nvector find_rough_interval(gsl_function* f, unsigned int Npoints, unsigned int, fitmode);

nvector find_rough_interval_polynomial_fit(gsl_function* f, unsigned int Npoints, unsigned int);
nvector find_rough_interval_sigmoidal_fit(gsl_function* f, unsigned int Npoints, unsigned int);

/* will give a point where the function f is roughly 0 (can shift this with the shift parameter for f) */
double find_zero(gsl_function* f, unsigned int Npoints, unsigned int verbose);

double function_av_extinct_solver(double, void*);
double solve_for_delta_with_fit(const gsl_vector*, double&, double&, const Metaparameters&, delta_solver);

#endif
