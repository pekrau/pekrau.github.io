"""crng/crng_pickle.py

crng: Random-number generators as Python extension types coded in C.

Make crng types pickleable by registering functions in copy_reg.

Importing this module will register reduction and construction functions
for all crng types. The user need not do anything else.

Copyright (C) 2000-2002 Per J. Kraulis

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (file gpl.txt) for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
"""

_version = '1.2'
_RCS = "$Id: crng_pickle.py,v 2.2 2002/10/23 11:29:37 per Exp $"
_copyright = 'Copyright (C) 2000 Per J. Kraulis'


import copy_reg
from crng import *

import crng                             # test crng._version
if crng._version != _version:
    raise RuntimeError, "crng module is version %s, while this script is version %s." % (crng._version, _version)


ParkMiller_type = type(ParkMiller())
def ParkMiller_construct(seed):
    return ParkMiller(seed)
def ParkMiller_reduce(rng):
    assert type(rng) == ParkMiller_type
    return ParkMiller_construct, (rng.seed,)
copy_reg.pickle(ParkMiller_type, ParkMiller_reduce, ParkMiller_construct)

WichmannHill_type = type(WichmannHill())
def WichmannHill_construct(seed1, seed2, seed3):
    try:
        return WichmannHill(seed1, seed2, seed3)
    except ValueError:
        print seed1, seed2, seed3
        raise
def WichmannHill_reduce(rng):
    assert type(rng) == WichmannHill_type
    return WichmannHill_construct, (rng.seed1, rng.seed2, rng.seed3)
copy_reg.pickle(WichmannHill_type, WichmannHill_reduce, WichmannHill_construct)

LEcuyer_type = type(LEcuyer())
def LEcuyer_construct(seed1, seed2):
    return LEcuyer(seed1, seed2)
def LEcuyer_reduce(rng):
    assert type(rng) == LEcuyer_type
    return LEcuyer_construct, (rng.seed1, rng.seed2)
copy_reg.pickle(LEcuyer_type, LEcuyer_reduce, LEcuyer_construct)

Ranlux_type = type(Ranlux())
def Ranlux_construct(*state):
    return Ranlux(state=state)
def Ranlux_reduce(rng):
    assert type(rng) == Ranlux_type
    return Ranlux_construct, rng.state
copy_reg.pickle(Ranlux_type, Ranlux_reduce, Ranlux_construct)

Taus88_type = type(Taus88())
def Taus88_construct(seed1, seed2, seed3):
    return Taus88(seed1, seed2, seed3)
def Taus88_reduce(rng):
    assert type(rng) == Taus88_type
    return Taus88_construct, (rng.seed1, rng.seed2, rng.seed3)
copy_reg.pickle(Taus88_type, Taus88_reduce, Taus88_construct)

MRG32k3a_type = type(MRG32k3a())
def MRG32k3a_construct(s10, s11, s12, s20, s21, s22):
    return MRG32k3a(s10, s11, s12, s20, s21, s22)
def MRG32k3a_reduce(rng):
    assert type(rng) == MRG32k3a_type
    return MRG32k3a_construct, (rng.s10, rng.s11, rng.s12,
                                rng.s20, rng.s21, rng.s22)
copy_reg.pickle(MRG32k3a_type, MRG32k3a_reduce, MRG32k3a_construct)

MT19937_type = type(MT19937())
def MT19937_construct(*state):
    return MT19937(state=state)
def MT19937_reduce(rng):
    assert type(rng) == MT19937_type
    return MT19937_construct, rng.state
copy_reg.pickle(MT19937_type, MT19937_reduce, MT19937_construct)

UniformDeviate_type = type(UniformDeviate())
def UniformDeviate_construct(rng, a, b):
    return UniformDeviate(rng, a, b)
def UniformDeviate_reduce(rng):
    assert type(rng) == UniformDeviate_type
    return UniformDeviate_construct, (rng.rng, rng.a, rng.b)
copy_reg.pickle(UniformDeviate_type,
                UniformDeviate_reduce, UniformDeviate_construct)

ExponentialDeviate_type = type(ExponentialDeviate())
def ExponentialDeviate_construct(rng, mean):
    return ExponentialDeviate(rng, mean)
def ExponentialDeviate_reduce(rng):
    assert type(rng) == ExponentialDeviate_type
    return ExponentialDeviate_construct, (rng.rng, rng.mean)
copy_reg.pickle(ExponentialDeviate_type,
                ExponentialDeviate_reduce, ExponentialDeviate_construct)

NormalDeviate_type = type(NormalDeviate())
def NormalDeviate_construct(rng, mean, stdev, stored):
    return NormalDeviate(rng, mean, stdev, stored)
def NormalDeviate_reduce(rng):
    assert type(rng) == NormalDeviate_type
    return NormalDeviate_construct, (rng.rng, rng.mean, rng.stdev, rng.stored)
copy_reg.pickle(NormalDeviate_type,
                NormalDeviate_reduce, NormalDeviate_construct)

GammaDeviate_type = type(GammaDeviate())
def GammaDeviate_construct(rng, order, scale, direct):
    return GammaDeviate(rng, order, scale, direct)
def GammaDeviate_reduce(rng):
    assert type(rng) == GammaDeviate_type
    return GammaDeviate_construct, (rng.rng, rng.order, rng.scale, rng.direct)
copy_reg.pickle(GammaDeviate_type,
                GammaDeviate_reduce, GammaDeviate_construct)

BetaDeviate_type = type(BetaDeviate())
def BetaDeviate_construct(rng, a, b):
    return BetaDeviate(rng, a, b)
def BetaDeviate_reduce(rng):
    assert type(rng) == BetaDeviate_type
    return BetaDeviate_construct, (rng.rng, rng.a, rng.b)
copy_reg.pickle(BetaDeviate_type,
                BetaDeviate_reduce, BetaDeviate_construct)

PoissonDeviate_type = type(PoissonDeviate())
def PoissonDeviate_construct(rng, mean, direct):
    return PoissonDeviate(rng, mean, direct)
def PoissonDeviate_reduce(rng):
    assert type(rng) == PoissonDeviate_type
    return PoissonDeviate_construct, (rng.rng, rng.mean, rng.direct)
copy_reg.pickle(PoissonDeviate_type,
                PoissonDeviate_reduce, PoissonDeviate_construct)

BinomialDeviate_type = type(BinomialDeviate())
def BinomialDeviate_construct(rng, p, n):
    return BinomialDeviate(rng, p, n)
def BinomialDeviate_reduce(rng):
    assert type(rng) == BinomialDeviate_type
    return BinomialDeviate_construct, (rng.rng, rng.p, rng.n)
copy_reg.pickle(BinomialDeviate_type,
                BinomialDeviate_reduce, BinomialDeviate_construct)

GeometricDeviate_type = type(GeometricDeviate())
def GeometricDeviate_construct(rng, p):
    return GeometricDeviate(rng, p)
def GeometricDeviate_reduce(rng):
    assert type(rng) == GeometricDeviate_type
    return GeometricDeviate_construct, (rng.rng, rng.p)
copy_reg.pickle(GeometricDeviate_type,
                GeometricDeviate_reduce, GeometricDeviate_construct)

BernoulliDeviate_type = type(BernoulliDeviate())
def BernoulliDeviate_construct(rng, p):
    return BernoulliDeviate(rng, p)
def BernoulliDeviate_reduce(rng):
    assert type(rng) == BernoulliDeviate_type
    return BernoulliDeviate_construct, (rng.rng, rng.p)
copy_reg.pickle(BernoulliDeviate_type,
                BernoulliDeviate_reduce, BernoulliDeviate_construct)


def test():
    "Validate pickle of crng objects."
    import StringIO
    import pickle
    import sys

    rnglist = [ParkMiller(32),
               WichmannHill(12, 67, 35),
               LEcuyer(57, 69),
               Ranlux(2, 1905),
               Taus88(109, 659, 365),
               MRG32k3a(50, 89, 10968, 23, 9506),
               MT19937(9485),
               UniformDeviate(rng=ParkMiller(45), a=-1.0, b=2.0),
               ExponentialDeviate(rng=WichmannHill(), mean=3.3),
               NormalDeviate(rng=Taus88(), mean=2.5, stdev=0.75),
               GammaDeviate(rng=LEcuyer(87,23), order=15, scale=0.5, direct=1),
               BetaDeviate(rng=MRG32k3a(), a=3.5, b=1.25),
               PoissonDeviate(rng=ParkMiller(), mean=4.5),
               BinomialDeviate(rng=MT19937(), p=0.8, n=19),
               GeometricDeviate(rng=Ranlux(luxury=4), p=0.3),
               BernoulliDeviate(rng=WichmannHill(), p=0.24)]

    file = StringIO.StringIO()
    p = pickle.Pickler(file)

    orig = []
    for rng in rnglist:
        rng.compute(1001)               # warmup
        p.dump(rng)                     # save the state
        rng.compute(500)                # move on a bit
        orig.append(rng(20))            # save output at this point

    data = file.getvalue()
    file.close()

    file = StringIO.StringIO(data)
    p = pickle.Unpickler(file)

    for val in orig:
        rng = p.load()                  # retrieve state
        rng.compute(500)                # move on same bit
        if val != rng(20):              # compare output at this point
            print 'crng_pickle test failed for', rng
            sys.exit(1)
    print 'crng_pickle test OK'


if __name__ == '__main__':
    test()
