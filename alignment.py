#!/usr/bin/env python

import sys

def do_compare(f1, f2):
    # skip comments. assume f1 and f2 has the same number of
    # comment lines only on their heads
    while True:
        l1 = f1.readline().strip()
        l2 = f2.readline().strip()
        if l1[0] != "#" and l2[0] != "#":
            break

    # read the first lines of the data
    l1 = f1.readline().strip()
    l2 = f2.readline().strip()

    # assume two files are sorted in the order of addr (there are I guess)
    while True:
        # either line is empty, comparing done
        if l1 == "" or l2 == "":
            break

        addr1, num1 = map(int, l1.split(","))
        addr2, num2 = map(int, l2.split(","))

        #print("addr1: ", addr1)
        #print("addr2: ", addr2)
        
        if(addr1 == addr2):
            #print("addr1: ", addr1)
            #print("addr2: ", addr2)
            # print and proceed both addr
            print(addr2, ",", num1, ",", num2)
            l1 = f1.readline().strip()
            l2 = f2.readline().strip()
        elif(addr2 > addr1):
            #print("addr1: ", addr1)
            #print("addr2: ", addr2)
            # proceed addr1
            #print("# addr1 is smaller, proceed addr1")
            l1 = f1.readline().strip()
            continue
        elif(addr2 < addr1):
            #print("addr1: ", addr1)
            #print("addr2: ", addr2)
            # proceed addr2
            #print("# addr2 is smaller, proceed addr2")
            l2 = f2.readline().strip()
            continue

def main():
    if(len(sys.argv) < 3):
        print("usage: %s num_loads_to_page_with_prefecth.csv num_loads_to_page_without_prefecth.csv")
    else:
        f1 = open(sys.argv[1])
        f2 = open(sys.argv[2])
        do_compare(f1, f2)

if __name__ == "__main__":
    main()
