# pickle_read.py
# program to read in the pickled data

import pickle

file = open('mydata.pickled', 'r')
mydata = pickle.load(file)
print mydata
mydata = pickle.load(file)
print mydata
