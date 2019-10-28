# make_dbmfile.py
# test dbm-type database; gdbm variant

import gdbm

hashfile = gdbm.open('hashfile', 'c')   # create the file
hashfile['a key'] = 'a bit of data'
hashfile['protein sequence'] = 'ASWQQEDFFGLKPVCDAS'
