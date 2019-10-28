"""crng/display.py

crng: Random-number generators as Python extension types coded in C.

Display crng results visually in a 2D plot.

Copyright (C) 2000-2002Per J. Kraulis

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
_RCS = "$Id: display.py,v 2.4 2002/10/23 11:29:47 per Exp $"
_copyright = 'Copyright (C) 2000 Per J. Kraulis'

import math, sys, types
from Tkinter import *
from crng import *

import crng                             # test crng._version
if crng._version != _version:
    raise RuntimeError, "crng module is version %s, while this script is version %s." % (crng._version, _version)


WINDOWSIZE = 600
NUMBER = 1000
SIZE = 2
_ALL = 'all'

_inidle = type(sys.stdin) == types.InstanceType and \
	  sys.stdin.__class__.__name__ == 'PyShell'

rng = MT19937(seed=1)

DEFAULT_DEVIATE = 'UniformDeviate(0, 1)'

deviates = {DEFAULT_DEVIATE:
            (UniformDeviate(rng, 0.0, 1.0), 1.0, 0.5),
            'UniformDeviate(0.2, 0.8)':
            (UniformDeviate(rng, 0.2, 0.8), 1.0, 0.5),
            'ExponentialDeviate(1.0)':
            (ExponentialDeviate(rng, 1.0), 4.0*math.log(2.0), 1.0),
            'ExponentialDeviate(0.5)':
            (ExponentialDeviate(rng, 0.5), 4.0*math.log(2.0), 1.0),
            'ExponentialDeviate(2.0)':
            (ExponentialDeviate(rng, 2.0), 4.0*math.log(2.0), 1.0),
            'GammaDeviate(order=5)':
            (GammaDeviate(rng, order=5, scale=10.0/40.0), 10.0, 1.0),
            'GammaDeviate(order=10)':
            (GammaDeviate(rng, order=10, scale=10.0/40.0), 10.0, 1.0),
            'GammaDeviate(order=15)':
            (GammaDeviate(rng, order=15, scale=10.0/40.0), 10.0, 1.0),
            'GammaDeviate(order=20)':
            (GammaDeviate(rng, order=20, scale=10.0/40.0), 10.0, 1.0),
            'GammaDeviate(order=25)':
            (GammaDeviate(rng, order=25, scale=10.0/40.0), 10.0, 1.0),
            'NormalDeviate(mean=5.0, stdev=0.5)':
            (NormalDeviate(rng, mean=5.0, stdev=0.5), 10.0, 1.0),
            'NormalDeviate(mean=5.0, stdev=1.0)':
            (NormalDeviate(rng, mean=5.0, stdev=1.0), 10.0, 1.0),
            'NormalDeviate(mean=5.0, stdev=2.0)':
            (NormalDeviate(rng, mean=5.0, stdev=2.0), 10.0, 1.0),
            'BetaDeviate(a=1, b=16)':
            (BetaDeviate(rng, a=1, b=16), 1.0, 0.25),
            'BetaDeviate(a=1, b=8)':
            (BetaDeviate(rng, a=1, b=8), 1.0, 0.25),
            'BetaDeviate(a=1, b=4)':
            (BetaDeviate(rng, a=1, b=4), 1.0, 0.25),
            'BetaDeviate(a=1, b=2)':
            (BetaDeviate(rng, a=1, b=2), 1.0, 0.25),
            'BetaDeviate(a=1, b=1)':
            (BetaDeviate(rng, a=1, b=1), 1.0, 0.25),
            'BetaDeviate(a=2, b=2)':
            (BetaDeviate(rng, a=2, b=2), 1.0, 0.25),
            'BetaDeviate(a=2, b=4)':
            (BetaDeviate(rng, a=2, b=4), 1.0, 0.25),
            'BetaDeviate(a=2, b=8)':
            (BetaDeviate(rng, a=2, b=8), 1.0, 0.25),
            'BetaDeviate(a=3, b=3)':
            (BetaDeviate(rng, a=3, b=3), 1.0, 0.25),
            'BetaDeviate(a=4, b=4)':
            (BetaDeviate(rng, a=6, b=6), 1.0, 0.25)
            }


class Chooser:
    def __init__(self, axis):
        self.axis = axis
        self.variable = StringVar()
    def __call__(self):
        self.deviate, self.range, self.scale = deviates[self.variable.get()]
        draw()
    def set_default(self):
        self.variable.set(DEFAULT_DEVIATE)
        self.deviate, self.range, self.scale = deviates[DEFAULT_DEVIATE]


def make_menubutton(menubar, choice):
    menubutton = Menubutton(menubar, text="%s deviate" % choice.axis)
    menubutton.pack(side=LEFT, padx='2m')
    menu = Menu(menubutton)
    keys = deviates.keys()
    keys.sort()
    for key in keys:
        menu.add_radiobutton(label=key, variable=choice.variable, command=choice)
    choice.set_default()
    menubutton['menu'] = menu
    return menubutton

def draw(e=None):
    canvas.delete(_ALL)

    xdev = xchoice.deviate
    XRANGE = xchoice.range
    XSCALE = xchoice.scale
    ydev = ychoice.deviate
    YRANGE = ychoice.range
    YSCALE = ychoice.scale
    width = float(canvas.cget('width'))
    height = float(canvas.cget('height'))
    
    xcoeff = width / float(XRANGE)
    xstep = 1.0 / width
    ycoeff = height / float(YRANGE)
    ystep = 1.0 / height

    for i in xrange(NUMBER):
        x = xcoeff * xdev()
        y = height - ycoeff * ydev()
        canvas.create_rectangle(x-SIZE, y-SIZE, x+SIZE, y+SIZE, tag=_ALL)

    try:
        coords = []
        for ix in xrange(0, width):
            coords.append((ix,
                           height - XSCALE * width * xdev.density(ix / xcoeff)))
        apply(canvas.create_line, tuple(coords), {'fill': 'red', 'tag': _ALL})
    except NotImplementedError:
        pass

    try:
        coords = []
        for iy in xrange(0, height):
            coords.append((YSCALE * height * ydev.density(iy / ycoeff),
                           height - iy))
        apply(canvas.create_line, tuple(coords), {'fill': 'blue', 'tag': _ALL})
    except NotImplementedError:
        pass


if __name__ == '__main__':
    root = Tk()
    root.title('crng')
    xchoice = Chooser('x')
    ychoice = Chooser('y')
    menubar = Frame(root, relief=RAISED, borderwidth=2)
    menubar.pack(side=TOP, fill=X)
    xmenubutton = make_menubutton(menubar, xchoice)
    ymenubutton = make_menubutton(menubar, ychoice)
    menubar.tk_menuBar(xmenubutton, ymenubutton)
    canvas = Canvas(root, width=WINDOWSIZE, height=WINDOWSIZE,
                    background='white')
    canvas.pack(fill=BOTH, expand=YES)
    root.resizable(0,0)
    draw()
    if not _inidle:
            root.mainloop()
