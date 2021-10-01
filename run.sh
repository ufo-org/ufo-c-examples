#!/bin/bash
implementations=( ufo nyc nyc++ normil toronto )
benchmarks=( seq fib psql bzip mmap )
patterns=( scan random )

function run_benchmark {
    echo 'Executing: bench' $@ 1>&2
    ./bench $@
}

# with benchmark=mmap
function with {
    local param=$1
    shift
    $@ --${param}
}  

function iterate {
    local iterations=${1}
    shift
    for i in $(seq 1 ${iterations})
    do
        $@
    done
}

function for_each {
    local array="${1}s"
    local flag="${1}"
    shift
    elements=${array}[@]
    for element in ${!elements}
    do
        $@ --${flag}=${element}
    done
}

# Interpret:
$@