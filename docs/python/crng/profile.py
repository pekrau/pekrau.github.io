"""crng/profile.py

crng: Random-number generators as Python extension types coded in C.

Profile the crng module; only the speed of the C code implementation is tested.

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
"""

_version = '1.2'
_RCS = "$Id: profile.py,v 2.3 2002/10/23 11:29:43 per Exp $"
_copyright = 'Copyright (C) 2000 Per J. Kraulis'

import time
from crng import *

import crng                             # test crng._version
if crng._version != _version:
    raise RuntimeError, "crng module is version %s, while this script is version %s." % (crng._version, _version)


rngs = (
        'ParkMiller()',
        'WichmannHill()',
        'LEcuyer()',
        'Ranlux(0)',
        'Ranlux(1)',
        'Ranlux(2)',
        'Ranlux(3)',
        'Ranlux(4)',
        'Taus88()',
        'MRG32k3a()',
        'MT19937()',
##        'UniformDeviate(MT19937())',
##        'UniformDeviate(WichmannHill())',
##        'ExponentialDeviate(WichmannHill())',
##        'ExponentialDeviate(Ranlux(2))',
##        'ExponentialDeviate(MT19937())',
##        'NormalDeviate(WichmannHill())',
##        'NormalDeviate(MT19937())',
##        'GammaDeviate(MT19937(), order=2, direct=0)',
##        'GammaDeviate(MT19937(), order=2, direct=1)',
##        'GammaDeviate(MT19937(), order=10, direct=0)',
##        'GammaDeviate(MT19937(), order=10, direct=1)',
##        'GammaDeviate(MT19937(), order=12, direct=0)',
##        'GammaDeviate(MT19937(), order=12, direct=1)',
##        'BinomialDeviate(MT19937(), number=3)',
##        'BinomialDeviate(MT19937(), number=8)',
##        'BinomialDeviate(MT19937(), number=12)',
##        'BinomialDeviate(MT19937(), number=20)',
##        'BinomialDeviate(MT19937(), number=30)',
##        'BinomialDeviate(MT19937(), number=100)',
##        'PoissonDeviate(MT19937(), mean=1.0, direct=0)',
##        'PoissonDeviate(MT19937(), mean=1.0, direct=1)',
##        'PoissonDeviate(MT19937(), mean=8.0, direct=0)',
##        'PoissonDeviate(MT19937(), mean=8.0, direct=1)',
##        'PoissonDeviate(MT19937(), mean=12.0, direct=0)',
##        'PoissonDeviate(MT19937(), mean=12.0, direct=1)',
##        'PoissonDeviate(MT19937(), mean=16.0, direct=0)',
##        'PoissonDeviate(MT19937(), mean=16.0, direct=1)',
##        'PoissonDeviate(Ranlux(3), mean=16.0, direct=0)',
##        'PoissonDeviate(Ranlux(3), mean=16.0, direct=1)',
##        'GammaDeviate(Ranlux(3), order=6, direct=0)',
##        'GammaDeviate(Ranlux(3), order=6, direct=1)',
##        'GammaDeviate(Ranlux(3), order=12, direct=0)',
##        'GammaDeviate(Ranlux(3), order=12, direct=1)',
        )

ITERATIONS = 20.0                           # millions

for rs in rngs:
    r = eval(rs)
    t0 = time.clock()
    r.compute(ITERATIONS * 1000000)
    t1 = time.clock()
    t = t1 - t0
    print "%-20s: %6.2f 10^6 iters/sec (%6.3f)" % (rs,
                                                   ITERATIONS/t,
                                                   t/ITERATIONS)
