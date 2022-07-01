# read_dbmfile.py
# test dbm-type database; gdbm variant

import gdbm

hashfile = gdbm.open('hashfile', 'r')   # open existing file
for key in hashfile.keys():             # loop over all keys in file
    print key, '=', hashfile[key]
