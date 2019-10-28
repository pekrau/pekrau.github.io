# hash.py
# test hash tables (dictionaries) in Python

hashtable = {'this is a key': 'and this is the value',
             'another key':   'another value'}

print 'line 1:', hashtable['this is a key']
hashtable['third key'] = 'bla'
print 'line 2', hashtable['third key']
