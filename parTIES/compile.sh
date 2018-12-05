#!/bin/bash

g++ -std=c++11 -w -fopenmp dynamic.cpp  -g -Wall -O3 -mtune=native -march=native -o dynamic
g++ -std=c++11 -w -fopenmp static.cpp  -g -Wall -O3 -mtune=native -march=native -o static
