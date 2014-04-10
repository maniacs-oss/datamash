/* compute - perform simple calculation on input data
   Copyright (C) 2014 Assaf Gordon.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Assaf Gordon */
#include <config.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "xalloc.h"
#include "size_max.h"

#include "utils.h"

/* Compare two flowting-point variables, while avoiding '==' .
see:
http://www.gnu.org/software/libc/manual/html_node/Comparison-Functions.html */
int
cmp_long_double (const void *p1, const void *p2)
{
  const long double *a = (const long double *)p1;
  const long double *b = (const long double *)p2;
  return ( *a > *b ) - (*a < *b);
}

long double
median_value ( const long double * const values, size_t n )
{
#if 0
  return percentile_value(values, n, 2.0/4.0);
#else
  /* Equivalent to the above, but slightly faster */
  return (n&0x01)
    ?values[n/2]
    :( (values[n/2-1] + values[n/2]) / 2.0 );
#endif
}

/* This implementation follows R's summary() and quantile(type=7) functions.
   See discussion here:
   http://tolstoy.newcastle.edu.au/R/e17/help/att-1067/Quartiles_in_R.pdf */
long double percentile_value ( const long double * const values,
                               const size_t n, const double percentile )
{
  const double h = ( (n-1) * percentile ) ;
  const size_t fh = floor(h);

  if (n==0 || percentile<0.0 || percentile>100.0)
    return 0;

  if (n==1)
    return values[0];

  return values[fh] + (h-fh) * ( values[fh+1] - values[fh] ) ;
}


/* Given a sorted array of doubles, return the MAD value
   (median absolute deviation), with scale constant 'scale' */
long double mad_value ( const long double * const values, size_t n, double scale )
{
  const long double median = median_value(values,n);
  long double *mads = xnmalloc(n,sizeof(long double));
  long double mad = 0 ;
  for (size_t i=0; i<n; ++i)
    mads[i] = fabsl(median - values[i]);
  qsortfl(mads,n);
  mad = median_value(mads,n);
  free(mads);
  return mad * scale;
}

long double
arithmetic_mean_value ( const long double * const values, const size_t n)
{
  long double sum=0;
  long double mean;
  for (size_t i = 0; i < n; i++)
    sum += values[i];
  mean = sum / n ;
  return mean;
}

long double
variance_value ( const long double * const values, size_t n, int df )
{
  long double sum=0;
  long double mean;
  long double variance;

  if ( df<0 || (size_t)df == n )
    return nanl("");

  mean = arithmetic_mean_value(values, n);

  sum = 0 ;
  for (size_t i = 0; i < n; i++)
    sum += (values[i] - mean) * (values[i] - mean);

  variance = sum / ( n - df );

  return variance;
}

long double
stdev_value ( const long double * const values, size_t n, int df )
{
  return sqrtl ( variance_value ( values, n, df ) );
}

/*
 Given an array of doubles, return the skewness
 'df' is degrees-of-freedom. Use DF_POPULATION or DF_SAMPLE (see above).
 */
long double skewness_value ( const long double * const values, size_t n, int df )
{
  long double moment2=0;
  long double moment3=0;
  long double mean;
  long double skewness;

  if (n<=1)
    return nanl("");

  mean = arithmetic_mean_value(values, n);

  for (size_t i = 0; i < n; i++)
    {
      const long double t = (values[i] - mean);
      moment2 += t*t;
      moment3 += t*t*t;
    }
  moment2 /= n;
  moment3 /= n;

  /* can't use 'powl(moment2,3.0/2.0)' - not all systems have powl */
  skewness = moment3 / sqrtl(moment2*moment2*moment2);
  if ( df == DF_SAMPLE )
    {
      if (n<=2)
        return nanl("");
      skewness = ( sqrtl(n*(n-1)) / (n-2) ) * skewness;
    }

  return skewness;
}

/* Standard error of skewness (SES), given the sample size 'n' */
long double SES_value ( size_t n )
{
  if (n<=2)
    return nanl("");
  return sqrtl( (long double)(6.0*n*(n-1)) /
                  ((long double)(n-2)*(n+1)*(n+3)) );
}

/* Skewness Test statistics Z = ( sample skewness / SES ) */
long double skewnessZ_value ( const long double * const values, size_t n)
{
  const long double skew = skewness_value (values,n,DF_SAMPLE);
  const long double SES = SES_value (n);
  if (isnan(skew) || isnan(SES) )
    return nanl("");
  return skew/SES;
}


/*
 Given an array of doubles, return the excess kurtosis
 'df' is degrees-of-freedom. Use DF_POPULATION or DF_SAMPLE (see above).
 */
long double excess_kurtosis_value ( const long double * const values, size_t n, int df )
{
  long double moment2=0;
  long double moment4=0;
  long double mean;
  long double excess_kurtosis;

  if (n<=1)
    return nanl("");

  mean = arithmetic_mean_value(values, n);

  for (size_t i = 0; i < n; i++)
    {
      const long double t = (values[i] - mean);
      moment2 += t*t;
      moment4 += t*t*t*t;
    }
  moment2 /= n;
  moment4 /= n;

  excess_kurtosis = moment4 / (moment2*moment2) - 3;

  if ( df == DF_SAMPLE )
    {
      if (n<=3)
        return nanl("");
      excess_kurtosis = ( ((long double)n-1) / (((long double)n-2)*((long double)n-3)) ) *
                        ( (n+1)*excess_kurtosis + 6 ) ;
    }

  return excess_kurtosis;
}

/* Standard error of kurtisos (SEK), given the sample size 'n' */
long double SEK_value ( size_t n )
{
  const long double ses = SES_value(n);

  if (n<=3)
    return nanl("");

   return 2 * ses * sqrtl( (long double)(n*n-1) /
                            ((long double)((n-3)*(n+5)))  );
}

/* Kurtosis Test statistics Z = ( sample kurtosis / SEK ) */
long double kurtosisZ_value ( const long double * const values, size_t n)
{
  const long double kurt = excess_kurtosis_value (values,n,DF_SAMPLE);
  const long double SEK = SEK_value (n);
  if (isnan(kurt) || isnan(SEK) )
    return nanl("");
  return kurt/SEK;
}

/*
 Chi-Squared - Cumulative distribution function,
 for the special case of DF=2.
 Equivalent to the R function 'pchisq(x,df=2)'.
*/
long double pchisq_df2(long double x)
{
  return 1.0 - expl(-x/2);
}

/*
 Given an array of doubles, return the p-Value
 Of the Jarque-Bera Test for normality
   http://en.wikipedia.org/wiki/Jarque%E2%80%93Bera_test
 Equivalent to R's "jarque.test()" function in the "moments" library.
 */
long double jarque_bera_pvalue ( const long double * const values, size_t n )
{
  const long double k = excess_kurtosis_value(values,n,DF_POPULATION);
  const long double s = skewness_value(values,n,DF_POPULATION);
  const long double jb = (long double)(n*(s*s + k*k/4))/6.0 ;
  const long double pval = 1.0 - pchisq_df2(jb);
  if (n<=1 || isnan(k) || isnan(s))
    return nanl("");
  return pval;
}

/*
 D'Agostino-Perason omnibus test for normality.
 returns the p-value,
 where the null-hypothesis is normal distribution.
*/
long double dagostino_pearson_omnibus_pvalue( const long double * const values, size_t n)
{
  const long double z_skew = skewnessZ_value (values, n);
  const long double z_kurt = kurtosisZ_value (values, n);
  const long double DP = z_skew*z_skew + z_kurt*z_kurt;
  const long double pval = 1.0 - pchisq_df2( DP );

  if (isnan(z_skew) || isnan(z_kurt))
    return nanl("");
  return pval;
}


long double
mode_value ( const long double * const values, size_t n, enum MODETYPE type)
{
  /* not ideal implementation but simple enough */
  /* Assumes 'values' are already sorted, find the longest sequence */
  long double last_value = values[0];
  size_t seq_size=1;
  size_t best_seq_size= (type==MODE)?1:SIZE_MAX;
  size_t best_value = values[0];

  for (size_t i=1; i<n; i++)
    {
      bool eq = (cmp_long_double(&values[i],&last_value)==0);

      if (eq)
        seq_size++;

      if ( ((type==MODE) && (seq_size > best_seq_size))
           ||
           ((type==ANTIMODE) && (seq_size < best_seq_size)))
        {
          best_seq_size = seq_size;
          best_value = last_value;
        }

      if (!eq)
          seq_size = 1;

      last_value = values[i];
    }
  return best_value;
}

int
cmpstringp(const void *p1, const void *p2)
{
  /* The actual arguments to this function are "pointers to
   * pointers to char", but strcmp(3) arguments are "pointers
   * to char", hence the following cast plus dereference */
  return strcmp(* (char * const *) p1, * (char * const *) p2);
}

int
cmpstringp_nocase(const void *p1, const void *p2)
{
  /* The actual arguments to this function are "pointers to
   * pointers to char", but strcmp(3) arguments are "pointers
   * to char", hence the following cast plus dereference */
  return strcasecmp(* (char * const *) p1, * (char * const *) p2);
}

/* Sorts (in-place) an array of long-doubles */
void qsortfl(long double *values, size_t n)
{
  qsort (values, n, sizeof (long double), cmp_long_double);
}
