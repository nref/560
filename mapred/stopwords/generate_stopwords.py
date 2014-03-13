from numpy import *
import sys

f = open(sys.argv[1])
lines = f.readlines()

counts = zeros(len(lines), dtype=int)
words = empty(len(lines), dtype=object)

for i in range(len(lines)):
    elems = lines[i].replace('\n', '').split("\t")
    
    words[i] = elems[0]
    counts[i] = int(elems[1])

avg = mean(counts)
max_idx = argmax(counts)
max = counts[max_idx]
stdev = std(counts)

# Stop words: print all words with frequency
# > 3 standard deviations above the mean
for i in range(len(counts)):
    if counts[i] > avg + 3 * stdev:
        print words[i]