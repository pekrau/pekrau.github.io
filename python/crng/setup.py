#!/usr/bin/env python

##   crng: Random-number generators as Python extension types coded in C.

##   Setup script using Distutils. Thanks to Paul Moore.

from distutils.core import setup, Extension

_version = '1.2'
_RCS = "$Id: setup.py,v 2.4 2002/10/23 11:29:32 per Exp $"
_copyright = None

setup (name = "crng",
       version = _version,
       description = "Random Number Generators",
       author = "Per J. Kraulis <per@sbc.su.se>",
       url = "http://www.sbc.su.se/~per/crng/",
       py_modules = ["crng_pickle"],
       ext_modules = [ Extension("crng", ["crng.c"]) ]
       )
