#ifndef __COSEC_MATH_H__
#define __COSEC_MATH_H__

#define INFINITY (float)
#define HUGE_VAL  (double)1e308

double floor(double x);
double ceil(double x);
double fabs(double x);

double frexp(double x, int *exp);
double modf(double x, double *ptr);

double fmod(double x, double y);
double pow(double x, double y);

double exp(double x);
double ldexp(double x, int exp);

double log(double x);
double log10(double x);

double sqrt(double x);

double atan(double x);
double asin(double x);
double acos(double x);

double tan(double x);
double sin(double x);
double cos(double x);

double atan2(double y, double x);

double tanh(double x);
double sinh(double x);
double cosh(double x);

#endif // __COSEC_MATH_H__
