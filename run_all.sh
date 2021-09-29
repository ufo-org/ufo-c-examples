#!/bin/bash

implementations=( ufo nyc nyc++ normil )
benchmarks=( seq fib psql bzip )
patterns=( scan random )
iterations=5

for implementation in ${implementations[@]}; do
for benchmark in ${benchmarks[@]}; do
for pattern in ${patterns[@]}; do
for iteration in `seq 1 $iterations`; do

    ./bench -i $implementation -b $benchmark -p $pattern -f test/test2.txt.bz2

done; done; done; done
