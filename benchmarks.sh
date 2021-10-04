
#!/bin/bash
source run.sh

# with timing=timing_psql.csv \
# with benchmark=psql \
# with_each writes [ 0 10 ] \
# with size=1000000   \
# with sample-size=10000 \
# with pattern=random \
# with_each min-load [ 4096 524288 8388608 ] \
# iterate 20 \
# run_benchmark

# with timing=timing_psql.csv \
# with benchmark=psql \
# with_each writes [ 0 10 ] \
# with size=1000000   \
# with sample-size=10000 \
# with pattern=random \
# iterate 20 \
# run_benchmark

# 4-16GB 
# sample 

#1MB, 4MB, 16MB, 128MB

with_each implementation [ ufo normil ] \
with timing=timing_seq.csv \
with benchmark=seq \
with_each writes [ 0 10 ] \
with size=$(:= 4 GB) \
with sample-size=$(:= 1 GB) \
with pattern=random \
with_each min-load [ $(:= 1 MB) $(:= 4 MB) $(:= 16 MB) $(:= 128 MB) ] \
iterate 10 \
run_benchmark
