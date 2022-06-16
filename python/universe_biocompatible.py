"""Python code to compute value for the blog post
"Is the Universe a nice place for life? On an argument for God."
How large volume fraction of the known universe is biocompatible?
"""

from __future__ import print_function
import math

# All measurements in meter or cubic meter

# Thickness of Earth's biosphere
# http://www.newworldencyclopedia.org/entry/Biosphere

biosphere_thickness = 23000.0

def spherical_shell(outer_radius, inner_radius):
    "The volume of a spherical shell."
    return 4 * math.pi * (outer_radius**3 - inner_radius**3) / 3.0

# Earth's radius
# https://en.wikipedia.org/wiki/Earth

earth_radius = 6371000.0

biosphere_volume = spherical_shell(earth_radius + biosphere_thickness,
                                   earth_radius)
print('Biosphere volume:', "{:.3g}".format(biosphere_volume))

earth_volume = spherical_shell(earth_radius, 0.0)
print('Earth volume:', "{:.3g}".format(earth_volume))

earth_biocompat_fraction = biosphere_volume / earth_volume
print('Fraction of Earth volume that is biocompatible:',
      "{:.3f} %".format(100.0 * earth_biocompat_fraction))

# Number of biocompatible planets in the Milky Way galaxy, using
# a number from the Drake equation (high estimate?)
# https://en.wikipedia.org/wiki/Drake_equation

number_biocompat_in_galaxy = 40e9

# Number of galaxies in the known universe (high estimate?)
# http://www.forbes.com/sites/ethansiegel/2015/04/24/how-we-know-how-many-galaxies-are-in-the-universe-thanks-to-hubble/

number_galaxies = 200e9

# Volume of known universe
# https://en.wikipedia.org/wiki/Observable_universe

universe_volume = 4.0e80

# Fraction of universe volume that is biocompatible.
# Assuming Milky Way and Earth are representative.

universe_biocompat_fraction = \
    number_biocompat_in_galaxy * \
    number_galaxies * \
    earth_biocompat_fraction * \
    earth_volume / universe_volume


print('Fraction of Universe volume that is biocompatible:',
      "{:.2g}".format(100.0 * universe_biocompat_fraction),
      '%')
bio_fmt = "{:.38f}".format(100.0 * universe_biocompat_fraction)
print('  ', bio_fmt, '%')

nobio_fmt = '99.' + bio_fmt[2:-1].replace('0', '9') + str(10-int(bio_fmt[-1]))
print('Fraction of Universe volume that is not biocompatible:')
print('  ', nobio_fmt, '%')

# Compare with grain of sand vs Earth
# https://en.wikipedia.org/wiki/Orders_of_magnitude_(volume)

sand_volume = 6.2e-11
print('Fraction grain of sand vs Earth:' ,
      "{:.2g}".format(100.0 * sand_volume / earth_volume),
      '%')

print("Grain-to-Earth ratio compared with fraction biocompatible Universe",
      "{:.2g}".format((sand_volume / earth_volume) / universe_biocompat_fraction))
