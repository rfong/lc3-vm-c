#!/usr/bin/python3
'''
decimal number to binary number
usage: `./binary.py <number>`
'''
import sys

base = 2
n = None

if (len(sys.argv) < 2):
  print("enter a number to convert")
  exit()
else:
  n = int(sys.argv[1])
print("%d -> base%d" % (n, base))

currPow = 1
while (currPow < n/base):
  currPow *= base
repr = ""
while (currPow >= 1):
  digit = int(n/currPow)
  n -= digit*currPow
  repr += str(digit)
  currPow /= base

print(repr)
