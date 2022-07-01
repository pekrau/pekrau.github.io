/* crng.c

  crng: Random-number generators as Python extension types coded in C.

  See the file doc.html for documentation.

  Copyright (C) 2000-2002 Per J. Kraulis

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program (see file gpl.txt); if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
  MA  02111-1307  USA


  The MT19937 code has been copyrighted by M. Matsumoto & T. Nishimura,
  (see the copyright notice in the comment header for that section),
  and is governed by the GNU Lesser General Public License (file lgpl.txt,
  previously called 'GNU Library General Public License').

  to do:
  - more non-uniform deviates: spherical, Cauchy, hypergeometric,... ?
*/

#include "Python.h"

/*  #include <assert.h> */
#include <math.h>
#include <stdlib.h>

/*  #define DEBUG 1 */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_E
#define M_E 2.7182818284590452354
#endif

#define INV_SQRT_2_PI 0.39894228040143270 /* 1/sqrt(2.0 * M_PI) */

#define DEFAULT_SEED 314159265

static char _version[] = "1.2";
static char _RCS[] = "$Id: crng.c,v 2.11 2002/10/23 11:12:05 per Exp $";
static char _copyright[] = "Copyright (C) 2000-2002 Per J. Kraulis";

static char _doc[] =
"Random-number generators implemented as C extension types.\n\
\n\
The generators are uniform in the open interval (0.0,1.0).\n\
Instance objects are callable; calls are equivalent to the 'next' method.\n\
\n\
    Random-number generator types\n\
ParkMiller    -- Park-Miller 'minimal standard'\n\
WichmannHill  -- Wichmann-Hill portable RNG (algorithm AS 183)\n\
LEcuyer       -- Pierre L'Ecuyer's MLCG of 1988\n\
Ranlux        -- Martin Luescher's RANLUX\n\
Taus88        -- three-component combined Tausworthe RNG by L'Ecuyer\n\
MRG32k3a      -- combined multiple recursive 32-bit RNG by L'Ecuyer\n\
MT19937       -- Mersenne Twister by Matsumoto and Nishimura\n\
\n\
    Continuous deviate types\n\
UniformDeviate      -- Uniform deviate in the open interval (a,b)\n\
ExponentialDeviate  -- Exponential deviate with given mean\n\
NormalDeviate       -- Normal (Gaussian) deviate with given mean and stdev\n\
GammaDeviate        -- Gamma deviate with a given order and scale\n\
BetaDeviate         -- Beta deviate for the parameters a and b\n\
\n\
    Integer-valued deviate types\n\
PoissonDeviate      -- Poisson deviate with a given mean\n\
BinomialDeviate     -- Binomial deviate for a given probability and number\n\
GeometricDeviate    -- Geometric deviate for a given probability\n\
BernoulliDeviate    -- Bernoulli deviate for a given probability\n\
\n\
    Utility functions for sampling and shuffling\n\
choose   -- return a new sequence of objects chosen without replacement\n\
sample   -- return a new sequence of objects sampled with replacement\n\
shuffle  -- return a new sequence with objects shuffled\n\
stir     -- shuffle a list of objects in-place\n\
pick     -- pick one object from the sequence";

static char _next_doc[] =
  "next(n=1)    -- return the next n value(s), as a float or tuple of floats";
static char _compute_doc[] =
  "compute(n=1) -- compute the next n value(s); None returned";
static char _density_doc[] =
  "density(x)   -- density of deviate at point x (float)";

static char _crng_next_value[] = "_crng_next_value";
static char _crng_basic_next_value[] = "_crng_basic_next_value";

static char _seed_int_error[] = "seed must be integer";
static char _seed_pos_error[] = "seed must be positive";

static char _next_error[] = "argument to method 'next' must be positive";
static char _compute_error[] = "argument to method 'compute' must be positive";

static char _state_error[] =
  "'state' is not a tuple, or contains invalid data";

static char _attr_error[] =
  "cannot delete or create attributes in this object";

static char _crng_basic_error[] =
  "'rng' must be a basic RNG from module crng";
static char _a_float_error[] = "'a' must be float";
static char _b_float_error[] = "'b' must be float";
static char _a_pos_error[] = "'a' must be positive";
static char _b_pos_error[] = "'b' must be positive";
static char _ab_lt_error[] = "'a' and 'b' must satisfy a<b";
static char _mean_float_error[] = "'mean' must be float";
static char _mean_pos_error[] = "'mean' must be positive";
static char _mean_nonneg_error[] = "'mean' must be non-negative";
static char _direct_int_error[] = "'direct' must be integer";
static char _p_float_error[] = "'p' must be float";
static char _p_01_error[] = "'p' must be in interval [0,1]";
static char _p_open_01_error[] = "'p' must be in interval (0,1]";
static char _n_int_error[] = "'n' must be integer";
static char _n_pos_error[] = "'n' must be positive";
static char _stdev_float_error[] = "'stdev' must be float";
static char _stdev_pos_error[] = "'stdev' must be positive";
static char _order_float_error[] = "'order' must be float";
static char _order_pos_error[] = "'order' must be positive";
static char _scale_float_error[] = "'scale' must be float";
static char _scale_pos_error[] = "'scale' must be positive";


/*------------------------------------------------------------*/
static void			/* generic instance destruction */
generic_dealloc (PyObject *self)
{
#ifdef DEBUG
  fprintf(stderr, "*** generic_dealloc %p\n", (void *) self);
#endif
  PyMem_Free(self);
}


/*------------------------------------------------------------*/
static PyObject *		/* next for double next_value */
next_double (PyObject *self, PyObject *args,
	     double (*next_value) (PyObject *self))
{
  long n = 1;

  if (! PyArg_ParseTuple(args, "|l", &n)) return NULL;

  if (n == 1) {
    return PyFloat_FromDouble(next_value (self));

  } else if (n > 1) {
    register long i;
    PyObject *tuple, *value;

    tuple = PyTuple_New(n);
    if (!tuple) return NULL;

    for (i = 0; i < n; i++) {
      value = PyFloat_FromDouble(next_value(self));
      if (!value) return NULL;	/* XXX potential memory leak! */
      PyTuple_SET_ITEM(tuple, i, value);
    }

    return tuple;

  } else {
    PyErr_SetString(PyExc_ValueError, _next_error);
    return NULL;
  }
}


/*------------------------------------------------------------*/
static PyObject *		/* next for long next_value */
next_long (PyObject *self, PyObject *args,
	   long (*next_value) (PyObject *self))
{
  long n = 1;

  if (! PyArg_ParseTuple(args, "|l", &n)) return NULL;

  if (n == 1) {
    return PyInt_FromLong(next_value(self));

  } else if (n > 1) {
    register long i;
    PyObject *tuple, *value;

    tuple = PyTuple_New(n);
    if (!tuple) return NULL;

    for (i = 0; i < n; i++) {
      value = PyInt_FromLong(next_value(self));
      if (!value) return NULL;	/* XXX potential memory leak! */
      PyTuple_SET_ITEM(tuple, i, value);
    }

    return tuple;

  } else {
    PyErr_SetString(PyExc_ValueError, _next_error);
    return NULL;
  }
}


/*------------------------------------------------------------*/
static PyObject *		/* compute for double next_value */
compute_double (PyObject *self, PyObject *args,
		double (*next_value) (PyObject *self))
{
  long n = 1;

  if (! PyArg_ParseTuple(args, "|l", &n)) return NULL;

  if (n >= 1) {
    for (; n; n--) next_value(self);
    Py_INCREF(Py_None);
    return Py_None;

  } else {
    PyErr_SetString(PyExc_ValueError, _compute_error);
    return NULL;
  }
}


/*------------------------------------------------------------*/
static PyObject *		/* compute for long next_value */
compute_long (PyObject *self, PyObject *args,
	      long (*next_value) (PyObject *self))
{
  long n = 1;

  if (! PyArg_ParseTuple(args, "|l", &n)) return NULL;

  if (n >= 1) {
    for (; n; n--) next_value(self);
    Py_INCREF(Py_None);
    return Py_None;

  } else {
    PyErr_SetString(PyExc_ValueError, _compute_error);
    return NULL;
  }
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.density()" */
uniform_01_density (PyObject *self, PyObject *args)
{
  double x;

  if (! PyArg_ParseTuple(args, "d", &x)) return NULL;

  if ((x <= 0.0) || (x >= 1.0)) {
    return PyFloat_FromDouble(0.0);
  } else {
    return PyFloat_FromDouble(1.0);
  }
}


/*------------------------------------------------------------*/
static double			/* ln(Gamma(x)) */
log_gamma (double x)
{
  register int j;
  register double y, t, s;
  static double c[6] = {76.18009172947146, -86.50532032941677,
			24.01409824083091, -1.231739572450155,
			0.1208650973866179e-2, -0.5395239384953e-5};

  y = x - 1.0;
  t = y + 5.5;
  t -= (y + 0.5) * log(t);
  s = 1.000000000190015;
  for (j=0; j<6; j++) s += c[j] / ++y;
  return (-t + log(2.5066282746310005 * s));
}


/*****************************************************************************
 * ParkMiller
 *
 * Park and Miller 'minimal standard' prime modulus multiplicative
 * congruential algorithm.
 *
 * This code is a translation of the Pascal code (Integer Version 2)
 * in the reference.
 *
 * Reference:
 * Stephen K. Park & Keith W. Miller, "Random Number Generators: Good ones
 * are hard to find", Comm. A.C.M., 1988, vol 31, pp 1192-1201.
 *****************************************************************************/

#define PARK_MILLER_A 16807L
#define PARK_MILLER_M 2147483647L /* 2^31-1 */
#define PARK_MILLER_Q 127773L	/* M / A */
#define PARK_MILLER_R 2836L	/* M % A */
#define PARK_MILLER_NORM 4.6566128730773926e-10 /* 1/double(M+1) */

typedef struct {
  PyObject_HEAD
  long seed;
} ParkMiller_object;

staticforward PyTypeObject ParkMiller_type;

static char ParkMiller_doc[] =
"Park-Miller's 'minimal standard', period: 2.1e9\n\
S.K. Park & K.W. Miller, Comm. A.C.M. (1988) 31, 1192-1201.\n\
\n\
    Object creation\n\
ParkMiller(seed=314159265)\n\
  seed >0  -- initial seed\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
seed  -- current seed (read-only)";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
ParkMiller_next_value (ParkMiller_object *self)
{
  register long lo, hi, test;

  register long seed = self->seed; /* optimization; localize value */

  hi = seed / PARK_MILLER_Q;
  lo = seed % PARK_MILLER_Q;
  test = PARK_MILLER_A * lo - PARK_MILLER_R * hi;
  seed = (test > 0) ? test : test + PARK_MILLER_M;

  self->seed = seed;		/* copy back the localized value */

  return (PARK_MILLER_NORM * (double) seed);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
ParkMiller_next (ParkMiller_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) ParkMiller_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
ParkMiller_compute (ParkMiller_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) ParkMiller_next_value);
}


/*------------------------------------------------------------*/
static struct PyMethodDef ParkMiller_methods[] = { /* order optimized */
  {"next", (PyCFunction) ParkMiller_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) ParkMiller_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) uniform_01_density, METH_VARARGS, _density_doc},
  {NULL, NULL}
};


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
ParkMiller_getattr (ParkMiller_object *self, char *name)
{
  if (strcmp(name, "seed") == 0) {
    return PyInt_FromLong(self->seed); 
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[s]", "seed");
  } else if ((strcmp(name, _crng_basic_next_value) == 0) ||
	     (strcmp(name, _crng_next_value) == 0)) {
    return PyCObject_FromVoidPtr((void *) ParkMiller_next_value, NULL);
  }
  return Py_FindMethod(ParkMiller_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
ParkMiller_repr (ParkMiller_object *self)
{
   char str[64];
   sprintf (str, "crng.ParkMiller(seed=%li)", self->seed);
   return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
ParkMiller_str (ParkMiller_object *self)
{
   char str[64];
   sprintf (str, "<crng.ParkMiller object at %p>", self);
   return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = ParkMiller()" */
ParkMiller_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"seed", NULL};
  ParkMiller_object *new;

  new = PyObject_NEW(ParkMiller_object, &ParkMiller_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new ParkMiller %p\n", (void *) new);
#endif

  new->seed = DEFAULT_SEED;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|l", argnames,
				    &(new->seed)))
    goto error;

  if (new->seed <= 0) {
    PyErr_SetString(PyExc_ValueError, _seed_pos_error);
    goto error;
  }

  return (PyObject *) new;

error:
  PyMem_Free(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject ParkMiller_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "ParkMiller",			/* tp_name */
  sizeof(ParkMiller_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  generic_dealloc,   /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		   /* tp_print    "print x"     */
  (getattrfunc) ParkMiller_getattr,/* tp_getattr  "x.attr"      */
  (setattrfunc) 0,		   /* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		   /* tp_compare  "x > y"       */
  (reprfunc)    ParkMiller_repr,   /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  ParkMiller_next, /* tp_call    "x()"     */
  (reprfunc)     ParkMiller_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  ParkMiller_doc		/* tp_doc */
};


/*****************************************************************************
 * WichmannHill
 *
 * The Wichmann-Hill portable RNG algorithm.
 *
 * References:
 * B.A. Wichmann & I.D. Hill, "Algorithm AS 183: An efficient and portable
 *   pseudo-random number generator", Applied Statistics, 1982, vol 31,
 *   pp 188-190.
 * B.A. Wichmann & I.D. Hill, "Correction to Algorithm AS 183",
 *   Applied Statistics, 1984, vol 33, p 123.
 * A.I. McLeod, "A remark on Algorithm AS 183", Applied Statistics,
 *   1985, vol 34, pp 198-200.
 *****************************************************************************/

typedef struct {
  PyObject_HEAD
  long seed1, seed2, seed3;
} WichmannHill_object;

staticforward PyTypeObject WichmannHill_type;

static char WichmannHill_doc[] =
"Wichmann-Hill portable RNG (algorithm AS 183), period: 6.95e12\n\
B.A. Wichmann & I.D. Hill, Applied Stat. (1982) 31, 188-190.\n\
\n\
    Object creation\n\
WichmannHill(seed1=314, seed2=159, seed3=365)\n\
  0< seed1 <=30268  -- initial first seed\n\
  0< seed2 <=30306  -- initial second seed\n\
  0< seed3 <=30322  -- initial third seed\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
seed1  -- current first seed (read-only)\n\
seed2  -- current second seed (read-only)\n\
seed3  -- current third seed (read-only)";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
WichmannHill_next_value (WichmannHill_object *self)
{
  self->seed1 = (self->seed1 * 171L) % 30269L;
  self->seed2 = (self->seed2 * 172L) % 30307L;
  self->seed3 = (self->seed3 * 170L) % 30323L;
  return fmod(((double) self->seed1 / 30269.0 +
	       (double) self->seed2 / 30307.0 +
	       (double) self->seed3 / 30323.0), 1.0);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
WichmannHill_next (WichmannHill_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) WichmannHill_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
WichmannHill_compute (WichmannHill_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) WichmannHill_next_value);
}


/*------------------------------------------------------------*/
static struct PyMethodDef WichmannHill_methods[] = { /* order optimized */
  {"next", (PyCFunction) WichmannHill_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) WichmannHill_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) uniform_01_density, METH_VARARGS, _density_doc},
  {NULL, NULL}
};


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
WichmannHill_getattr (WichmannHill_object *self, char *name)
{
  if (strcmp(name, "seed1") == 0) {
    return PyInt_FromLong(self->seed1); 
  } else if (strcmp(name, "seed2") == 0) {
    return PyInt_FromLong(self->seed2); 
  } else if (strcmp(name, "seed3") == 0) {
    return PyInt_FromLong(self->seed3); 
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[sss]", "seed1", "seed2", "seed3");
  } else if ((strcmp(name, _crng_basic_next_value) == 0) ||
	     (strcmp(name, _crng_next_value) == 0)) {
    return PyCObject_FromVoidPtr((void *) WichmannHill_next_value, NULL);
  }
  return Py_FindMethod(WichmannHill_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
WichmannHill_repr (WichmannHill_object *self)
{
  char str[128];
  sprintf(str, "crng.WichmannHill(seed1=%li, seed2=%li, seed3=%li)",
	  self->seed1, self->seed2, self->seed3);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
WichmannHill_str (WichmannHill_object *self)
{
   char str[64];
   sprintf (str, "<crng.WichmannHill object at %p>", self);
   return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = WichmannHill()" */
WichmannHill_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"seed1", "seed2", "seed3", NULL};
  WichmannHill_object *new;

  new = PyObject_NEW(WichmannHill_object, &WichmannHill_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new WichmannHill %p\n", (void *) new);
#endif

  new->seed1 = 314;
  new->seed2 = 159;
  new->seed3 = 365;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|lll", argnames,
				    &(new->seed1), &(new->seed2),
				    &(new->seed3)))
    goto error;

  if ((new->seed1 <= 0) || (new->seed1 > 30268)) {
    PyErr_SetString(PyExc_ValueError,
		    "seed1 value out of range: 0<seed<=30268");
    goto error;
  } else if ((new->seed2 <= 0) || (new->seed2 > 30306)) {
    PyErr_SetString(PyExc_ValueError,
		    "seed2 value out of range: 0<seed<=30306");
    goto error;
  } else if ((new->seed3 <= 0) || (new->seed3 > 30322)) {
    PyErr_SetString(PyExc_ValueError,
		    "seed3 value out of range: 0<seed<=30322");
  }

  return (PyObject *) new;

error:
  PyMem_Free(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject WichmannHill_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "WichmannHill",		/* tp_name */
  sizeof(WichmannHill_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  generic_dealloc, /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) WichmannHill_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) 0,		/* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    WichmannHill_repr, /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  WichmannHill_next, /*tp_call    "x()"     */
  (reprfunc)     WichmannHill_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  WichmannHill_doc		/* tp_doc */
};


/*****************************************************************************
 * LEcuyer
 *
 * Pierre L'Ecuyer's algorithm of 1988; a combined multiplicative linear
 * congruential generator.
 *
 * This code is a translation of the Pascal code (Figure 3) in
 * the reference.
 *
 * Reference:
 * Pierre L'Ecuyer, "Efficient and portable combined random number
 * generators.", Communications of the A.C.M., 1988, vol 31, pp 742-749 + 774.
 *****************************************************************************/

#define LECUYER_A1 53668L
#define LECUYER_B1 40014L
#define LECUYER_C1 12211L
#define LECUYER_M1 2147483563L /* 2^31-1 */
#define LECUYER_A2 52774L
#define LECUYER_B2 40692L
#define LECUYER_C2 3791L
#define LECUYER_M2 2147483399L
#define LECUYER_NORM 4.6566130573917691e-10 /* 1/LECUYER_M1 */

typedef struct {
  PyObject_HEAD
  long seed1, seed2;
} LEcuyer_object;

staticforward PyTypeObject LEcuyer_type;

static char LEcuyer_doc[] =
"Pierre L'Ecuyer's MLCG of 1988, period: 2.3e18\n\
P. L'Ecuyer, Comm. A.C.M. (1988) 31, 742-749.\n\
\n\
    Object creation\n\
LEcuyer(seed1=314159265, seed2=314159263)\n\
  seed1 >0  -- initial first seed\n\
  seed2 >0  -- initial second seed\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
seed1  -- current first seed\n\
seed2  -- current second seed";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
LEcuyer_next_value (LEcuyer_object *self)
{
  register long k, z;

  register long seed1 = self->seed1; /* optimization; localize values */
  register long seed2 = self->seed2;

  k = seed1 / LECUYER_A1;
  seed1 = LECUYER_B1 * (seed1 - k * LECUYER_A1) - k * LECUYER_C1;
  if (seed1 < 0) seed1 += LECUYER_M1;

  k = seed2 / LECUYER_A2;
  seed2 = LECUYER_B2 * (seed2 - k * LECUYER_A2) - k * LECUYER_C2;
  if (seed2 < 0) seed2 += LECUYER_M2;

  z = seed1 - seed2;
  if (z < 1L) z += LECUYER_M1 - 1;

  self->seed1 = seed1;		/* copy back the localized values */
  self->seed2 = seed2;

  return (LECUYER_NORM * (double) z);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
LEcuyer_next (LEcuyer_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) LEcuyer_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
LEcuyer_compute (LEcuyer_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) LEcuyer_next_value);
}


/*------------------------------------------------------------*/
static struct PyMethodDef LEcuyer_methods[] = { /* order optimized */
  {"next", (PyCFunction) LEcuyer_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) LEcuyer_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) uniform_01_density, METH_VARARGS, _density_doc},
  {NULL, NULL}
};


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
LEcuyer_getattr (LEcuyer_object *self, char *name)
{
  if (strcmp(name, "seed1") == 0) {
    return PyInt_FromLong(self->seed1); 
  } else if (strcmp(name, "seed2") == 0) {
    return PyInt_FromLong(self->seed2); 
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[ss]", "seed1", "seed2");
  } else if ((strcmp(name, _crng_basic_next_value) == 0) ||
	     (strcmp(name, _crng_next_value) == 0)) {
    return PyCObject_FromVoidPtr((void *) LEcuyer_next_value, NULL);
  }
  return Py_FindMethod(LEcuyer_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
LEcuyer_repr (LEcuyer_object *self)
{
  char str[128];
  sprintf(str, "crng.LEcuyer(seed1=%li, seed2=%li)", self->seed1, self->seed2);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
LEcuyer_str (LEcuyer_object *self)
{
   char str[64];
   sprintf (str, "<crng.LEcuyer object at %p>", self);
   return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = LEcuyer()" */
LEcuyer_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"seed1", "seed2", NULL};
  LEcuyer_object *new;

  new = PyObject_NEW(LEcuyer_object, &LEcuyer_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new LEcuyer %p\n", (void *) new);
#endif

  new->seed1 = DEFAULT_SEED;
  new->seed2 = DEFAULT_SEED - 2;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|ll", argnames,
				    &(new->seed1), &(new->seed2)))
    goto error;

  if ((new->seed1 <= 0) || (new->seed2 <= 0)) {
    PyErr_SetString(PyExc_ValueError, _seed_pos_error);
    goto error;
  }

  if (new->seed1 > LECUYER_M1 - 1) new->seed1 = new->seed1 % (LECUYER_M1 - 1);
  if (new->seed2 > LECUYER_M2 - 1) new->seed2 = new->seed2 % (LECUYER_M2 - 1);

  return (PyObject *) new;

error:
  PyMem_Free(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject LEcuyer_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "LEcuyer",			/* tp_name */
  sizeof(LEcuyer_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  generic_dealloc,/* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) LEcuyer_getattr,/* tp_getattr  "x.attr"      */
  (setattrfunc) 0,		/* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    LEcuyer_repr,	/* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  LEcuyer_next,	/* tp_call    "x()"     */
  (reprfunc)     LEcuyer_str,	/* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  LEcuyer_doc			/* tp_doc */
};


/*****************************************************************************
 * Ranlux
 *
 * Algorithm by Martin Luescher, based on RCARRY by Marsaglia and Zaman.
 *
 * No copyright was claimed in the original message containing this code.
 *
 * References:
 * Martin Luescher, "A portable high-quality random number generator for
 *   lattice field theory simulations", Computer Physics Communications, 1994,
 *   vol 79, pp 100-110.
 * F. James, "RANLUX: A Fortran implementation of the high-quality
 *   pseudorandom number generator of Luescher", Computer Physics
 *   Communications, 1994, vol 79, pp 111-114.
 * F. James, Erratum, Computer Physics Communications, 1996, vol 97, p 357.
 * Fred James, Fortran implementation: RANLUX, as reported by Byron Bodo
 *   in a Usenet News message, March 1995.
 *
 * Luxury levels:
 *  0: Equivalent to original RCARRY by Marsaglia and Zaman. 
 *     Has a very long period, but fails many tests.
 *  1: Considerable improvement in quality over level 0. 
 *     Passes the gap test, but still fails the spectral test.
 *  2: Passes all known tests, but theoretically still defective.
 *  3: Any theoretically possible correlations have a very small 
 *     chance of being observed.
 *  4: Highest possible luxury, all 24 bits chaotic.
 *****************************************************************************/

#define RANLUX_TWOM24 (1.0/16777216.0)
#define RANLUX_TWOM12 (1.0/4096.0)

static int nskip_lookup[] = {0, 24, 73, 199, 365};

typedef struct {
  PyObject_HEAD
  int    luxury;
  int    i24, j24, in24;
  float  carry;
  float  seeds[25];		/* seeds[0] is not used. */
} Ranlux_object;

staticforward PyTypeObject Ranlux_type;

static char Ranlux_doc[] =
"Martin Luescher's RANLUX, period: ~5.2e171\n\
M. Luescher, Comp. Phys. Comm. (1994) 79, 100-110.\n\
\n\
    Object creation\n\
Ranlux(luxury=3, seed=314159265, state=None)\n\
  0<= luxury <=4  -- quality level\n\
  seed >0         -- initial seed\n\
  state           -- set the initial state from a state tuple\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
luxury  -- quality level (read-only)\n\
state   -- the state tuple reflecting the current state (read-only)";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
Ranlux_next_value (Ranlux_object *self)
{
  register double value;
  register float uni;

  register int i24 = self->i24;	/* optimization: localize values */
  register int j24 = self->j24;
  register float *seeds = self->seeds;

  uni = seeds[j24] - seeds[i24] - self->carry;
  if (uni < 0.0F) {
    uni++;
    self->carry = RANLUX_TWOM24;
  } else {
    self->carry = 0.0F;
  }

  seeds[i24] = uni;
  if (--i24 == 0) i24 = 24;	/* instead of 'next' array in Fortran impl */
  if (--j24 == 0) j24 = 24;

  if (uni < RANLUX_TWOM12) {	/* small numbers (with less than 12 */
				/* "significant" bits) are "padded" */
    value = (double) (uni + RANLUX_TWOM24 * seeds[j24]);
    if (value == 0.0) value = RANLUX_TWOM24 * RANLUX_TWOM24;
  } else {
    value = (double) uni;
  }

  if (++self->in24 == 24) {	/* luxury action, as proposed by Luescher */

    register int isk;
    register int nskip = nskip_lookup[self->luxury];
    register float carry = self->carry; /* optimization: localize value */

    self->in24 = 0;

    for (isk = 1; isk <= nskip; isk++) {

      uni = seeds[j24] - seeds[i24] - carry;
      if (uni < 0.0F) {
	uni++;
	carry = RANLUX_TWOM24;
      } else {
	carry = 0.0F;
      }

      seeds[i24] = uni;
      if (--i24 == 0) i24 = 24;
      if (--j24 == 0) j24 = 24;
    }

    self->carry = carry;	/* copy back the localized value */
  }

  self->i24 = i24;		/* copy back the localized values */
  self->j24 = j24;

  return value;
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
Ranlux_next (Ranlux_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) Ranlux_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
Ranlux_compute (Ranlux_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) Ranlux_next_value);
}


/*------------------------------------------------------------*/
static struct PyMethodDef Ranlux_methods[] = { /* order optimized */
  {"next", (PyCFunction) Ranlux_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) Ranlux_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) uniform_01_density, METH_VARARGS, _density_doc},
  {NULL, NULL}
};


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
Ranlux_getattr (Ranlux_object *self, char *name)
{
  if (strcmp(name, "luxury") == 0) {
    return PyInt_FromLong((long) self->luxury);

  } else if (strcmp(name, "state") == 0) {
    int i;
    PyObject *item, *tuple;

    tuple = PyTuple_New(29);
    if (!tuple) return NULL;
    item = PyInt_FromLong((long) self->luxury);
    if (!item) goto error;
    PyTuple_SET_ITEM(tuple, 0, item);
    item = PyInt_FromLong((long) self->i24);
    if (!item) goto error;
    PyTuple_SET_ITEM(tuple, 1, item);
    item = PyInt_FromLong((long) self->j24);
    if (!item) goto error;
    PyTuple_SET_ITEM(tuple, 2, item);
    item = PyInt_FromLong((long) self->in24);
    if (!item) goto error;
    PyTuple_SET_ITEM(tuple, 3, item);
    item = PyFloat_FromDouble((double) self->carry);
    if (!item) goto error;
    PyTuple_SET_ITEM(tuple, 4, item);

    for (i = 1; i < 25; i++) {	/* seeds[0] is not used */
      item = PyFloat_FromDouble((double) self->seeds[i]);
      if (!item) goto error;
      PyTuple_SET_ITEM(tuple, i + 4, item);
    }

    return tuple;

  error:
    Py_DECREF(tuple);
    return NULL;

  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[ss]", "luxury", "state");
  } else if ((strcmp(name, _crng_basic_next_value) == 0) ||
	     (strcmp(name, _crng_next_value) == 0)) {
    return PyCObject_FromVoidPtr((void *) Ranlux_next_value, NULL);
  }
  return Py_FindMethod(Ranlux_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
Ranlux_repr (Ranlux_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.Ranlux(state=");
  if (!str) return NULL;
  item = Ranlux_getattr(self, "state");
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
Ranlux_str (Ranlux_object *self)
{
   char s[64];
   sprintf (s, "<crng.Ranlux object at %p>", self);
   return PyString_FromString(s);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = Ranlux()" */
Ranlux_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"luxury", "seed", "state", NULL};
  Ranlux_object *new;
  long iseeds[25];		/* iseeds[0] is not used */
  long i, k, seed = DEFAULT_SEED;
  PyObject *state = NULL;

  new = PyObject_NEW(Ranlux_object, &Ranlux_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new Ranlux %p\n", (void *) new);
#endif

  new->luxury = 3;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|ilO!", argnames,
				    &(new->luxury), &seed,
				    &PyTuple_Type, &state))
    goto error;

  if (state) {			/* state tuple given; ignore luxury and seed */
    PyObject *item;

    if (PyTuple_Size(state) != 29) goto state_error;
    item = PyTuple_GET_ITEM(state, 0);
    if (! PyInt_Check(item)) goto state_error;
    new->luxury = (int) PyInt_AS_LONG(item);
    if ((new->luxury < 0) || (new->luxury > 4)) goto state_error;
    item = PyTuple_GET_ITEM(state, 1);
    if (! PyInt_Check(item)) goto state_error;
    new->i24 = (int) PyInt_AS_LONG(item);
    if ((new->i24 <= 0) || (new->i24 > 24)) goto state_error;
    item = PyTuple_GET_ITEM(state, 2);
    if (! PyInt_Check(item)) goto state_error;
    new->j24 = (int) PyInt_AS_LONG(item);
    if ((new->j24 <= 0) || (new->j24 > 24)) goto state_error;
    item = PyTuple_GET_ITEM(state, 3);
    if (! PyInt_Check(item)) goto state_error;
    new->in24 = (int) PyInt_AS_LONG(item);
    if ((new->in24 < 0) || (new->in24 >= 24)) goto state_error;
    item = PyTuple_GET_ITEM(state, 4);
    if (! PyFloat_Check(item)) goto state_error;
    new->carry = (float) PyFloat_AS_DOUBLE(item);

    for (i = 1; i < 25; i++) {
      item = PyTuple_GET_ITEM(state, i + 4);
      if (! PyFloat_Check(item)) goto state_error;
      new->seeds[i] = (float) PyFloat_AS_DOUBLE(item);
    }

  } else {			/* compute initial state from seed */
    if ((new->luxury < 0) || (new->luxury > 4)) {
      PyErr_SetString(PyExc_ValueError, "luxury out of range: 0<=luxury<=4");
      goto error;
    }

    if (seed <= 0) {
      PyErr_SetString(PyExc_ValueError, _seed_pos_error);
      goto error;
    }

    new->in24 = 0;
    for (i = 1; i <= 24; i++) {
      k = seed / 53668;
      seed = 40014 * (seed - k * 53668) - k * 12211;
      if (seed < 0) seed += 2147483563;
      iseeds[i] = seed % 16777216;
    }

    for (i = 1; i <= 24; i++) new->seeds[i] = iseeds[i] * RANLUX_TWOM24;

    new->i24 = 24;
    new->j24 = 10;
    new->carry = 0.0F;
    if (new->seeds[24] == 0.0) new->carry = RANLUX_TWOM24;
  }

  return (PyObject *) new;

state_error:
  PyErr_SetString(PyExc_ValueError, _state_error);
error:
  PyMem_Free(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject Ranlux_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "Ranlux",			/* tp_name */
  sizeof(Ranlux_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  generic_dealloc,/* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) Ranlux_getattr,	/* tp_getattr  "x.attr"      */
  (setattrfunc) 0,		/* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    Ranlux_repr,	/* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  Ranlux_next,	/* tp_call    "x()"     */
  (reprfunc)     Ranlux_str,	/* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  Ranlux_doc			/* tp_doc */
};


/*****************************************************************************
 * Taus88
 *
 * Three-component combined Tausworthe generator by Pierre L'Ecuyer.
 *
 * No copyright for this code was claimed in the paper from which it
 * was extracted.
 *
 * Reference:
 * Pierre L'Ecuyer, "Maximally equidistributed combined Tausworthe generators",
 *   Math. of Comput., 1996, vol 65, pp 203-213.
 *****************************************************************************/

#define TAUS88_NORM 2.3283064365386963e-10 /* 1.0/2^32 */
#define TAUS88_MASK   0xffffffffUL /* required on 64 bit machines */

typedef struct {
  PyObject_HEAD
  unsigned long seed1, seed2, seed3;
} Taus88_object;

staticforward PyTypeObject Taus88_type;

static char Taus88_doc[] =
"Three-component combined Tausworthe RNG by L'Ecuyer, period: ~3.1e26\n\
P. L'Ecuyer, Math. of Comput. (1996) 65, 203-213.\n\
\n\
    Object creation\n\
Taus88(seed1=314159265, seed2=314159263, seed3=314159261)\n\
  seed1 >=2   -- initial first seed\n\
  seed2 >=8   -- initial second seed\n\
  seed3 >=16  -- initial third seed\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
seed1  -- current first seed (read-only)\n\
seed2  -- current second seed (read-only)\n\
seed3  -- current third seed (read-only)";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
Taus88_next_value (Taus88_object *self)
{
  register unsigned long b;

  register unsigned long seed1 = self->seed1; /* optimization; localize */
  register unsigned long seed2 = self->seed2;
  register unsigned long seed3 = self->seed3;

  b = (((seed1 << 13) ^ seed1) >> 19);
  seed1 = ((((seed1 & 4294967294UL) << 12) & TAUS88_MASK) ^ b);
  b = ((( seed2 << 2) ^ seed2) >> 25);
  seed2 = ((((seed2 & 4294967288UL) << 4) & TAUS88_MASK) ^ b);
  b = (((seed3 << 3) ^ seed3) >> 11);
  seed3 = ((((seed3 & 4294967280UL) << 17) & TAUS88_MASK) ^ b);

  self->seed1 = seed1;		/* copy back the localized values */
  self->seed2 = seed2;
  self->seed3 = seed3;

  return (TAUS88_NORM * (double) (seed1 ^ seed2 ^ seed3));
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
Taus88_next (Taus88_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) Taus88_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
Taus88_compute (Taus88_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) Taus88_next_value);
}


/*------------------------------------------------------------*/
static struct PyMethodDef Taus88_methods[] = { /* order optimized */
  {"next", (PyCFunction) Taus88_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) Taus88_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) uniform_01_density, METH_VARARGS, _density_doc},
  {NULL, NULL}
};


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
Taus88_getattr (Taus88_object *self, char *name)
{
  if (strcmp(name, "seed1") == 0) {
    return PyLong_FromUnsignedLong(self->seed1);
  } else if (strcmp(name, "seed2") == 0) {
    return PyLong_FromUnsignedLong(self->seed2);
  } else if (strcmp(name, "seed3") == 0) {
    return PyLong_FromUnsignedLong(self->seed3);
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[sss]", "seed1", "seed2", "seed3");
  } else if ((strcmp(name, _crng_basic_next_value) == 0) ||
	     (strcmp(name, _crng_next_value) == 0)) {
    return PyCObject_FromVoidPtr((void *) Taus88_next_value, NULL);
  }
  return Py_FindMethod(Taus88_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
Taus88_repr (Taus88_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.Taus88(seed1=");
  if (!str) return NULL;
  item = PyLong_FromUnsignedLong(self->seed1);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", seed2="));
  if (!str) return NULL;
  item = PyLong_FromUnsignedLong(self->seed2);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", seed3="));
  if (!str) return NULL;
  item = PyLong_FromUnsignedLong(self->seed3);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
Taus88_str (Taus88_object *self)
{
  char str[64];
  sprintf(str, "<crng.Taus88 object at %p>", self);
  return PyString_FromString (str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = Taus88()" */
Taus88_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"seed1", "seed2", "seed3", NULL};
  Taus88_object *new;
  PyObject *seed1 = NULL, *seed2 = NULL, *seed3 = NULL;

  new = PyObject_NEW(Taus88_object, &Taus88_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new Taus88 %p\n", (void *) new);
#endif

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|OOO", argnames,
				    &seed1, &seed2, &seed3))
    goto error;

  if (seed1 == NULL) {
    new->seed1 = DEFAULT_SEED;
  } else if (PyInt_Check(seed1)) {
    new->seed1 = (unsigned long) PyInt_AS_LONG(seed1);
  } else if (PyLong_Check(seed1)) {
    new->seed1 = PyLong_AsUnsignedLong(seed1);
  } else {
    PyErr_SetString(PyExc_TypeError, _seed_int_error);
    goto error;
  }
  if (new->seed1 < 2) {
    PyErr_SetString(PyExc_ValueError, "seed1 must be >=2");
    goto error;
  }

  if (seed2 == NULL) {
    new->seed2 = DEFAULT_SEED - 2;
  } else if (PyInt_Check(seed2)) {
    new->seed2 = (unsigned long) PyInt_AS_LONG(seed2);
  } else if (PyLong_Check(seed2)) {
    new->seed2 = PyLong_AsUnsignedLong(seed2);
  } else {
    PyErr_SetString(PyExc_TypeError, _seed_int_error);
    goto error;
  }
  if (new->seed2 < 8) {
    PyErr_SetString(PyExc_ValueError, "seed2 must be >=8");
    goto error;
  }

  if (seed3 == NULL) {
    new->seed3 = DEFAULT_SEED - 4;
  } else if (PyInt_Check(seed3)) {
    new->seed3 = (unsigned long) PyInt_AS_LONG(seed3);
  } else if (PyLong_Check(seed3)) {
    new->seed3 = PyLong_AsUnsignedLong(seed3);
  } else {
    PyErr_SetString(PyExc_TypeError, _seed_int_error);
    goto error;
  }
  if (new->seed3 < 16) {
    PyErr_SetString(PyExc_ValueError, "seed3 must be >=16");
    goto error;
  }

  return (PyObject *) new;

error:
  PyMem_Free(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject Taus88_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "Taus88",			/* tp_name */
  sizeof(Taus88_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  generic_dealloc,/* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) Taus88_getattr,	/* tp_getattr  "x.attr"      */
  (setattrfunc) 0,		/* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    Taus88_repr,	/* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  Taus88_next,	/* tp_call    "x()"     */
  (reprfunc)     Taus88_str,	/* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  Taus88_doc			/* tp_doc */
};


/*****************************************************************************
 * MRG32k3a
 *
 * A combined multiple recursive 32-bit random-number generator by
 * Pierre L'Ecuyer.
 *
 * Reference:
 * Pierre L'Ecuyer, "Good parameters and implementations for combined
 *   multiple recursive random number generation", Operations Research,
 *   1998, vol 47, pp 159-164.
 *****************************************************************************/

#define MRG32K3A_M1   4294967087.0 /* 2^32 - 209 */
#define MRG32K3A_M2   4294944443.0 /* 2^32 - 22853 */
#define MRG32K3A_A12     1403580.0
#define MRG32K3A_A13N     810728.0
#define MRG32K3A_A21      527612.0
#define MRG32K3A_A23N    1370589.0
#define MRG32K3A_NORM 2.328306549295728e-10 /* 1/double(M1+1) */

typedef struct {
  PyObject_HEAD
  double s10, s11, s12, s20, s21, s22;
} MRG32k3a_object;

staticforward PyTypeObject MRG32k3a_type;

static char MRG32k3a_doc[] =
"Combined multiple recursive 32-bit RNG by L'Ecuyer, period: ~2^191 = ~3.1e57\n\
P. L'Ecuyer, Operations Res. (1998) 47, 159-164.\n\
\n\
    Object creation\n\
MRG32k3a(s10=314159265L, s11=314159263L, s12=314159261L,\n\
         s20=314159259L, s21=314159257L, s22=314159255L)\n\
  s10, s11, s12 >0  -- initial seed vector s1 elements\n\
  s20, s21, s22 >0  -- initial seed vector s2 elements\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
s10, s11, s12  -- current seed vector s1 elements (read-only)\n\
s20, s21, s22  -- current seed vector s2 elements (read-only)";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
MRG32k3a_next_value (MRG32k3a_object *self)
{
  register long k;
  register double p1, p2;
				/* Component 1 */
  p1 = MRG32K3A_A12 * self->s11 - MRG32K3A_A13N * self->s10;
  k = p1 / MRG32K3A_M1;
  p1 -= k * MRG32K3A_M1;
  if (p1 < 0.0) p1 += MRG32K3A_M1;
  self->s10 = self->s11;
  self->s11 = self->s12;
  self->s12 = p1;
				/* Component 2 */
  p2 = MRG32K3A_A21 * self->s22 - MRG32K3A_A23N * self->s20;
  k = p2 / MRG32K3A_M2;
  p2 -= k * MRG32K3A_M2;
  if (p2 < 0.0) p2 += MRG32K3A_M2;
  self->s20 = self->s21;
  self->s21 = self->s22;
  self->s22 = p2;
				/* Combination */
  if (p1 <= p2) {
    return ((p1 - p2 + MRG32K3A_M1) * MRG32K3A_NORM);
  } else {
    return ((p1 - p2) * MRG32K3A_NORM);
  }
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
MRG32k3a_next (MRG32k3a_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) MRG32k3a_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
MRG32k3a_compute (MRG32k3a_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) MRG32k3a_next_value);
}


/*------------------------------------------------------------*/
static struct PyMethodDef MRG32k3a_methods[] = { /* order optimized */
  {"next", (PyCFunction) MRG32k3a_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) MRG32k3a_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) uniform_01_density, METH_VARARGS, _density_doc},
  {NULL, NULL}
};


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
MRG32k3a_getattr (MRG32k3a_object *self, char *name)
{
  if (strcmp(name, "s10") == 0) {
    return PyLong_FromDouble(self->s10);
  } else if (strcmp(name, "s11") == 0) {
    return PyLong_FromDouble(self->s11);
  } else if (strcmp(name, "s12") == 0) {
    return PyLong_FromDouble(self->s12);
  } else if (strcmp(name, "s20") == 0) {
    return PyLong_FromDouble(self->s20);
  } else if (strcmp(name, "s21") == 0) {
    return PyLong_FromDouble(self->s21);
  } else if (strcmp(name, "s22") == 0) {
    return PyLong_FromDouble(self->s22);
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[sssssss]",
			 "s10", "s11", "s12", "s20", "s21", "s22");
  } else if ((strcmp(name, _crng_basic_next_value) == 0) ||
	     (strcmp(name, _crng_next_value) == 0)) {
    return PyCObject_FromVoidPtr((void *) MRG32k3a_next_value, NULL);
  }
  return Py_FindMethod(MRG32k3a_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
MRG32k3a_repr (MRG32k3a_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.MRG32k3a(s10=");
  if (!str) return NULL;
  item = PyLong_FromDouble(self->s10);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", s11="));
  if (!str) return NULL;
  item = PyLong_FromDouble(self->s11);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", s12="));
  if (!str) return NULL;
  item = PyLong_FromDouble(self->s12);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", s20="));
  if (!str) return NULL;
  item = PyLong_FromDouble(self->s20);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", s21="));
  if (!str) return NULL;
  item = PyLong_FromDouble(self->s21);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", s22="));
  if (!str) return NULL;
  item = PyLong_FromDouble(self->s22);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
MRG32k3a_str (MRG32k3a_object *self)
{
  char str[64];
  sprintf(str, "<crng.MRG32k3a object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = MRG32k3a()" */
MRG32k3a_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"s10", "s11", "s12", "s20", "s21", "s22", NULL};
  MRG32k3a_object *new;
  int i;
  PyObject *s[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
  double sd[6];

  new = PyObject_NEW(MRG32k3a_object, &MRG32k3a_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new MRG32k3a %p\n", (void *) new);
#endif

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|OOOOOO", argnames,
				    &s[0], &s[1], &s[2], &s[3], &s[4], &s[5]))
    goto error;

  for (i = 0; i < 6; i++) {
    if (s[i] == NULL) {
      sd[i] = (double) DEFAULT_SEED;
    } else if (PyInt_Check(s[i])) {
      sd[i] = (double) PyInt_AS_LONG(s[i]);
    } else if (PyLong_Check(s[i])) {
      sd[i] = PyLong_AsDouble(s[i]);
    } else {
      PyErr_SetString(PyExc_TypeError, _seed_int_error);
      goto error;
    }
    if ((sd[i] <= 0.0) || (sd[i] > (double) ULONG_MAX)) {
      PyErr_SetString(PyExc_ValueError, _seed_pos_error);
      goto error;
    }
  }

  new->s10 = sd[0];
  new->s11 = sd[1];
  new->s12 = sd[2];
  new->s20 = sd[3];
  new->s21 = sd[4];
  new->s22 = sd[5];

  return (PyObject *) new;

error:
  PyMem_Free(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject MRG32k3a_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "MRG32k3a",			/* tp_name */
  sizeof(MRG32k3a_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  generic_dealloc,/* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) MRG32k3a_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) 0,		/* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    MRG32k3a_repr,	/* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  MRG32k3a_next,	/* tp_call    "x()"     */
  (reprfunc)     MRG32k3a_str,	/* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  MRG32k3a_doc			/* tp_doc */
};


/*****************************************************************************
 * MT19937
 *
 * Mersenne Twister algorithm by M. Matsumoto & T. Nishimura.
 *
 * Copyright (C) 1997, 1999 Makoto Matsumoto and Takuji Nishimura.
 * When you use this, send an email to: matumoto@math.keio.ac.jp
 * with an appropriate reference to your work.
 *
 * The MT19937 code by Matsumoto and Nishimura is distributed under the
 * GNU Library General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Library General Public License for more details.
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA.
 *
 * Reference:
 * M. Matsumoto & T. Nishimura, ""Mersenne Twister: A 623-Dimensionally
 *   Equidistributed Uniform Pseudo-Random Number Generator",
 *   ACM Transactions on Modeling and Computer Simulation, 1998, vol 8,
 *   pp 3-30.
 *****************************************************************************/

#define MT19937_N		624
#define MT19937_M		397
#define MT19937_K		0x9908b0dfUL
#define MT19937_UPPER_MASK	0x80000000UL
#define MT19937_LOWER_MASK	0x7fffffffUL
#define MT_19937_LOBIT_CHOICE(y) (((y) & 0x1) ? MT19937_K : 0)
#define MT19937_TEMPERING_MASK_B 0x9d2c5680UL
#define MT19937_TEMPERING_MASK_C 0xefc60000UL
#define MT19937_TEMPERING_SHIFT_U(y)  (y >> 11)
#define MT19937_TEMPERING_SHIFT_S(y)  (y << 7)
#define MT19937_TEMPERING_SHIFT_T(y)  (y << 15)
#define MT19937_TEMPERING_SHIFT_L(y)  (y >> 18)
#define MT19937_NORM		2.3283064365386963e-10 /* 1/2^32 */

typedef struct {
  PyObject_HEAD
  unsigned long mt[MT19937_N];
  int mti;
} MT19937_object;

staticforward PyTypeObject MT19937_type;

static char MT19937_doc[] =
"Mersenne Twister by Matsumoto and Nishimura, period: 2^19937-1 = ~4.3e6001\n\
M. Matsumoto & T. Nishimura, ACM Trans. Mod. Comp. Sim. (1998) 8, 3-30.\n\
\n\
    Object creation\n\
MT19937(seed=314159265, state=None)\n\
  seed >0  -- initial seed\n\
  state    -- set the initial state from a state tuple\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
state  -- the state tuple reflecting the current state (read-only)";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
MT19937_next_value (MT19937_object *self)
{
  register unsigned long y;

  if (self->mti >= MT19937_N) {
    register int kk;
    register unsigned long *mt = self->mt;

    for (kk = 0; kk < MT19937_N - MT19937_M; kk++) {
      y = (mt[kk] & MT19937_UPPER_MASK) | (mt[kk+1] & MT19937_LOWER_MASK);
      mt[kk] = mt[kk+MT19937_M] ^ (y >> 1) ^ MT_19937_LOBIT_CHOICE(y);
    }
    for (; kk < MT19937_N - 1; kk++) {
      y = (mt[kk] & MT19937_UPPER_MASK) | (mt[kk+1] & MT19937_LOWER_MASK);
      mt[kk] = mt[kk+(MT19937_M-MT19937_N)] ^ (y >> 1) ^ MT_19937_LOBIT_CHOICE(y);
    }
    y = (mt[MT19937_N-1] & MT19937_UPPER_MASK) | (mt[0] & MT19937_LOWER_MASK);
    mt[MT19937_N-1] = mt[MT19937_M-1] ^ (y >> 1) ^ MT_19937_LOBIT_CHOICE(y);

    self->mti = 0;
  }

  y = self->mt[self->mti++];
  y ^= MT19937_TEMPERING_SHIFT_U(y);
  y ^= MT19937_TEMPERING_SHIFT_S(y) & MT19937_TEMPERING_MASK_B;
  y ^= MT19937_TEMPERING_SHIFT_T(y) & MT19937_TEMPERING_MASK_C;
  y ^= MT19937_TEMPERING_SHIFT_L(y);

  if (y) {
    return (MT19937_NORM * (double) y);
  } else {			/* do not accept 0; if so, get next value */
    return MT19937_next_value(self);
  }
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
MT19937_next (MT19937_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) MT19937_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
MT19937_compute (MT19937_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) MT19937_next_value);
}


/*------------------------------------------------------------*/
static struct PyMethodDef MT19937_methods[] = { /* order optimized */
  {"next", (PyCFunction) MT19937_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) MT19937_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) uniform_01_density, METH_VARARGS, _density_doc},
  {NULL, NULL}
};


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
MT19937_getattr (MT19937_object *self, char *name)
{
  if (strcmp(name, "state") == 0) {
    int i;
    PyObject *item, *tuple;

    tuple = PyTuple_New(MT19937_N + 1);
    if (!tuple) return NULL;
    item = PyInt_FromLong((long) self->mti);
    if (!item) goto error;
    PyTuple_SET_ITEM(tuple, 0, item);

    for (i = 0; i < MT19937_N; i++) {
      item = PyLong_FromUnsignedLong(self->mt[i]);
      if (!item) goto error;
      PyTuple_SET_ITEM(tuple, i + 1, item);
    }

    return tuple;

  error:
    Py_DECREF(tuple);
    return NULL;

  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[s]", "state");
  } else if ((strcmp(name, _crng_basic_next_value) == 0) ||
	     (strcmp(name, _crng_next_value) == 0)) {
    return PyCObject_FromVoidPtr((void *) MT19937_next_value, NULL);
  }
  return Py_FindMethod(MT19937_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
MT19937_repr (MT19937_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.MT19937(state=");
  if (!str) return NULL;
  item = MT19937_getattr(self, "state");
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
MT19937_str (MT19937_object *self)
{
  char str[64];
  sprintf(str, "<crng.MT19937 object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = MT19937()" */
MT19937_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"seed", "state", NULL};
  MT19937_object *new;
  long signedseed = DEFAULT_SEED;
  unsigned long seed;
  PyObject *state = NULL;
  int i;

  new = PyObject_NEW(MT19937_object, &MT19937_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new MT19937 %p\n", (void *) new);
#endif

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|lO!", argnames,
				    &signedseed, &PyTuple_Type, &state))
    goto error;

  if (state) {			/* state tuple given; ignore seed */
    PyObject *item;

    if (PyTuple_Size(state) != MT19937_N + 1) goto state_error;
    item = PyTuple_GET_ITEM(state, 0);
    if (! PyInt_Check(item)) goto state_error;
    new->mti = (int) PyInt_AS_LONG(item);
    if ((new->mti <= 0) || (new->mti > MT19937_N)) goto state_error;

    for (i = 0; i < MT19937_N; i++) {
      item = PyTuple_GET_ITEM(state, i + 1);
      if (! PyLong_Check(item)) goto state_error;
      new->mt[i] = PyLong_AsUnsignedLong(item);
    }

  } else {			/* compute initial state from seed */
    if (signedseed <= 0) {
      PyErr_SetString(PyExc_ValueError, _seed_pos_error);
      goto error;
    }

    seed = signedseed;

    for (i = 0; i < MT19937_N; i++) {
      new->mt[i] = seed & 0xffff0000U;
      seed = 69069U * seed + 1;
      new->mt[i] |= (seed & 0xffff0000U) >> 16;
      seed = 69069U * seed + 1;
    }
    new->mti = MT19937_N;
  }

  return (PyObject *) new;

state_error:
  PyErr_SetString(PyExc_ValueError, _state_error);
error:
  PyMem_Free(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject MT19937_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "MT19937",			/* tp_name */
  sizeof(MT19937_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  generic_dealloc,/* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) MT19937_getattr,/* tp_getattr  "x.attr"      */
  (setattrfunc) 0,		/* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    MT19937_repr,	/* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  MT19937_next,	/* tp_call    "x()"     */
  (reprfunc)     MT19937_str,	/* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  MT19937_doc			/* tp_doc */
};


/*------------------------------------------------------------*/
static PyObject *		/* default RNG object for non-std deviates */
new_default_rng (void)
{
  ParkMiller_object *new;

  new = PyObject_NEW(ParkMiller_object, &ParkMiller_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new ParkMiller %p\n", (void *) new);
#endif

  new->seed = DEFAULT_SEED;

  return (PyObject *) new;
}


/*****************************************************************************
 * UniformDeviate
 *
 * Uniform deviate in the open interval (a,b), where a<b.
 *****************************************************************************/

typedef struct {
  PyObject_HEAD
  PyObject *rng;
  double (*next_value) (PyObject *rng);
  double a, width;
} UniformDeviate_object;

staticforward PyTypeObject UniformDeviate_type;

static char UniformDeviate_doc[] =
"Uniform deviate in the open interval (a,b).\n\
\n\
    Object creation\n\
UniformDeviate(rng=ParkMiller(), a=0.0, b=1.0)\n\
  rng   -- basic RNG; must be a crng type\n\
  a, b  -- limits of the open interval, such that a<b\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
rng   -- basic RNG; must be a crng type\n\
a, b  -- limits of the open interval, such that a<b";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
UniformDeviate_next_value (UniformDeviate_object *self)
{
  return (self->a + self->width * self->next_value(self->rng));
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
UniformDeviate_next (UniformDeviate_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) UniformDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
UniformDeviate_compute (UniformDeviate_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) UniformDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.density()" */
UniformDeviate_density (UniformDeviate_object *self, PyObject *args)
{
  double x;

  if (! PyArg_ParseTuple(args, "d", &x)) return NULL;

  if ((x <= self->a) || (x >= self->a + self->width)) {
    return PyFloat_FromDouble(0.0);
  } else {
    return PyFloat_FromDouble(1.0 / self->width);
  }
}


/*------------------------------------------------------------*/
static struct PyMethodDef UniformDeviate_methods[] = { /* order optimized */
  {"next", (PyCFunction) UniformDeviate_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) UniformDeviate_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) UniformDeviate_density, METH_VARARGS, _density_doc},
  {NULL, NULL, 0, NULL}
};


/*------------------------------------------------------------*/
static void			/* instance destruction */
UniformDeviate_dealloc (UniformDeviate_object *self)
{
#ifdef DEBUG
  fprintf(stderr, "*** UniformDeviate_dealloc %p\n", (void *) self);
#endif
  Py_XDECREF(self->rng);
  PyMem_Free(self);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
UniformDeviate_getattr (UniformDeviate_object *self, char *name)
{
  if (strcmp(name, "rng") == 0) {
    Py_INCREF(self->rng);
    return self->rng;
  } else if (strcmp(name, "a") == 0) {
    return PyFloat_FromDouble(self->a);
  } else if (strcmp(name, "b") == 0) {
    return PyFloat_FromDouble(self->a + self->width); 
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[sss]", "rng", "a", "b");
  } else if (strcmp(name, _crng_next_value) == 0) {
    return PyCObject_FromVoidPtr((void *) UniformDeviate_next_value, NULL);
  }
  return Py_FindMethod(UniformDeviate_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static int			/* on "instance.attr = obj */
UniformDeviate_setattr (UniformDeviate_object *self, char *name, PyObject *obj)
{
  if (!obj) {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;

  } else if (strcmp(name, "rng") == 0) {
    PyObject *cobj = PyObject_GetAttrString(obj, _crng_basic_next_value);
    if (cobj && PyCObject_Check(cobj)) {
      Py_DECREF(self->rng);
      self->rng = obj;
      Py_INCREF(self->rng);
      self->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
    } else {
      Py_XDECREF(cobj);
      PyErr_SetString(PyExc_TypeError, _crng_basic_error);
      return -1;
    }

  } else if (strcmp(name, "a") == 0) {
    double a;
    if (PyFloat_Check(obj)) {
      a = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      a = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _a_float_error);
      return -1;
    }
    if (a >= self->a + self->width) {
      PyErr_SetString(PyExc_ValueError, _ab_lt_error);
      return -1;
    }
    self->a = a;

  } else if (strcmp(name, "b") == 0) {
    double b;
    if (PyFloat_Check(obj)) {
      b = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      b = (double) PyInt_Check(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _b_float_error);
      return -1;
    }
    if (self->a >= b) {
      PyErr_SetString(PyExc_ValueError, _ab_lt_error);
      return -1;
    }
    self->width = b - self->a;

  } else {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;
  }

  return 0;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
UniformDeviate_repr (UniformDeviate_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.UniformDeviate(a=");
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->a);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", b="));
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->a + self->width);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", rng="));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyObject_Repr(self->rng));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
UniformDeviate_str (UniformDeviate_object *self)
{
  char str[64];
  sprintf(str, "<crng.UniformDeviate object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = UniformDeviate()" */
UniformDeviate_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"rng", "a", "b", NULL};
  UniformDeviate_object *new;
  PyObject *cobj = NULL;
  double b;

  new = PyObject_NEW(UniformDeviate_object, &UniformDeviate_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new UniformDeviate %p\n", (void *) new);
#endif

  new->rng = NULL;
  new->a = 0.0;
  b = 1.0;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|Odd", argnames,
				    &(new->rng), &(new->a), &b))
    goto error;

  if (new->rng == NULL) {
    new->rng = new_default_rng();
    if (new->rng == NULL) goto error;
  } else {
    Py_INCREF(new->rng);
  }

  cobj = PyObject_GetAttrString(new->rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    new->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if (new->a >= b) {
    PyErr_SetString(PyExc_ValueError, _ab_lt_error);
    goto error;
  } else {
    new->width = b - new->a;
  }

  return (PyObject *) new;

error:
  Py_XDECREF(cobj);
  Py_XDECREF(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject UniformDeviate_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "UniformDeviate",		/* tp_name */
  sizeof(UniformDeviate_object),/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  UniformDeviate_dealloc, /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) UniformDeviate_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) UniformDeviate_setattr, /* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    UniformDeviate_repr, /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  UniformDeviate_next, /* tp_call    "x()"     */
  (reprfunc)     UniformDeviate_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  UniformDeviate_doc		/* tp_doc */
};


/*****************************************************************************
 * ExponentialDeviate
 *
 * Exponential deviate with a given mean.
 *****************************************************************************/

typedef struct {
  PyObject_HEAD
  PyObject *rng;
  double (*next_value) (PyObject *rng);
  double mean;
} ExponentialDeviate_object;

staticforward PyTypeObject ExponentialDeviate_type;

static char ExponentialDeviate_doc[] =
"Exponential deviate with a given mean.\n\
\n\
    Object creation\n\
ExponentialDeviate(rng=ParkMiller(), mean=1.0)\n\
  rng   -- basic RNG; must be a crng type\n\
  mean  -- deviate mean value, must be positive\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
rng   -- basic RNG; must be a crng type\n\
mean  -- deviate mean value; must be positive";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
ExponentialDeviate_next_value (ExponentialDeviate_object *self)
{
  return (-log (self->next_value(self->rng)) * self->mean);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
ExponentialDeviate_next (ExponentialDeviate_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) ExponentialDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
ExponentialDeviate_compute (ExponentialDeviate_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) ExponentialDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.density()" */
ExponentialDeviate_density (ExponentialDeviate_object *self, PyObject *args)
{
  double x;

  if (! PyArg_ParseTuple(args, "d", &x)) return NULL;

  if (x <= 0.0) {
    return PyFloat_FromDouble(0.0);
  } else {
    return PyFloat_FromDouble(exp (- x / self->mean) / self->mean);
  }
}


/*------------------------------------------------------------*/
static struct PyMethodDef ExponentialDeviate_methods[] = {/* order optimized */
  {"next", (PyCFunction) ExponentialDeviate_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) ExponentialDeviate_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) ExponentialDeviate_density, METH_VARARGS, _density_doc},
  {NULL, NULL, 0, NULL}
};


/*------------------------------------------------------------*/
static void			/* instance destruction */
ExponentialDeviate_dealloc (ExponentialDeviate_object *self)
{
#ifdef DEBUG
  fprintf(stderr, "*** ExponentialDeviate_dealloc %p\n", (void *) self);
#endif
  Py_XDECREF(self->rng);
  PyMem_Free(self);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
ExponentialDeviate_getattr (ExponentialDeviate_object *self, char *name)
{
  if (strcmp(name, "rng") == 0) {
    Py_INCREF(self->rng);
    return self->rng;
  } else if (strcmp(name, "mean") == 0) {
    return PyFloat_FromDouble(self->mean);
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[ss]", "rng", "mean");
  } else if (strcmp(name, _crng_next_value) == 0) {
    return PyCObject_FromVoidPtr((void *) ExponentialDeviate_next_value, NULL);
  }
  return Py_FindMethod(ExponentialDeviate_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static int			/* on "instance.attr = obj */
ExponentialDeviate_setattr (ExponentialDeviate_object *self,
			    char *name, PyObject *obj)
{
  if (!obj) {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;

  } else if (strcmp(name, "rng") == 0) {
    PyObject *cobj = PyObject_GetAttrString(obj, _crng_basic_next_value);
    if (cobj && PyCObject_Check(cobj)) {
      Py_DECREF(self->rng);
      self->rng = obj;
      Py_INCREF(self->rng);
      self->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
    } else {
      Py_XDECREF(cobj);
      PyErr_SetString(PyExc_TypeError, _crng_basic_error);
      return -1;
    }

  } else if (strcmp(name, "mean") == 0) {
    double mean;
    if (PyFloat_Check(obj)) {
      mean = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      mean = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _mean_float_error);
      return -1;
    }
    if (mean <= 0.0) {
      PyErr_SetString(PyExc_ValueError, _mean_pos_error);
      return -1;
    }
    self->mean = mean;

  } else {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;
  }

  return 0;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
ExponentialDeviate_repr (ExponentialDeviate_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.ExponentialDeviate(mean=");
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->mean);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", rng="));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyObject_Repr(self->rng));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
ExponentialDeviate_str (ExponentialDeviate_object *self)
{
  char str[64];
  sprintf(str, "<crng.ExponentialDeviate object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = ExponentialDeviate()" */
ExponentialDeviate_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"rng", "mean", NULL};
  ExponentialDeviate_object *new;
  PyObject *cobj = NULL;

  new = PyObject_NEW(ExponentialDeviate_object, &ExponentialDeviate_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new ExponentialDeviate %p\n", (void *) new);
#endif

  new->rng = NULL;
  new->mean = 1.0;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|Od", argnames,
				     &(new->rng), &(new->mean)))
    goto error;

  if (new->rng == NULL) {
    new->rng = new_default_rng();
    if (new->rng == NULL) goto error;
  } else {
    Py_INCREF(new->rng);
  }

  cobj = PyObject_GetAttrString(new->rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    new->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if (new->mean <= 0.0) {
    PyErr_SetString(PyExc_ValueError, _mean_pos_error);
    goto error;
  }

  return (PyObject *) new;

error:
  Py_XDECREF(cobj);
  Py_XDECREF(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject ExponentialDeviate_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "ExponentialDeviate",		/* tp_name */
  sizeof(ExponentialDeviate_object), /* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  ExponentialDeviate_dealloc, /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) ExponentialDeviate_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) ExponentialDeviate_setattr, /* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    ExponentialDeviate_repr, /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  ExponentialDeviate_next, /* tp_call    "x()"     */
  (reprfunc)     ExponentialDeviate_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  ExponentialDeviate_doc	/* tp_doc */
};


/*****************************************************************************
 * NormalDeviate
 *
 * Normal (gaussian) deviate with a given mean and standard deviation.
 *
 * This implementation uses the Box-Muller method:
 * G.E.P. Box & M.E. Muller, Annals Math. Stat. (1958) 29, 610-611.
 *****************************************************************************/

typedef struct {
  PyObject_HEAD
  PyObject *rng;
  double (*next_value) (PyObject *rng);
  double mean, stdev;
  int stored;
  double store;
} NormalDeviate_object;

staticforward PyTypeObject NormalDeviate_type;

static char NormalDeviate_doc[] =
"Normal (Gaussian) deviate with a given mean and standard deviation.\n\
\n\
    Object creation\n\
NormalDeviate(rng=ParkMiller(), mean=0.0, stdev=1.0, stored=None)\n\
  rng    -- basic RNG; must be a crng type\n\
  mean   -- deviate mean value\n\
  stdev  -- deviate standard deviation, must be positive\n\
  stored -- next raw random value; saved from the previous calculation\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
rng    -- basic RNG; must be a crng type\n\
mean   -- deviate mean value\n\
stdev  -- deviate standard deviation, must be positive\n\
stored -- next raw random value; saved from the previous calculation";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
NormalDeviate_next_value (NormalDeviate_object *self)
{
  if (self->stored) {
    self->stored = 0;
    return self->mean + self->stdev * self->store;

  } else {
    register double v1, v2, s;

    do {
      v1 = 2.0 * self->next_value(self->rng) - 1.0;
      v2 = 2.0 * self->next_value(self->rng) - 1.0;
      s = v1*v1 + v2*v2;
    } while (s >= 1.0 || s == 0.0);

    s = sqrt(-2.0 * log(s) / s);

    self->store = s * v2;
    self->stored = 1;

    return self->mean + self->stdev * s * v1;
  }
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
NormalDeviate_next (NormalDeviate_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) NormalDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
NormalDeviate_compute (NormalDeviate_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) NormalDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.density()" */
NormalDeviate_density (NormalDeviate_object *self, PyObject *args)
{
  double x;
  register double y;

  if (! PyArg_ParseTuple(args, "d", &x)) return NULL;

  y = (x - self->mean) / self->stdev;
  return PyFloat_FromDouble(INV_SQRT_2_PI * exp(-0.5 * y * y) / self->stdev);
}


/*------------------------------------------------------------*/
static struct PyMethodDef NormalDeviate_methods[] = {/* order optimized */
  {"next", (PyCFunction) NormalDeviate_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) NormalDeviate_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) NormalDeviate_density, METH_VARARGS, _density_doc},
  {NULL, NULL, 0, NULL}
};


/*------------------------------------------------------------*/
static void			/* instance destruction */
NormalDeviate_dealloc (NormalDeviate_object *self)
{
#ifdef DEBUG
  fprintf(stderr, "*** NormalDeviate_dealloc %p\n", (void *) self);
#endif
  Py_XDECREF(self->rng);
  PyMem_Free(self);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
NormalDeviate_getattr (NormalDeviate_object *self, char *name)
{
  if (strcmp(name, "rng") == 0) {
    Py_INCREF(self->rng);
    return self->rng;
  } else if (strcmp(name, "mean") == 0) {
    return PyFloat_FromDouble(self->mean);
  } else if (strcmp(name, "stdev") == 0) {
    return PyFloat_FromDouble(self->stdev);
  } else if (strcmp(name, "stored") == 0) {
    if (self->stored) {
      return PyFloat_FromDouble(self->store);
    } else {
      Py_INCREF(Py_None);
      return Py_None;
    }
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[ssss]", "rng", "mean", "stdev", "stored");
  } else if (strcmp(name, _crng_next_value) == 0) {
    return PyCObject_FromVoidPtr((void *) NormalDeviate_next_value, NULL);
  }
  return Py_FindMethod(NormalDeviate_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static int			/* on "instance.attr = obj */
NormalDeviate_setattr (NormalDeviate_object *self, char *name, PyObject *obj)
{
  if (!obj) {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;

  } else if (strcmp(name, "rng") == 0) {
    PyObject *cobj = PyObject_GetAttrString(obj, _crng_basic_next_value);
    if (cobj && PyCObject_Check(cobj)) {
      Py_DECREF(self->rng);
      self->rng = obj;
      Py_INCREF(self->rng);
      self->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
    } else {
      Py_XDECREF(cobj);
      PyErr_SetString(PyExc_TypeError, _crng_basic_error);
      return -1;
    }

  } else if (strcmp(name, "mean") == 0) {
    double mean;
    if (PyFloat_Check(obj)) {
      mean = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      mean = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _mean_float_error);
      return -1;
    }
    self->mean = mean;

  } else if (strcmp(name, "stdev") == 0) {
    double stdev;
    if (PyFloat_Check(obj)) {
      stdev = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      stdev = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _stdev_float_error);
      return -1;
    }
    if (stdev <= 0.0) {
      PyErr_SetString(PyExc_ValueError, _stdev_pos_error);
      return -1;
    }
    self->stdev = stdev;

  } else {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;
  }

  return 0;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
NormalDeviate_repr (NormalDeviate_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.NormalDeviate(mean=");
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->mean);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", stdev="));
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->stdev);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", stored="));
  if (!str) return NULL;
  if (self->stored) {
    item = PyFloat_FromDouble(self->store);
    if (!item) goto error;
    PyString_ConcatAndDel(&str, PyObject_Repr(item));
    Py_DECREF(item);
  } else {
    PyString_Concat(&str, PyObject_Repr(Py_None));
  }
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", rng="));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyObject_Repr(self->rng));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
NormalDeviate_str (NormalDeviate_object *self)
{
  char str[64];
  sprintf(str, "<crng.NormalDeviate object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = NormalDeviate()" */
NormalDeviate_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"rng", "mean", "stdev", "stored", NULL};
  NormalDeviate_object *new;
  PyObject *cobj = NULL, *stored = Py_None;

  new = PyObject_NEW(NormalDeviate_object, &NormalDeviate_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new NormalDeviate %p\n", (void *) new);
#endif

  new->rng = NULL;
  new->mean = 0.0;
  new->stdev = 1.0;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|OddO", argnames,
				    &(new->rng), &(new->mean), &(new->stdev),
				    &stored))
    goto error;

  if (new->rng == NULL) {
    new->rng = new_default_rng();
    if (new->rng == NULL) goto error;
  } else {
    Py_INCREF(new->rng);
  }

  cobj = PyObject_GetAttrString(new->rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    new->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if (new->stdev <= 0.0) {
    PyErr_SetString(PyExc_ValueError, _stdev_pos_error);
    goto error;
  }

  if (stored == Py_None) {
    new->stored = 0;
  } else if (PyInt_Check(stored)) {
    new->stored = 1;
    new->store = (double) PyInt_AS_LONG(stored);
  } else if (PyFloat_Check(stored)) {
    new->stored = 1;
    new->store = PyFloat_AsDouble(stored);
  } else {
    PyErr_SetString(PyExc_ValueError, "'stored' must be float or None");
    goto error;
  }

  return (PyObject *) new;

error:
  Py_XDECREF(cobj);
  Py_XDECREF(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject NormalDeviate_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "NormalDeviate",		/* tp_name */
  sizeof(NormalDeviate_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  NormalDeviate_dealloc, /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) NormalDeviate_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) NormalDeviate_setattr, /* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    NormalDeviate_repr, /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  NormalDeviate_next, /* tp_call    "x()"     */
  (reprfunc)     NormalDeviate_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  NormalDeviate_doc		/* tp_doc */
};


/*****************************************************************************
 * GammaDeviate
 *
 * Gamma deviate with a given order and scale, calculated either
 * directly or by the rejection method.
 *****************************************************************************/

typedef struct {
  PyObject_HEAD
  PyObject *rng;
  double (*next_value) (PyObject *rng);
  double order, scale, fraction;
  unsigned long lorder, direct;
} GammaDeviate_object;

staticforward PyTypeObject GammaDeviate_type;

static char GammaDeviate_doc[] =
"Gamma deviate with a given order and scale.\n\
\n\
    Object creation\n\
GammaDeviate(rng=ParkMiller(), order=2, scale=1.0, direct=(order<=12))\n\
  rng     -- basic RNG; must be a crng type\n\
  order   -- order of the gamma distribution, must be positive\n\
  scale   -- scale value, must be positive\n\
  direct  -- direct method of calculation, otherwise rejection method; boolean\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
rng     -- basic RNG; must be a crng type\n\
order   -- order of the gamma distribution, must be positive\n\
scale   -- scale value, must be positive\n\
direct  -- direct method of calculation, otherwise rejection method; boolean";


/*------------------------------------------------------------*/
static double			/* fractional part of order */
GammaDeviate_fraction (double (*next_value) (PyObject *rng),
		       PyObject *rng, double fraction)
{
  register double p, q, x, u, v;

  p = M_E / (fraction + M_E);
  do {
    u = next_value(rng);
    v = next_value(rng);
    if (u < p) {
      x = exp((1.0 / fraction) * log(v));
      q = exp(-x);
    } else {
      x = 1.0 - log(v);
      q = exp((fraction - 1.0) * log(x));
    }
  } while (next_value(rng) >= q);

  return x;
}


/*------------------------------------------------------------*/
static double			/* direct method for integer order */
GammaDeviate_direct (double (*next_value) (PyObject *rng),
		     PyObject *rng, unsigned long order)
{
  register double x;
  register unsigned long n;

/*    assert (order > 0); */

  x = next_value(rng);
  for (n = order - 1; n; n--) x *= next_value(rng);
  return -log(x);
}


/*------------------------------------------------------------*/
static double			/* rejection method for float order */
GammaDeviate_rejection (double (*next_value) (PyObject *rng),
			PyObject *rng, double order)
{
    register double x, s, y, v;

    s = sqrt(2.0 * order - 1.0);
    do {
      do {
	y = tan(M_PI * next_value(rng));
	x = s * y + order - 1.0;
      } while (x <= 0.0);
      v = next_value(rng);
    } while (v > (1.0 + y * y) * exp((order - 1.0) *
				     log(x / (order - 1.0)) - s * y));

    return x;
}


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
GammaDeviate_next_value (GammaDeviate_object *self)
{
  register double x;

  if (self->lorder == 0) {	/* order < 1.0 */
    x = GammaDeviate_fraction(self->next_value, self->rng, self->fraction);

  } else if (self->direct) {
    x = GammaDeviate_direct(self->next_value, self->rng, self->lorder);
    if (self->fraction != 0.0)  x += GammaDeviate_fraction(self->next_value,
							   self->rng,
							   self->fraction);

  } else {
    x = GammaDeviate_rejection(self->next_value, self->rng, self->order);
  }

  return self->scale * x;
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
GammaDeviate_next (GammaDeviate_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) GammaDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
GammaDeviate_compute (GammaDeviate_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) GammaDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.density()" */
GammaDeviate_density (GammaDeviate_object *self, PyObject *args)
{
  double x, y;

  if (! PyArg_ParseTuple(args, "d", &x)) return NULL;

  if (x < 0.0) {
    y = 0.0;
  } else if (x == 0.0) {
    if (self->order == 1.0) {
      y = 1.0 / self->scale;
    } else {
      y = 0.0;
    }
  } else if (self->order == 1.0) {
    y = exp(-x / self->scale) / self->scale;
  } else {
    y = exp((self->order - 1.0) * log(x / self->scale) -
	    x / self->scale - log_gamma(self->order)) / self->scale;
  }
  return PyFloat_FromDouble(y);
}


/*------------------------------------------------------------*/
static struct PyMethodDef GammaDeviate_methods[] = {/* order optimized */
  {"next", (PyCFunction) GammaDeviate_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) GammaDeviate_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) GammaDeviate_density, METH_VARARGS, _density_doc},
  {NULL, NULL, 0, NULL}
};


/*------------------------------------------------------------*/
static void			/* instance destruction */
GammaDeviate_dealloc (GammaDeviate_object *self)
{
#ifdef DEBUG
  fprintf(stderr, "*** GammaDeviate_dealloc %p\n", (void *) self);
#endif
  Py_XDECREF(self->rng);
  PyMem_Free(self);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
GammaDeviate_getattr (GammaDeviate_object *self, char *name)
{
  if (strcmp(name, "rng") == 0) {
    Py_INCREF(self->rng);
    return self->rng;
  } else if (strcmp(name, "order") == 0) {
    return PyFloat_FromDouble(self->order);
  } else if (strcmp(name, "scale") == 0) {
    return PyFloat_FromDouble(self->scale);
  } else if (strcmp(name, "direct") == 0) {
    return PyInt_FromLong(self->direct);
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[ssss]", "rng", "order", "scale", "direct");
  } else if (strcmp(name, _crng_next_value) == 0) {
    return PyCObject_FromVoidPtr((void *) GammaDeviate_next_value, NULL);
  }
  return Py_FindMethod(GammaDeviate_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static int			/* on "instance.attr = obj */
GammaDeviate_setattr (GammaDeviate_object *self, char *name, PyObject *obj)
{
  if (!obj) {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;

  } else if (strcmp(name, "rng") == 0) {
    PyObject *cobj = PyObject_GetAttrString(obj, _crng_basic_next_value);
    if (cobj && PyCObject_Check(cobj)) {
      Py_DECREF(self->rng);
      self->rng = obj;
      Py_INCREF(self->rng);
      self->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
    } else {
      Py_XDECREF(cobj);
      PyErr_SetString(PyExc_TypeError, _crng_basic_error);
      return -1;
    }

  } else if (strcmp(name, "order") == 0) {
    double order;
    if (PyFloat_Check(obj)) {
      order = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      order = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _order_float_error);
      return -1;
    }
    if (order <= 0.0) {
      PyErr_SetString(PyExc_ValueError, _order_pos_error);
      return -1;
    }
    self->order = order;
    self->lorder = (long) floor(self->order);
    self->fraction = self->order - (double) self->lorder;

  } else if (strcmp(name, "scale") == 0) {
    double scale;
    if (PyFloat_Check(obj)) {
      scale = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      scale = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _scale_float_error);
      return -1;
    }
    if (scale <= 0.0) {
      PyErr_SetString(PyExc_ValueError, _scale_pos_error);
      return -1;
    }
    self->scale = scale;

  } else if (strcmp(name, "direct") == 0) {
    if (PyInt_Check(obj)) {
      self->direct = (PyInt_AsLong(obj) != 0);
    } else {
      PyErr_SetString(PyExc_TypeError, _direct_int_error);
      return -1;
    }

  } else {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;
  }

  return 0;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
GammaDeviate_repr (GammaDeviate_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.GammaDeviate(order=");
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->order);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", scale="));
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->scale);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", direct="));
  if (!str) return NULL;
  item = PyInt_FromLong(self->direct);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  PyString_ConcatAndDel(&str, PyString_FromString(", rng="));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyObject_Repr(self->rng));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
GammaDeviate_str (GammaDeviate_object *self)
{
  char str[64];
  sprintf(str, "<crng.GammaDeviate object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = GammaDeviate()" */
GammaDeviate_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"rng", "order", "scale", "direct", NULL};
  GammaDeviate_object *new;
  PyObject *cobj = NULL;

  new = PyObject_NEW(GammaDeviate_object, &GammaDeviate_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new GammaDeviate %p\n", (void *) new);
#endif

  new->rng = NULL;
  new->order = 2.0;
  new->scale = 1.0;
  new->direct = DEFAULT_SEED;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|Oddl", argnames,
				    &(new->rng), &(new->order), &(new->scale),
				    &(new->direct)))
    goto error;

  if (new->rng == NULL) {
    new->rng = new_default_rng();
    if (new->rng == NULL) goto error;
  } else {
    Py_INCREF(new->rng);
  }

  cobj = PyObject_GetAttrString(new->rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    new->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if (new->order <= 0.0) {
    PyErr_SetString(PyExc_ValueError, _order_pos_error);
    goto error;
  }

  if (new->scale <= 0.0) {
    PyErr_SetString(PyExc_ValueError, _scale_pos_error);
    goto error;
  }

  new->lorder = (long) floor(new->order);
  new->fraction = new->order - (double) new->lorder;

  if (new->direct == DEFAULT_SEED) {
    new->direct = (new->order <= 12.0);
  } else {
    new->direct = (new->direct != 0);
  }

  return (PyObject *) new;

error:
  Py_XDECREF(cobj);
  Py_XDECREF(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject GammaDeviate_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "GammaDeviate",		/* tp_name */
  sizeof(GammaDeviate_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  GammaDeviate_dealloc, /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) GammaDeviate_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) GammaDeviate_setattr, /* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    GammaDeviate_repr, /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  GammaDeviate_next, /* tp_call    "x()"     */
  (reprfunc)     GammaDeviate_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  GammaDeviate_doc		/* tp_doc */
};


/*****************************************************************************
 * BetaDeviate
 *
 * Beta deviate for the parameters a and b.
 *****************************************************************************/

typedef struct {
  PyObject_HEAD
  GammaDeviate_object *agamma, *bgamma;
} BetaDeviate_object;

staticforward PyTypeObject BetaDeviate_type;

static char BetaDeviate_doc[] =
"Beta deviate for the parameters a and b.\n\
\n\
    Object creation\n\
BetaDeviate(rng=ParkMiller(), a=0.0, b=0.0)\n\
  rng     -- basic RNG; must be a crng type\n\
  a, b    -- parameters, must be positive\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as a float or tuple of floats\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
rng     -- basic RNG; must be a crng type\n\
a, b    -- parameters, must be positive";


/*------------------------------------------------------------*/
static double			/* actually compute the next value */
BetaDeviate_next_value (BetaDeviate_object *self)
{
  double x1 = GammaDeviate_next_value(self->agamma);
  double x2 = GammaDeviate_next_value(self->bgamma);

  return x1 / (x1 + x2);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
BetaDeviate_next (BetaDeviate_object *self, PyObject *args)
{
  return next_double((PyObject *) self, args,
		     (double (*) (PyObject *)) BetaDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
BetaDeviate_compute (BetaDeviate_object *self, PyObject *args)
{
  return compute_double((PyObject *) self, args,
			(double (*) (PyObject *)) BetaDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.density()" */
BetaDeviate_density (BetaDeviate_object *self, PyObject *args)
{
  double x;

  if (! PyArg_ParseTuple(args, "d", &x)) return NULL;

  if ((x < 0.0) || (x > 1.0)) {
    return PyFloat_FromDouble(0.0);
  } else {
    double a = self->agamma->order;
    double b = self->bgamma->order;
    double gab = log_gamma(a + b);
    double ga = log_gamma(a);
    double gb = log_gamma(b);
    return PyFloat_FromDouble(exp(gab - ga - gb) *
			      pow(x, a - 1.0) * pow(1.0 - x, b - 1.0));
  }
}


/*------------------------------------------------------------*/
static struct PyMethodDef BetaDeviate_methods[] = {/* order optimized */
  {"next", (PyCFunction) BetaDeviate_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) BetaDeviate_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) BetaDeviate_density, METH_VARARGS, _density_doc},
  {NULL, NULL, 0, NULL}
};


/*------------------------------------------------------------*/
static void			/* instance destruction */
BetaDeviate_dealloc (BetaDeviate_object *self)
{
#ifdef DEBUG
  fprintf(stderr, "*** BetaDeviate_dealloc %p\n", (void *) self);
#endif
  Py_XDECREF(self->agamma);
  Py_XDECREF(self->bgamma);
  PyMem_Free(self);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
BetaDeviate_getattr (BetaDeviate_object *self, char *name)
{
  if (strcmp(name, "rng") == 0) {
    Py_INCREF(self->agamma->rng);
    return self->agamma->rng;
  } else if (strcmp(name, "a") == 0) {
    return PyFloat_FromDouble(self->agamma->order);
  } else if (strcmp(name, "b") == 0) {
    return PyFloat_FromDouble(self->bgamma->order);
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[sss]", "rng", "a", "b");
  } else if (strcmp(name, _crng_next_value) == 0) {
    return PyCObject_FromVoidPtr((void *) BetaDeviate_next_value, NULL);
  }
  return Py_FindMethod(BetaDeviate_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static int			/* on "instance.attr = obj */
BetaDeviate_setattr (BetaDeviate_object *self, char *name, PyObject *obj)
{
  if (!obj) {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;

  } else if (strcmp(name, "rng") == 0) {
    PyObject *cobj = PyObject_GetAttrString(obj, _crng_basic_next_value);
    if (cobj && PyCObject_Check(cobj)) {
      Py_DECREF(self->agamma->rng);
      Py_DECREF(self->bgamma->rng);
      self->agamma->rng = obj;
      Py_INCREF(self->agamma->rng);
      self->agamma->next_value =
	(double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
      self->bgamma->rng = obj;
      Py_INCREF(self->bgamma->rng);
      self->bgamma->next_value =
	(double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
    } else {
      Py_XDECREF(cobj);
      PyErr_SetString(PyExc_TypeError, _crng_basic_error);
      return -1;
    }

  } else if (strcmp(name, "a") == 0) {
    double a;
    if (PyFloat_Check(obj)) {
      a = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      a = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString (PyExc_TypeError, _a_float_error);
      return -1;
    }
    if (a <= 0.0) {
      PyErr_SetString(PyExc_ValueError, _a_pos_error);
      return -1;
    }
    self->agamma->order = a;
    self->agamma->lorder = (long) floor(a);
    self->agamma->fraction = a - (double) self->agamma->lorder;
    if (self->agamma->fraction != 0.0) {
      self->agamma->direct = 0;
    } else {
      self->agamma->direct = (a < 12.0);
    }

  } else if (strcmp(name, "b") == 0) {
    double b;
    if (PyFloat_Check(obj)) {
      b = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      b = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _b_float_error);
      return -1;
    }
    if (b <= 0.0) {
      PyErr_SetString(PyExc_ValueError, _b_pos_error);
      return -1;
    }
    self->bgamma->order = b;
    self->bgamma->lorder = (long) floor(b);
    self->bgamma->fraction = b - (double) self->bgamma->lorder;
    if (self->bgamma->fraction != 0.0) {
      self->bgamma->direct = 0;
    } else {
      self->bgamma->direct = (b < 12.0);
    }

  } else {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;
  }

  return 0;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
BetaDeviate_repr (BetaDeviate_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.BetaDeviate(a=");
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->agamma->order);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", b="));
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->bgamma->order);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", rng="));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyObject_Repr(self->agamma->rng));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
BetaDeviate_str (BetaDeviate_object *self)
{
  char str[64];
  sprintf(str, "<crng.BetaDeviate object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = BetaDeviate()" */
BetaDeviate_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"rng", "a", "b", NULL};
  BetaDeviate_object *new;
  PyObject *rng = NULL, *cobj = NULL;
  double a = 1.0, b = 1.0;

  new = PyObject_NEW(BetaDeviate_object, &BetaDeviate_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new BetaDeviate %p\n", (void *) new);
#endif

  new->agamma = NULL;
  new->bgamma = NULL;
  new->agamma = PyObject_NEW(GammaDeviate_object, &GammaDeviate_type);
  if (new->agamma == NULL) goto error;
#ifdef DEBUG
  fprintf(stderr, "*** new GammaDeviate %p\n", (void *) new->agamma);
#endif
  new->agamma->rng = NULL;
  new->bgamma = PyObject_NEW(GammaDeviate_object, &GammaDeviate_type);
  if (new->bgamma == NULL) goto error;
#ifdef DEBUG
  fprintf(stderr, "*** new GammaDeviate %p\n", (void *) new->bgamma);
#endif
  new->bgamma->rng = NULL;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|Odd", argnames,
				    &(rng), &(a), &(b)))
    goto error;

  if (rng == NULL) {
    rng = new_default_rng();
    if (rng == NULL) goto error;
  } else {
    Py_INCREF(rng);
  }

  cobj = PyObject_GetAttrString(rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    new->agamma->rng = rng;
    Py_INCREF(new->agamma->rng);
    new->agamma->next_value =
      (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
    new->bgamma->rng = rng;
    Py_INCREF(new->bgamma->rng);
    new->bgamma->next_value =
      (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if (a <= 0.0) {
    PyErr_SetString(PyExc_ValueError, _a_pos_error);
    goto error;
  }
  new->agamma->order = a;
  new->agamma->lorder = (long) floor(a);
  new->agamma->fraction = a - (double) new->agamma->lorder;
  if (new->agamma->fraction != 0.0) {
    new->agamma->direct = 0;
  } else {
    new->agamma->direct = (a < 12.0);
  }
  new->agamma->scale = 1.0;

  if (b <= 0.0) {
    PyErr_SetString(PyExc_ValueError, _b_pos_error);
    goto error;
  }
  new->bgamma->order = b;
  new->bgamma->lorder = (long) floor(b);
  new->bgamma->fraction = b - (double) new->bgamma->lorder;
  if (new->bgamma->fraction != 0.0) {
    new->bgamma->direct = 0;
  } else {
    new->bgamma->direct = (b < 12.0);
  }
  new->bgamma->scale = 1.0;

  Py_DECREF(rng);
  return (PyObject *) new;

error:
  Py_XDECREF(cobj);
  Py_XDECREF(rng);
  Py_XDECREF(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject BetaDeviate_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "BetaDeviate",		/* tp_name */
  sizeof(BetaDeviate_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  BetaDeviate_dealloc, /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) BetaDeviate_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) BetaDeviate_setattr, /* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    BetaDeviate_repr, /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  BetaDeviate_next, /* tp_call    "x()"     */
  (reprfunc)     BetaDeviate_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  BetaDeviate_doc		/* tp_doc */
};


/*****************************************************************************
 * PoissonDeviate
 *
 * Poisson deviate with a given mean.
 *****************************************************************************/

typedef struct {
  PyObject_HEAD
  PyObject *rng;
  double (*next_value) (PyObject *rng);
  double mean;
  long direct;
  double g, sq, log_mean;
} PoissonDeviate_object;

staticforward PyTypeObject PoissonDeviate_type;

static char PoissonDeviate_doc[] =
"Poisson deviate with a given mean.\n\
\n\
    Object creation\n\
PoissonDeviate(rng=ParkMiller(), mean=1.0, direct=(mean<12.0))\n\
  rng     -- basic RNG; must be a crng type\n\
  mean    -- deviate mean, must be non-negative\n\
  direct  -- direct method of calculation, otherwise rejection method; boolean\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as an integer or tuple of integers\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
rng     -- basic RNG; must be a crng type\n\
mean    -- deviate mean, must be non-negative\n\
direct  -- direct method of calculation, otherwise rejection method; boolean";


/*------------------------------------------------------------*/
static void			/* compute and store some factors */
PoissonDeviate_precompute (PoissonDeviate_object *self)
{
  self->log_mean = log(self->mean);
  if (self->direct) {
    self->g = exp(-self->mean);
  } else {
    self->sq = sqrt(2.0 * self->mean);
    self->g = self->mean * self->log_mean - log_gamma(self->mean + 1.0);
  }
}


/*------------------------------------------------------------*/
static long			/* actually compute the next value */
PoissonDeviate_next_value (PoissonDeviate_object *self)
{
  /*
  register double (*next_value) (PyObject *) = self->next_value;
  register PyObject *rng = self->rng;
  */
  if (self->direct) {		/* direct method */
    register long em = -1;
    register double t = 1.0;
    register double g = self->g;
    do {
      ++em;
      t *= self->next_value(self->rng);
    } while (t > g);
    return em;

  } else {			/* rejection method */
    register double em, y, t;
    register double sq = self->sq;
    register double mean = self->mean;
    do {
      do {
	y = tan(M_PI * self->next_value(self->rng));
	em = sq * y + mean;
      } while (em < 0.0);
      em = floor(em);
      t = 0.9 * (1.0 + y * y) *
	exp(em * self->log_mean - log_gamma(em + 1.0) - self->g);
    } while (self->next_value(self->rng) > t);
    return (long) em;
  }
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
PoissonDeviate_next (PoissonDeviate_object *self, PyObject *args)
{
  return next_long((PyObject *) self, args,
		   (long (*) (PyObject *)) PoissonDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
PoissonDeviate_compute (PoissonDeviate_object *self, PyObject *args)
{
  return compute_long((PyObject *) self, args,
		      (long (*) (PyObject *)) PoissonDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.density()" */
PoissonDeviate_density (PoissonDeviate_object *self, PyObject *args)
{
  double x;

  if (! PyArg_ParseTuple(args, "d", &x)) return NULL;

  if (x < 0.0) {
    return PyFloat_FromDouble(0.0);
  } else {
    register double ix = floor(x);
    return PyFloat_FromDouble(exp(self->log_mean * ix -
				  self->mean - log_gamma(ix + 1.0)));
  }
}


/*------------------------------------------------------------*/
static struct PyMethodDef PoissonDeviate_methods[] = {/* order optimized */
  {"next", (PyCFunction) PoissonDeviate_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) PoissonDeviate_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) PoissonDeviate_density, METH_VARARGS, _density_doc},
  {NULL, NULL, 0, NULL}
};


/*------------------------------------------------------------*/
static void			/* instance destruction */
PoissonDeviate_dealloc (PoissonDeviate_object *self)
{
#ifdef DEBUG
  fprintf(stderr, "*** PoissonDeviate_dealloc %p\n", (void *) self);
#endif
  Py_XDECREF(self->rng);
  PyMem_Free(self);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
PoissonDeviate_getattr (PoissonDeviate_object *self, char *name)
{
  if (strcmp(name, "rng") == 0) {
    Py_INCREF(self->rng);
    return self->rng;
  } else if (strcmp(name, "mean") == 0) {
    return PyFloat_FromDouble(self->mean);
  } else if (strcmp(name, "direct") == 0) {
    return PyInt_FromLong(self->direct);
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[sss]", "rng", "mean", "direct");
  } else if (strcmp(name, _crng_next_value) == 0) {
    return PyCObject_FromVoidPtr((void *) PoissonDeviate_next_value, NULL);
  }
  return Py_FindMethod(PoissonDeviate_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static int			/* on "instance.attr = obj */
PoissonDeviate_setattr (PoissonDeviate_object *self, char *name, PyObject *obj)
{
  if (!obj) {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;

  } else if (strcmp(name, "rng") == 0) {
    PyObject *cobj = PyObject_GetAttrString(obj, _crng_basic_next_value);
    if (cobj && PyCObject_Check(cobj)) {
      Py_DECREF(self->rng);
      self->rng = obj;
      Py_INCREF(self->rng);
      self->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
    } else {
      Py_XDECREF(cobj);
      PyErr_SetString(PyExc_TypeError, _crng_basic_error);
      return -1;
    }

  } else if (strcmp(name, "mean") == 0) {
    double mean;
    if (PyFloat_Check(obj)) {
      mean = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      mean = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _mean_float_error);
      return -1;
    }
    if (mean < 0.0) {
      PyErr_SetString(PyExc_ValueError, _mean_nonneg_error);
      return -1;
    }

    self->mean = mean;
    PoissonDeviate_precompute(self);

  } else if (strcmp(name, "direct") == 0) {
    if (PyInt_Check(obj)) {
      self->direct = (PyInt_AsLong(obj) != 0);
    } else {
      PyErr_SetString(PyExc_TypeError, _direct_int_error);
      return -1;
    }

  } else {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;
  }

  return 0;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
PoissonDeviate_repr (PoissonDeviate_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.PoissonDeviate(mean=");
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->mean);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", direct="));
  if (!str) return NULL;
  item = PyInt_FromLong(self->direct);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", rng="));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyObject_Repr(self->rng));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
PoissonDeviate_str (PoissonDeviate_object *self)
{
  char str[64];
  sprintf(str, "<crng.PoissonDeviate object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = PoissonDeviate()" */
PoissonDeviate_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"rng", "mean", "direct", NULL};
  PoissonDeviate_object *new;
  PyObject *cobj = NULL;

  new = PyObject_NEW(PoissonDeviate_object, &PoissonDeviate_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new PoissonDeviate %p\n", (void *) new);
#endif

  new->rng = NULL;
  new->mean = 1.0;
  new->direct = DEFAULT_SEED;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|Odl", argnames,
				    &(new->rng), &(new->mean),
				    &(new->direct)))
    goto error;

  if (new->rng == NULL) {
    new->rng = new_default_rng();
    if (new->rng == NULL) goto error;
  } else {
    Py_INCREF(new->rng);
  }

  cobj = PyObject_GetAttrString(new->rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    new->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if (new->mean < 0.0) {
    PyErr_SetString(PyExc_ValueError, _mean_nonneg_error);
    goto error;
  }

  if (new->direct == DEFAULT_SEED) {
    new->direct = (new->mean < 12.0);
  } else {
    new->direct = (new->direct != 0);
  }

  PoissonDeviate_precompute(new);

  return (PyObject *) new;

error:
  Py_XDECREF(cobj);
  Py_XDECREF(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject PoissonDeviate_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "PoissonDeviate",		/* tp_name */
  sizeof(PoissonDeviate_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  PoissonDeviate_dealloc, /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) PoissonDeviate_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) PoissonDeviate_setattr, /* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    PoissonDeviate_repr, /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  PoissonDeviate_next, /* tp_call    "x()"     */
  (reprfunc)     PoissonDeviate_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  PoissonDeviate_doc		/* tp_doc */
};


/*****************************************************************************
 * BinomialDeviate
 *
 * Binomial deviate for a given probability and number.
 *****************************************************************************/

typedef struct {
  PyObject_HEAD
  PyObject *rng;
  double (*next_value) (PyObject *rng);
  double p;
  long n;
} BinomialDeviate_object;

staticforward PyTypeObject BinomialDeviate_type;

static char BinomialDeviate_doc[] =
"Binomial deviate for a given probability and number.\n\
\n\
    Object creation\n\
BinomialDeviate(rng=ParkMiller(), p=0.5, n=2)\n\
  rng  -- basic RNG; must be a crng type\n\
  p    -- probability of the event, such that 0.0<p<1.0\n\
  n    -- number of trials; must be positive\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as an integer or tuple of integers\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
rng  -- basic RNG; must be a crng type\n\
p    -- probability of the event, such that 0.0<p<1.0\n\
n    -- number of trials; must be positive";


/*------------------------------------------------------------*/
static long			/* actually compute the next value */
BinomialDeviate_next_value (BinomialDeviate_object *self)
{
  register unsigned long i, a, b, k = 0, n = self->n;
  register double x, x1, x2, p = self->p;

  while (n > 20) {		/* tunable parameter; make member? */
    a = 1 + (n / 2);
    b = 1 + n - a;

    if (a < 12) {		/* more efficient than using BetaDeviate */
      x1 = GammaDeviate_direct(self->next_value, self->rng, a);
    } else {
      x1 = GammaDeviate_rejection(self->next_value, self->rng, (double) a);
    }
    if (b < 12) {
      x2 = GammaDeviate_direct(self->next_value, self->rng, b);
    } else {
      x2 = GammaDeviate_rejection(self->next_value, self->rng, (double) b);
    }
    x = x1 / (x1 + x2);

    if (x >= p) {
      n = a - 1;
      p /= x;
    } else {
      k += a;
      n = b - 1;
      p = (p - x) / (1.0 - x);
    }
  }

  for (i = 0; i < n; i++) {
    x = self->next_value(self->rng);
    if (x < p) k++;
  }

  return k;
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
BinomialDeviate_next (BinomialDeviate_object *self, PyObject *args)
{
  return next_long((PyObject *) self, args,
		   (long (*) (PyObject *)) BinomialDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
BinomialDeviate_compute (BinomialDeviate_object *self, PyObject *args)
{
  return compute_long((PyObject *) self, args,
		      (long (*) (PyObject *)) BinomialDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.density()" */
BinomialDeviate_density (BinomialDeviate_object *self, PyObject *args)
{
  long k;

  if (! PyArg_ParseTuple(args, "l", &k)) return NULL;

  if ((k < 0) || (k > self->n)) {
    return PyFloat_FromDouble(0.0);
  } else {
    register double prod = 1.0;
    if (k == 0) {
      prod = 1.0;
    } else {
      register long m;
      for (m = self->n; m >= k + 1; m--) {
	prod *= (double) m / (double) (m - k);
      }
    }
    return PyFloat_FromDouble(prod * pow(self->p, (double) k) *
			      pow(1.0 - self->p, (double) (self->n - k)));
  }
}


/*------------------------------------------------------------*/
static struct PyMethodDef BinomialDeviate_methods[] = {/* order optimized */
  {"next", (PyCFunction) BinomialDeviate_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) BinomialDeviate_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) BinomialDeviate_density, METH_VARARGS, _density_doc},
  {NULL, NULL, 0, NULL}
};


/*------------------------------------------------------------*/
static void			/* instance destruction */
BinomialDeviate_dealloc (BinomialDeviate_object *self)
{
#ifdef DEBUG
  fprintf(stderr, "*** BinomialDeviate_dealloc %p\n", (void *) self);
#endif
  Py_XDECREF(self->rng);
  PyMem_Free(self);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
BinomialDeviate_getattr (BinomialDeviate_object *self, char *name)
{
  if (strcmp(name, "rng") == 0) {
    Py_INCREF(self->rng);
    return self->rng;
  } else if (strcmp(name, "p") == 0) {
    return PyFloat_FromDouble(self->p);
  } else if (strcmp(name, "n") == 0) {
    return PyInt_FromLong(self->n);
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[sss]", "rng", "p", "n");
  } else if (strcmp(name, _crng_next_value) == 0) {
    return PyCObject_FromVoidPtr((void *) BinomialDeviate_next_value, NULL);
  }
  return Py_FindMethod(BinomialDeviate_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static int			/* on "instance.attr = obj */
BinomialDeviate_setattr (BinomialDeviate_object *self,
			 char *name, PyObject *obj)
{
  if (!obj) {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;

  } else if (strcmp(name, "rng") == 0) {
    PyObject *cobj = PyObject_GetAttrString(obj, _crng_basic_next_value);
    if (cobj && PyCObject_Check(cobj)) {
      Py_DECREF(self->rng);
      self->rng = obj;
      Py_INCREF(self->rng);
      self->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
    } else {
      Py_XDECREF(cobj);
      PyErr_SetString(PyExc_TypeError, _crng_basic_error);
      return -1;
    }

  } else if (strcmp(name, "p") == 0) {
    double p;
    if (PyFloat_Check(obj)) {
      p = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      p = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _p_float_error);
      return -1;
    }
    if ((p < 0.0) || (p > 1.0)){
      PyErr_SetString(PyExc_ValueError, _p_01_error);
      return -1;
    }
    self->p = p;

  } else if (strcmp(name, "n") == 0) {
    long n;
    if (PyInt_Check(obj)) {
      n = (int) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _n_int_error);
      return -1;
    }
    if (n <= 0) {
      PyErr_SetString(PyExc_ValueError, _n_pos_error);
      return -1;
    }
    self->n = n;

  } else {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;
  }

  return 0;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
BinomialDeviate_repr (BinomialDeviate_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.BinomialDeviate(p=");
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->p);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", n="));
  if (!str) return NULL;
  item = PyInt_FromLong(self->n);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", rng="));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyObject_Repr(self->rng));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
BinomialDeviate_str (BinomialDeviate_object *self)
{
  char str[64];
  sprintf(str, "<crng.BinomialDeviate object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = BinomialDeviate()" */
BinomialDeviate_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"rng", "p", "n", NULL};
  BinomialDeviate_object *new;
  PyObject *cobj = NULL;

  new = PyObject_NEW(BinomialDeviate_object, &BinomialDeviate_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new BinomialDeviate %p\n", (void *) new);
#endif

  new->rng = NULL;
  new->p = 0.5;
  new->n = 2;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|Odl", argnames,
				    &(new->rng), &(new->p), &(new->n)))
    goto error;

  if (new->rng == NULL) {
    new->rng = new_default_rng();
    if (new->rng == NULL) goto error;
  } else {
    Py_INCREF(new->rng);
  }

  cobj = PyObject_GetAttrString(new->rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    new->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if ((new->p < 0.0) || (new->p > 1.0)){
    PyErr_SetString(PyExc_ValueError, _p_01_error);
    goto error;
  }

  if (new->n <= 0) {
    PyErr_SetString(PyExc_ValueError, _n_pos_error);
    goto error;
  }

  return (PyObject *) new;

error:
  Py_XDECREF(cobj);
  Py_XDECREF(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject BinomialDeviate_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "BinomialDeviate",		/* tp_name */
  sizeof(BinomialDeviate_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  BinomialDeviate_dealloc, /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) BinomialDeviate_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) BinomialDeviate_setattr, /* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    BinomialDeviate_repr, /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  BinomialDeviate_next, /* tp_call    "x()"     */
  (reprfunc)     BinomialDeviate_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  BinomialDeviate_doc		/* tp_doc */
};


/*****************************************************************************
 * GeometricDeviate
 *
 * Geometric deviate for a given probability.
 *****************************************************************************/

typedef struct {
  PyObject_HEAD
  PyObject *rng;
  double (*next_value) (PyObject *rng);
  double p;
} GeometricDeviate_object;

staticforward PyTypeObject GeometricDeviate_type;

static char GeometricDeviate_doc[] =
"Geometric deviate for a given probability.\n\
\n\
    Object creation\n\
GeometricDeviate(rng=ParkMiller(), p=0.5)\n\
  rng  -- basic RNG; must be a crng type\n\
  p    -- probability of the event, in the interval [0,1)\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as an integer or tuple of integers\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(k)    -- density of the deviate at k (integer)\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
rng  -- basic RNG; must be a crng type\n\
p    -- probability of the event, in the interval [0,1)";

static char _gdev_density_doc[] =
"density(k)  -- density of deviate at k (integer)";


/*------------------------------------------------------------*/
static long			/* actually compute the next value */
GeometricDeviate_next_value (GeometricDeviate_object *self)
{
  register double p = self->p;

  if (p == 1.0) {
    return 1;
  } else {
    return (long) (log(self->next_value(self->rng)) / log(1.0 - p)) + 1L;
  }
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
GeometricDeviate_next (GeometricDeviate_object *self, PyObject *args)
{
  return next_long((PyObject *) self, args,
		   (long (*) (PyObject *)) GeometricDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
GeometricDeviate_compute (GeometricDeviate_object *self, PyObject *args)
{
  return compute_long((PyObject *) self, args,
		      (long (*) (PyObject *)) GeometricDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.density()" */
GeometricDeviate_density (GeometricDeviate_object *self, PyObject *args)
{
  long k;
  register double p = self->p;

  if (! PyArg_ParseTuple(args, "l", &k)) return NULL;

  if (k <= 0) {
    return PyFloat_FromDouble(0.0);
  } else if (k == 1) {
    return PyFloat_FromDouble(p);
  } else {
    return PyFloat_FromDouble(p * pow(1 - p, (double) (k - 1)));
  }
}


/*------------------------------------------------------------*/
static struct PyMethodDef GeometricDeviate_methods[] = {/* order optimized */
  {"next", (PyCFunction) GeometricDeviate_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) GeometricDeviate_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) GeometricDeviate_density, METH_VARARGS, _gdev_density_doc},
  {NULL, NULL, 0, NULL}
};


/*------------------------------------------------------------*/
static void			/* instance destruction */
GeometricDeviate_dealloc (GeometricDeviate_object *self)
{
#ifdef DEBUG
  fprintf(stderr, "*** GeometricDeviate_dealloc %p\n", (void *) self);
#endif
  Py_XDECREF(self->rng);
  PyMem_Free(self);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
GeometricDeviate_getattr (GeometricDeviate_object *self, char *name)
{
  if (strcmp(name, "rng") == 0) {
    Py_INCREF(self->rng);
    return self->rng;
  } else if (strcmp(name, "p") == 0) {
    return PyFloat_FromDouble(self->p);
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[ss]", "rng", "p");
  } else if (strcmp(name, _crng_next_value) == 0) {
    return PyCObject_FromVoidPtr((void *) GeometricDeviate_next_value, NULL);
  }
  return Py_FindMethod(GeometricDeviate_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static int			/* on "instance.attr = obj */
GeometricDeviate_setattr (GeometricDeviate_object *self, char *name, PyObject *obj)
{
  if (!obj) {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;

  } else if (strcmp(name, "rng") == 0) {
    PyObject *cobj = PyObject_GetAttrString(obj, _crng_basic_next_value);
    if (cobj && PyCObject_Check(cobj)) {
      Py_DECREF(self->rng);
      self->rng = obj;
      Py_INCREF(self->rng);
      self->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
    } else {
      PyErr_SetString(PyExc_TypeError, _crng_basic_error);
      Py_XDECREF(cobj);
      return -1;
    }

  } else if (strcmp(name, "p") == 0) {
    double p;
    if (PyFloat_Check(obj)) {
      p = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      p = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _p_float_error);
      return -1;
    }
    if ((p <= 0.0) || (p > 1.0)) {
      PyErr_SetString(PyExc_ValueError, _p_open_01_error);
      return -1;
    }
    self->p = p;

  } else {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;
  }

  return 0;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
GeometricDeviate_repr (GeometricDeviate_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.GeometricDeviate(p=");
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->p);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", rng="));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyObject_Repr(self->rng));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
GeometricDeviate_str (GeometricDeviate_object *self)
{
  char str[64];
  sprintf(str, "<crng.GeometricDeviate object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = GeometricDeviate()" */
GeometricDeviate_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"rng", "p", NULL};
  GeometricDeviate_object *new;
  PyObject *cobj = NULL;

  new = PyObject_NEW(GeometricDeviate_object, &GeometricDeviate_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new GeometricDeviate %p\n", (void *) new);
#endif

  new->rng = NULL;
  new->p = 0.5;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|Od", argnames,
				    &(new->rng), &(new->p)))
    goto error;

  if (new->rng == NULL) {
    new->rng = new_default_rng();
    if (new->rng == NULL) goto error;
  } else {
    Py_INCREF(new->rng);
  }

  cobj = PyObject_GetAttrString(new->rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    new->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if ((new->p <= 0.0) || (new->p > 1.0)) {
    PyErr_SetString(PyExc_ValueError, _p_open_01_error);
    goto error;
  }

  return (PyObject *) new;

error:
  Py_XDECREF(cobj);
  Py_XDECREF(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject GeometricDeviate_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "GeometricDeviate",		/* tp_name */
  sizeof(GeometricDeviate_object),	/* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  GeometricDeviate_dealloc, /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) GeometricDeviate_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) GeometricDeviate_setattr, /* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    GeometricDeviate_repr, /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  GeometricDeviate_next, /* tp_call    "x()"     */
  (reprfunc)     GeometricDeviate_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  GeometricDeviate_doc		/* tp_doc */
};


/*****************************************************************************
 * BernoulliDeviate
 *
 * Bernoulli deviate for a given probability.
 *****************************************************************************/

typedef struct {
  PyObject_HEAD
  PyObject *rng;
  double (*next_value) (PyObject *rng);
  double p;
} BernoulliDeviate_object;

staticforward PyTypeObject BernoulliDeviate_type;

static char BernoulliDeviate_doc[] =
"Bernoulli deviate for a given probability.\n\
\n\
    Object creation\n\
BernoulliDeviate(rng=ParkMiller(), p=0.5)\n\
  rng  -- basic RNG; must be a crng type\n\
  p    -- probability for obtaining the value 1 (otherwise 0)\n\
\n\
    Methods\n\
next(n=1)     -- return the next n value(s), as an integer or tuple of integers\n\
compute(n=1)  -- compute the next n value(s); None returned\n\
density(x)    -- density of the deviate at x\n\
\n\
    Instance object call\n\
obj(n)  -- equivalent to 'obj.next(n)'\n\
\n\
    Members\n\
rng  -- basic RNG; must be a crng type\n\
p    -- probability for obtaining the value 1 (otherwise 0)";


/*------------------------------------------------------------*/
static long			/* actually compute the next value */
BernoulliDeviate_next_value (BernoulliDeviate_object *self)
{
  return (self->next_value(self->rng) < self->p);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.next()" and "instance()" */
BernoulliDeviate_next (BernoulliDeviate_object *self, PyObject *args)
{
  return next_long((PyObject *) self, args,
		   (long (*) (PyObject *)) BernoulliDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.compute()" */
BernoulliDeviate_compute (BernoulliDeviate_object *self, PyObject *args)
{
  return compute_long((PyObject *) self, args,
		      (long (*) (PyObject *)) BernoulliDeviate_next_value);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.density()" */
BernoulliDeviate_density (BernoulliDeviate_object *self, PyObject *args)
{
  double x;

  if (! PyArg_ParseTuple(args, "d", &x)) return NULL;

  if (x == 0.0) {
    return PyFloat_FromDouble(1.0 - self->p);
  } else if (x == 1.0) {
    return PyFloat_FromDouble(self->p);
  } else {
    return PyFloat_FromDouble(0.0);
  }
}


/*------------------------------------------------------------*/
static struct PyMethodDef BernoulliDeviate_methods[] = {/* order optimized */
  {"next", (PyCFunction) BernoulliDeviate_next, METH_VARARGS, _next_doc},
  {"compute", (PyCFunction) BernoulliDeviate_compute, METH_VARARGS, _compute_doc},
  {"density", (PyCFunction) BernoulliDeviate_density, METH_VARARGS, _density_doc},
  {NULL, NULL, 0, NULL}
};


/*------------------------------------------------------------*/
static void			/* instance destruction */
BernoulliDeviate_dealloc (BernoulliDeviate_object *self)
{
#ifdef DEBUG
  fprintf(stderr, "*** BernoulliDeviate_dealloc %p\n", (void *) self);
#endif
  Py_XDECREF(self->rng);
  PyMem_Free(self);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "instance.attr" */
BernoulliDeviate_getattr (BernoulliDeviate_object *self, char *name)
{
  if (strcmp(name, "rng") == 0) {
    Py_INCREF(self->rng);
    return self->rng;
  } else if (strcmp(name, "p") == 0) {
    return PyFloat_FromDouble(self->p);
  } else if (strcmp(name, "__members__") == 0) {
    return Py_BuildValue("[ss]", "rng", "p");
  } else if (strcmp(name, _crng_next_value) == 0) {
    return PyCObject_FromVoidPtr((void *) BernoulliDeviate_next_value, NULL);
  }
  return Py_FindMethod(BernoulliDeviate_methods, (PyObject *) self, name);
}


/*------------------------------------------------------------*/
static int			/* on "instance.attr = obj */
BernoulliDeviate_setattr (BernoulliDeviate_object *self, char *name, PyObject *obj)
{
  if (!obj) {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;

  } else if (strcmp(name, "rng") == 0) {
    PyObject *cobj = PyObject_GetAttrString(obj, _crng_basic_next_value);
    if (cobj && PyCObject_Check(cobj)) {
      Py_DECREF(self->rng);
      self->rng = obj;
      Py_INCREF(self->rng);
      self->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
    } else {
      Py_XDECREF(cobj);
      PyErr_SetString(PyExc_TypeError, _crng_basic_error);
      return -1;
    }

  } else if (strcmp(name, "p") == 0) {
    double p;
    if (PyFloat_Check(obj)) {
      p = PyFloat_AsDouble(obj);
    } else if (PyInt_Check(obj)) {
      p = (double) PyInt_AsLong(obj);
    } else {
      PyErr_SetString(PyExc_TypeError, _p_float_error);
      return -1;
    }
    if ((p < 0.0) || (p > 1.0)) {
      PyErr_SetString(PyExc_ValueError, _p_01_error);
      return -1;
    }
    self->p = p;

  } else {
    PyErr_SetString(PyExc_TypeError, _attr_error);
    return -1;
  }

  return 0;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "repr(instance)" */
BernoulliDeviate_repr (BernoulliDeviate_object *self)
{
  PyObject *str, *item;

  str = PyString_FromString("crng.BernoulliDeviate(p=");
  if (!str) return NULL;
  item = PyFloat_FromDouble(self->p);
  if (!item) goto error;
  PyString_ConcatAndDel(&str, PyObject_Repr(item));
  Py_DECREF(item);
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(", rng="));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyObject_Repr(self->rng));
  if (!str) return NULL;
  PyString_ConcatAndDel(&str, PyString_FromString(")"));
  return str;

error:
  Py_DECREF(str);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject	*		/* on "str(instance)" */
BernoulliDeviate_str (BernoulliDeviate_object *self)
{
  char str[64];
  sprintf(str, "<crng.BernoulliDeviate object at %p>", self);
  return PyString_FromString(str);
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = BernoulliDeviate()" */
BernoulliDeviate_new (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"rng", "p", NULL};
  BernoulliDeviate_object *new;
  PyObject *cobj = NULL;

  new = PyObject_NEW(BernoulliDeviate_object, &BernoulliDeviate_type);
  if (new == NULL) return NULL;
#ifdef DEBUG
  fprintf(stderr, "*** new BernoulliDeviate %p\n", (void *) new);
#endif

  new->rng = NULL;
  new->p = 0.5;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "|Od", argnames,
				    &(new->rng), &(new->p)))
    goto error;

  if (new->rng == NULL) {
    new->rng = new_default_rng();
    if (new->rng == NULL) goto error;
  } else {
    Py_INCREF(new->rng);
  }

  cobj = PyObject_GetAttrString(new->rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    new->next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if ((new->p < 0.0) || (new->p > 1.0)) {
    PyErr_SetString(PyExc_ValueError, _p_01_error);
    goto error;
  }

  return (PyObject *) new;

error:
  Py_XDECREF(cobj);
  Py_XDECREF(new);
  return NULL;
}


/*------------------------------------------------------------*/
static PyTypeObject BernoulliDeviate_type = {
  PyObject_HEAD_INIT(NULL)	/* fix up the type slot in initcrng */
  0,				/* ob_size */
  "BernoulliDeviate",		/* tp_name */
  sizeof(BernoulliDeviate_object), /* tp_basicsize */
  0,				/* tp_itemsize */

  /* standard methods */
  (destructor)  BernoulliDeviate_dealloc, /* tp_dealloc  ref-count==0  */
  (printfunc)   0,		/* tp_print    "print x"     */
  (getattrfunc) BernoulliDeviate_getattr, /* tp_getattr  "x.attr"      */
  (setattrfunc) BernoulliDeviate_setattr, /* tp_setattr  "x.attr=v"    */
  (cmpfunc)     0,		/* tp_compare  "x > y"       */
  (reprfunc)    BernoulliDeviate_repr, /* tp_repr     `x`, print x  */

  /* type categories */
  0,				/* tp_as_number   +,-,*,/,%,&,>>,pow...*/
  0,				/* tp_as_sequence +,[i],[i:j],len, ...*/
  0,				/* tp_as_mapping  [key], len, ...*/

  /* more methods */
  (hashfunc)     0,		/* tp_hash    "dict[x]" */
  (ternaryfunc)  BernoulliDeviate_next, /* tp_call    "x()"     */
  (reprfunc)     BernoulliDeviate_str, /* tp_str     "str(x)"  */
  (getattrofunc) 0,		/* tp_getattro */
  (setattrofunc) 0,		/* tp_setattro */
  0,				/* tp_as_buffer */
  0L,				/* tp_flags */
  BernoulliDeviate_doc		/* tp_doc */
};


/*****************************************************************************
 * choose, sample, shuffle, stir, pick
 *
 * The functions Choose, Sample, Shuffle, Pick have been renamed to the
 * corresponding all lower-case identifiers. The capitalized names are
 * still available as aliases, but are deprecated; in a future version
 * they will be removed.
 *****************************************************************************/

static char choose_doc[] =
"Return a new sequence of the same type, with objects chosen without\n\
replacement, preserving the relative order.\n\
\n\
newseq = choose(seq, n, rng)\n\
  seq  -- sequence object (string, tuple, list)\n\
  n    -- number of objects to choose, 0<=n<=len(seq)\n\
  rng  -- basic RNG; must be a crng type";

static char sample_doc[] =
"Return a new sequence of the same type, with objects sampled with\n\
replacement; the sequence may contain multiple references to the same object.\n\
\n\
newseq = sample(seq, n, rng)\n\
  seq  -- sequence object (string, tuple, list)\n\
  n    -- number of objects to sample, 0<=n<=len(seq)\n\
  rng  -- basic RNG; must be a crng type";

static char shuffle_doc[] =
"Return a new sequence of the same type, with the references to the objects\n\
shuffled compared to the the original sequence.\n\
\n\
newseq = shuffle(seq, rng)\n\
  seq  -- sequence object (string, tuple, list)\n\
  rng  -- basic RNG; must be a crng type";

static char stir_doc[] =
"Shuffle the list of objects in-place. None returned.\n\
\n\
obj = stir(list, rng)\n\
  list  -- list object\n\
  rng   -- basic RNG; must be a crng type";

static char pick_doc[] =
"Pick one object out of the sequence and return a reference to it.\n\
\n\
obj = pick(seq, rng)\n\
  seq  -- sequence object (string, tuple, list)\n\
  rng  -- basic RNG; must be a crng type";

static char _sample_seq_error[] = "argument 'seq' must be a sequence object";
static char _sample_n_error[] = "argument 'n' must be 0<=n<=len(seq)";
static char _stir_list_error[] = "argument 'list' must be a list object";


/*------------------------------------------------------------*/
static PyObject *		/* on "x = choose(seq, n, rng)" */
choose (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"seq", "n", "rng", NULL};
  PyObject *seq, *rng, *cobj, *new, *item;
  double (*next_value) (PyObject *rng);
  int n, length, i, t = 0, m = 0;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "OiO", argnames,
				    &seq, &n, &rng))
    return NULL;

  if (! PySequence_Check(seq)) {
    PyErr_SetString(PyExc_TypeError, _sample_seq_error);
    return NULL;
  }

  length = PyObject_Length(seq);
  if ((n < 0) || (n > length)) {
    PyErr_SetString(PyExc_ValueError, _sample_n_error);
    return NULL;
  }

  cobj = PyObject_GetAttrString(rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if (PyString_Check(seq)) {
    char *newstr, *seqstr = PyString_AsString(seq);
    new = PyString_FromString("%&"); /* two chars needed for resize (?) */
    if (new == NULL) goto error;
    _PyString_Resize(&new, n);
    newstr = PyString_AsString(new);
    for (i = 0; i < length; i++) {
      if ((double) (length - t) * next_value(rng) < (double) (n - m)) {
	newstr[m++] = seqstr[i];
	if (m >= n) break;
      }
      t++;
    }

  } else if (PyTuple_Check(seq)) {
    new = PyTuple_New(n);
    if (new == NULL) goto error;
    for (i = 0; i < length; i++) {
      if ((double) (length - t) * next_value(rng) < (double) (n - m)) {
	item = PyTuple_GET_ITEM(seq, i);
	Py_INCREF(item);
	PyTuple_SET_ITEM(new, m++, item);
	if (m >= n) break;
      }
      t++;
    }

  } else if (PyList_Check(seq)) {
    new = PyList_New(n);
    if (new == NULL) goto error;
    for (i = 0; i < length; i++) {
      if ((double) (length - t) * next_value(rng) < (double) (n - m)) {
	item = PyList_GET_ITEM(seq, i);
 	Py_INCREF(item);
	PyList_SET_ITEM(new, m++, item);
	if (m >= n) break;
      }
      t++;
    }

  } else {
    PyErr_SetString(PyExc_TypeError, _sample_seq_error);
    goto error;
  }

  Py_DECREF(cobj);
  return new;

error:
  Py_XDECREF(cobj);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = sample(seq, n, rng)" */
sample (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"seq", "n", "rng", NULL};
  PyObject *seq, *rng, *cobj, *new, *item;
  double (*next_value) (PyObject *rng);
  int n, length, i, s;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "OiO", argnames,
				    &seq, &n, &rng))
    return NULL;

  if (! PySequence_Check(seq)) {
    PyErr_SetString(PyExc_TypeError, _sample_seq_error);
    return NULL;
  }

  length = PyObject_Length(seq);
  if ((n < 0) || (n > length)) {
    PyErr_SetString(PyExc_ValueError, _sample_n_error);
    return NULL;
  }

  cobj = PyObject_GetAttrString(rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  if (PyString_Check(seq)) {
    char *newstr, *seqstr = PyString_AsString(seq);
    new = PyString_FromString("%&"); /* two chars needed for resize (?) */
    if (new == NULL) goto error;
    _PyString_Resize(&new, n);
    newstr = PyString_AsString(new);
    for (i = 0; i < n; i++) {
      s = (int) ((double) length * next_value(rng));
      newstr[i] = seqstr[s];
    }

  } else if (PyTuple_Check(seq)) {
    new = PyTuple_New(n);
    if (new == NULL) goto error;
    for (i = 0; i < n; i++) {
      s = (int) ((double) length * next_value(rng));
      item = PyTuple_GET_ITEM(seq, s);
      Py_INCREF(item);
      PyTuple_SET_ITEM(new, i, item);
    }

  } else if (PyList_Check(seq)) {
    new = PyList_New(n);
    if (new == NULL) goto error;
    for (i = 0; i < n; i++) {
      s = (int) ((double) length * next_value(rng));
      item = PyList_GET_ITEM(seq, s);
      Py_INCREF(item);
      PyList_SET_ITEM(new, i, item);
    }

  } else {
    PyErr_SetString(PyExc_TypeError, _sample_seq_error);
    goto error;
  }

  Py_DECREF(cobj);
  return new;

error:
  Py_XDECREF(cobj);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = shuffle(seq, rng)" */
shuffle (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"seq", "rng", NULL};
  PyObject *seq, *rng, *cobj, *new, *item, *item2;
  double (*next_value) (PyObject *rng);
  int length, i, s;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "OO", argnames, &seq, &rng))
    return NULL;

  if (! PySequence_Check(seq)) {
    PyErr_SetString(PyExc_TypeError, _sample_seq_error);
    return NULL;
  }

  cobj = PyObject_GetAttrString(rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    goto error;
  }

  length = PyObject_Length(seq);

  if (PyString_Check(seq)) {
    char c, *str;
    new = PyString_FromStringAndSize(PyString_AsString(seq), length);
    if (new == NULL) goto error;
    str = PyString_AsString(new);
    for (i = length - 1; i > 0; i--) {
      s = (int) ((i + 1) * next_value(rng));
      c = str[i];
      str[i] = str[s];
      str[s] = c;
    }

  } else if (PyTuple_Check(seq)) {
    new = PyTuple_New(length);	/* PyTuple_GetSlice won't work */
    if (new == NULL) goto error; /* as required for a complete copy */
    for (i = 0; i < length; i++) { /* due to an optimization in its impl */
      item = PyTuple_GET_ITEM(seq, i);
      Py_INCREF(item);
      PyTuple_SET_ITEM(new, i, item);
    }
    for (i = length - 1; i > 0; i--) {
      s = (int) ((i + 1) * next_value(rng));
      item = PyTuple_GET_ITEM(new, i);
      item2 = PyTuple_GET_ITEM(new, s);
      PyTuple_SET_ITEM(new, i, item2);
      PyTuple_SET_ITEM(new, s, item);
    }

  } else if (PyList_Check(seq)) {
    new = PyList_GetSlice(seq, 0, length);
    if (new == NULL) goto error;
    for (i = 0; i < length; i++) {
      s = (int) ((i + 1) * next_value(rng));
      item = PyList_GET_ITEM(new, i);
      item2 = PyList_GET_ITEM(new, s);
      PyList_SET_ITEM(new, i, item2);
      PyList_SET_ITEM(new, s, item);
    }

  } else {
    PyErr_SetString(PyExc_TypeError, _sample_seq_error);
    goto error;
  }

  Py_DECREF(cobj);
  return new;

error:
  Py_XDECREF(cobj);
  return NULL;
}


/*------------------------------------------------------------*/
static PyObject *		/* on "stir(list, rng)" */
stir (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"list", "rng", NULL};
  PyObject *list, *rng, *cobj, *item, *item2;
  double (*next_value) (PyObject *rng);
  int length, i, s;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "OO", argnames, &list, &rng))
    return NULL;

  if (! PyList_Check(list)) {
    PyErr_SetString(PyExc_TypeError, _stir_list_error);
    return NULL;
  }

  cobj = PyObject_GetAttrString(rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    Py_XDECREF(cobj);
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    return NULL;
  }

  length = PyList_GET_SIZE(list);
  for (i = 0; i < length; i++) {
    s = (int) ((i + 1) * next_value(rng));
    item = PyList_GET_ITEM(list, i);
    item2 = PyList_GET_ITEM(list, s);
    PyList_SET_ITEM(list, i, item2);
    PyList_SET_ITEM(list, s, item);
  }

  Py_DECREF(cobj);
  Py_INCREF(Py_None);
  return Py_None;
}


/*------------------------------------------------------------*/
static PyObject *		/* on "x = pick(seq, rng)" */
pick (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static char *argnames[] = {"seq", "rng", NULL};
  PyObject *seq, *rng, *cobj, *new;
  double (*next_value) (PyObject *rng);
  int length;

  if (! PyArg_ParseTupleAndKeywords(args, kwargs, "OO", argnames,
				    &seq, &rng))
    return NULL;

  if (! PySequence_Check(seq)) {
    PyErr_SetString(PyExc_TypeError, _sample_seq_error);
    return NULL;
  }

  cobj = PyObject_GetAttrString(rng, _crng_basic_next_value);
  if (cobj && PyCObject_Check(cobj)) {
    next_value = (double (*)(PyObject *)) PyCObject_AsVoidPtr(cobj);
  } else {
    Py_XDECREF(cobj);
    PyErr_SetString(PyExc_TypeError, _crng_basic_error);
    return NULL;
  }

  length = PyObject_Length(seq);
  if (length == 0) {
    new = Py_None;
  } else {
    new = PySequence_GetItem(seq, (int) ((double) length * next_value(rng)));
  }

  Py_DECREF(cobj);
  Py_XINCREF(new);
  return new;
}


/*============================================================*/
static struct PyMethodDef crng_methods[] = {
    {"ParkMiller", (PyCFunction) ParkMiller_new,
     METH_VARARGS | METH_KEYWORDS, ParkMiller_doc},
    {"WichmannHill", (PyCFunction) WichmannHill_new,
     METH_VARARGS | METH_KEYWORDS, WichmannHill_doc},
    {"LEcuyer", (PyCFunction) LEcuyer_new,
     METH_VARARGS | METH_KEYWORDS, LEcuyer_doc},
    {"Ranlux", (PyCFunction) Ranlux_new,
     METH_VARARGS | METH_KEYWORDS, Ranlux_doc},
    {"Taus88", (PyCFunction) Taus88_new,
     METH_VARARGS | METH_KEYWORDS, Taus88_doc},
    {"MRG32k3a", (PyCFunction) MRG32k3a_new,
     METH_VARARGS | METH_KEYWORDS, MRG32k3a_doc},
    {"MT19937", (PyCFunction) MT19937_new,
     METH_VARARGS | METH_KEYWORDS, MT19937_doc},
    {"UniformDeviate", (PyCFunction) UniformDeviate_new,
     METH_VARARGS | METH_KEYWORDS, UniformDeviate_doc},
    {"ExponentialDeviate", (PyCFunction) ExponentialDeviate_new,
     METH_VARARGS | METH_KEYWORDS, ExponentialDeviate_doc},
    {"NormalDeviate", (PyCFunction) NormalDeviate_new,
     METH_VARARGS | METH_KEYWORDS, NormalDeviate_doc},
    {"GammaDeviate", (PyCFunction) GammaDeviate_new,
     METH_VARARGS | METH_KEYWORDS, GammaDeviate_doc},
    {"BetaDeviate", (PyCFunction) BetaDeviate_new,
     METH_VARARGS | METH_KEYWORDS, BetaDeviate_doc},
    {"PoissonDeviate", (PyCFunction) PoissonDeviate_new,
     METH_VARARGS | METH_KEYWORDS, PoissonDeviate_doc},
    {"BinomialDeviate", (PyCFunction) BinomialDeviate_new,
     METH_VARARGS | METH_KEYWORDS, BinomialDeviate_doc},
    {"GeometricDeviate", (PyCFunction) GeometricDeviate_new,
     METH_VARARGS | METH_KEYWORDS, GeometricDeviate_doc},
    {"BernoulliDeviate", (PyCFunction) BernoulliDeviate_new,
     METH_VARARGS | METH_KEYWORDS, BernoulliDeviate_doc},
    {"choose", (PyCFunction) choose,
     METH_VARARGS | METH_KEYWORDS, choose_doc},
    {"Choose", (PyCFunction) choose, /* for backwards compat; deprecated */
     METH_VARARGS | METH_KEYWORDS, choose_doc},
    {"sample", (PyCFunction) sample,
     METH_VARARGS | METH_KEYWORDS, sample_doc},
    {"Sample", (PyCFunction) sample, /* for backwards compat; deprecated */
     METH_VARARGS | METH_KEYWORDS, sample_doc},
    {"shuffle", (PyCFunction) shuffle,
     METH_VARARGS | METH_KEYWORDS, shuffle_doc},
    {"Shuffle", (PyCFunction) shuffle, /* for backwards compat; deprecated */
     METH_VARARGS | METH_KEYWORDS, shuffle_doc},
    {"stir", (PyCFunction) stir,
     METH_VARARGS | METH_KEYWORDS, stir_doc},
    {"pick", (PyCFunction) pick,
     METH_VARARGS | METH_KEYWORDS, pick_doc},
    {"Pick", (PyCFunction) pick, /* for backwards compat; deprecated */
     METH_VARARGS | METH_KEYWORDS, pick_doc},
    {NULL, NULL, 0, NULL}
};


/*============================================================*/
DL_EXPORT(void)
initcrng()			/* crng module initialization */
{
  PyObject *module, *dict, *item;

  /* Fix up the type slots of the type objects.
   * This must be done at run time, as MS Visual C++ and
   * Borland C choke if it is attempted at compile time.
   * Thanks to Paul Moore and Anton Vredegoor.
   */
  ParkMiller_type.ob_type = &PyType_Type;
  WichmannHill_type.ob_type = &PyType_Type;
  LEcuyer_type.ob_type = &PyType_Type;
  Ranlux_type.ob_type = &PyType_Type;
  Taus88_type.ob_type = &PyType_Type;
  MRG32k3a_type.ob_type = &PyType_Type;
  MT19937_type.ob_type = &PyType_Type;
  UniformDeviate_type.ob_type = &PyType_Type;
  ExponentialDeviate_type.ob_type = &PyType_Type;
  NormalDeviate_type.ob_type = &PyType_Type;
  GammaDeviate_type.ob_type = &PyType_Type;
  BetaDeviate_type.ob_type = &PyType_Type;
  PoissonDeviate_type.ob_type = &PyType_Type;
  BinomialDeviate_type.ob_type = &PyType_Type;
  GeometricDeviate_type.ob_type = &PyType_Type;
  BernoulliDeviate_type.ob_type = &PyType_Type;

  module = Py_InitModule("crng", crng_methods);
  dict = PyModule_GetDict(module);

  if (!(item = PyString_FromString(_doc))) goto fatal;
  if (PyDict_SetItemString(dict, "__doc__", item) != 0) goto fatal;

  if (!(item = PyString_FromString(_version))) goto fatal;
  if (PyDict_SetItemString(dict, "_version", item) != 0) goto fatal;

  if (!(item = PyString_FromString(_RCS))) goto fatal;
  if (PyDict_SetItemString(dict, "_RCS", item) != 0) goto fatal;

  if (!(item = PyString_FromString(_copyright))) goto fatal;
  if (PyDict_SetItemString(dict, "_copyright", item) != 0) goto fatal;

  return;

fatal:
  Py_FatalError("cannot add to crng dictionary");
}
