from numpy import *
import sys

def readFiles():
    f = open(sys.argv[1])
    lines = f.readlines()
    f.close();
    
    keys = empty(len(lines), dtype=object)
    values = empty(len(lines), dtype=object)
    
    for i in range(len(lines)):
        elems = lines[i].replace('\n', '').split("\t")
        
        keys[i] = elems[0]
        values[i] = elems[1].split(",")

    return keys, values

def main():
    keys, values = readFiles();

    for i in range(len(keys)):
        print keys[i] + " " + str(values[i])

if __name__ == "__main__":
    main()