/*
 * math.h -- matematiken som kärnan och WAV-läsningen behöver i
 * webbläsaren, till boken ABC80 inifrån
 *
 * Bara det som används: exp och log (för filtren och powf i ljud.c)
 * och absolutbeloppet. Funktionerna finns i ../libc.c.
 */

#ifndef MATH_H
#define MATH_H

double exp(double x);
double log(double x);
double pow(double x, double y);
float  powf(float x, float y);
double fabs(double x);

#endif
