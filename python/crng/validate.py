"""crng/validate.py

crng: Random-number generators as Python extension types coded in C.

Validate the crng module by comparing the values it produces with the
values that should be produced for some specific RNGs.

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
_RCS = "$Id: validate.py,v 2.3 2002/10/23 11:19:20 per Exp $"
_copyright = 'Copyright (C) 2000 Per J. Kraulis'

import exceptions
import crng

if crng._version != _version:           # test crng._version
    raise RuntimeError, "crng module is version %s, while this script is version %s." % (crng._version, _version)


class crngError(exceptions.Exception):
    def __init__(self, args=None):
        self.args = args

def validate_parkmiller():
    """
    The ParkMiller validation checks that a particular seed value occurs for
    the 10,000'th step after having initialized at seed=1.
    This is from the Park & Miller paper of 1988.
    """
    p=crng.ParkMiller(1)
    step10000 = 1043618065
    p.compute(10000)
    if p.seed != step10000:
        raise crngError, "the ParkMiller seed should be %i, but is %i" % (step10000, p.seed)


def validate_wichmannhill():
    """
    The WichmannHill validation checks that the same results are produced
    for the crng implementation (in C) as the whrandom implementation (in
    Python) as delivered in the standard Python library. The test checks
    the first 1000 numbers from the same seeds.
    """
    import whrandom
    whrandom.seed(1,2,3)
    w=crng.WichmannHill(1,2,3)
    for i in range(1,1001):
        x = "%10.8f" % whrandom.random()
        y = "%10.8f" % w.next()
        if x != y:
            raise crngError, "WichmannHill value (iteration %i) is %s, should be %s" % (i, y, x)


ranlux_valid = { 0: { 1: "0.53981817", 2: "0.76155043", 3: "0.06029940",
                      4: "0.79600263", 5: "0.30631220", 101: "0.41538775",
                      102: "0.05330932", 103: "0.58195311",
                      104: "0.91397446", 105: "0.67034441"},
                 3: { 1: "0.53981817", 2: "0.76155043", 3: "0.06029940",
                      4: "0.79600263", 5: "0.30631220", 101: "0.43156743",
                      102: "0.03774416", 103: "0.24897110",
                      104: "0.00147784", 105: "0.90274453"},
                 4: { 1: "0.94589490", 2: "0.47347850", 3: "0.95152789",
                      4: "0.42971975", 5: "0.09127384", 101: "0.02618265",
                      102: "0.03775346", 103: "0.97274780",
                      104: "0.13302165", 105: "0.43126065"}
                 }

def validate_ranlux (luxury):
    """
    The Ranlux validation checks that the numbers for some selected seeds and
    iterations agrees with the values given in the Fortran 77 code from which
    the crng code was translated.
    """
    if luxury == 0:
        seed = 314159265
    elif luxury == 3:
        seed = 314159265
    elif luxury == 4:
        seed = 1
    else:
        raise crngError, "no validation data available for luxury level %i" % luxury

    ranlux = crng.Ranlux(luxury, seed)
    for i in range(1,106):
        value = ranlux.next()
        if ranlux_valid[luxury].has_key(i):
            cstr = ranlux_valid[luxury][i]
            fstr = "%%.%if" % (len(cstr)-2)
            vstr = fstr % value
            if vstr != cstr:
                raise crngError, "ranlux value (luxury %i, iteration %i) is %s, should be %s" % (luxury, i, vstr, cstr)


mt19937_valid = { 1: "0.66757648", 2: "0.36908387", 3: "0.72483069",
                  4: "0.68775863", 5: "0.57364694", 996: "0.44807063",
                  997: "0.06424586", 998: "0.75766097", 999: "0.40567560",
                  1000: "0.23996701"}

def validate_MT19937():
    """
    The MT19937 validation checks that the numbers from a specific
    seed agrees with the beginning and end of an iteration series given
    by Matsumoto & Nishimura for their implementation.
    """
    MT19937 = crng.MT19937(4357)
    for i in range(1,1001):
        value = MT19937.next()
        if mt19937_valid.has_key(i):
            cstr = mt19937_valid[i]
            fstr = "%%.%if" % (len(cstr)-2)
            vstr = fstr % value
            if vstr != cstr:
                raise crngError, "MT19937 value (iteration %i) is %s, should be %s" % (i, vstr, cstr)

testsuite = ((validate_parkmiller, (), "crng.ParkMiller"),
             (validate_wichmannhill, (), "crng.WichmannHill"),
             (validate_ranlux, (0,), "crng.Ranlux(0)"),
             (validate_ranlux, (3,), "crng.Ranlux(3)"),
             (validate_ranlux, (4,), "crng.Ranlux(4)"),
             (validate_MT19937, (), "crng.MT19937"))

if __name__ == '__main__':

    valid = 1
    for t in testsuite:
        try:
            apply(t[0], t[1])
            print t[2], "passed the validation test."
        except crngError, msg:
            valid = 0
            print "***", t[2], "did not pass the validation test! ***"
            print msg

    if valid:
        print "\ncrng passed all validation tests successfully.\n"
    else:
        print "\n*** crng did not pass all validation tests successfully! ***\n"
