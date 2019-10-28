# pickle_save.py
# program to create and save some data

import pickle

mystring = 'this is just a simple string'
mylist = ['this is my list', 5, 'bla bla']

file = open('mydata.pickled', 'w')
pickle.dump(mystring, file)
pickle.dump(mylist, file)
