#!/bin/bash
implementations=( ufo nyc nyc++ normil )
benchmarks=( seq fib psql bzip )
patterns=( scan random )

function bench {
    ./bench $@
}

function iterate {
    local iterations=$1; shift
    for i in $(seq 1 $iterations)
    do
        $@
    done
}

function for_all_benchmarks {
    for benchmark in ${benchmarks[@]}
    do
        $@ -b $benchmark
    done
}

function for_all_implementations {
    for implementation in ${implementations[@]}
    do
        $@ -i $implementation
    done
}

function for_all_patterns {
    for pattern in ${patterns[@]}
    do
        $@ -p $pattern
    done
}

for_all_benchmarks \
for_all_implementations \
for_all_patterns \
    iterate 1 bench -f test/test2.txt.bz2
