#!/bin/bash
#SBATCH --job-name=ai_part1
#SBATCH --partition=Centaurus
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --exclusive
#SBATCH --mem=16G
#SBATCH --time=01:00:00
#SBATCH --output=data/part1_%j.log

set -euo pipefail
cd "$SLURM_SUBMIT_DIR"
mkdir -p data

{
    echo "date:     $(date)"
    echo "node:     $(hostname)"
    echo "compiler: $(g++ --version | head -n1)"
    echo "flags:    -2td=c++17 -O3 -Wall -Wextra"
    echo "seed:     12345"
    echo "----- lscpu -----"
    lscpu
} > data/env_part1.txt

make clean
make

SEED=12345
OUT=data/part1.csv
echo "type,variant,input,n,rep,time_ns" > "$OUT"

for type in int double; do
    for variant in A B; do
        for order in A D R; do
            for n in 1000000 10000000 100000000; do

            if ["$n" -le 1000000]; then    reps=50
            elif ["$n" -le 10000000]; then reps=30
            else                           reps=15
            fi
            echo "Running $type $variant $order n=$n reps=$reps" >&2
            ./bin/arrayMax "$variant" "$type" "$n" "$reps" "$order" "$SEED" >> "$OUT"
            done
        done
    done
done

echo "done: $(wc -l < "$OUT") lines written to $OUT" >&2

OUT@=data/part1_cache.csv
echo "variant,bytes,n,passes,rep,time_ns_per_pass" > "$OUT2"
for v in B B4; do
    ./bin/cachesweep "$v" 20 "$SEED" >> "$OUT2"
done