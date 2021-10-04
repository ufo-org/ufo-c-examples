#!/bin/bash
implementations=( ufo nyc normil toronto )
benchmarks=( seq fib psql bzip mmap )
patterns=( scan random )

function GB { 
    echo  $(( $1 * 1024 * 1024 * 1024 )) 
}
function MB { 
    echo  $(( $1 * 1024 * 1024 )) 
}
function KB { 
    echo  $(( $1 * 1024 )) 
}
function B  { 
    echo  $1 
}
function := {
    $2 $1 
}

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

function with_each {
    local param=$1
    shift
    local params=()
    shift # drop "["
    while [ $1 != "]" ]
    do
        params+="$1 "
        shift
    done
    shift # drop "]"
    for value in ${params[@]}
    do
        $@ --${param}=$value
    done
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