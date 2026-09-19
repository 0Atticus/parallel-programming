#!/bin/bash
#SBATCH --job-name=ai_part2
#SBATCH --partition=Centaurus
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --exclusive
#SBATCH --mem=16G
#SBATCH --time=01:00:00
#SBATCH --output=data/part2_%j.log

set -euo pipefail
cd "$SLURM_SUBMIT_DIR"
mkdir -p data

{
    echo "date:     $(date)"
    echo "node:     $(hostname)"
    echo "compiler: $(g++ --version | head -n1)"
    echo "flags:    -2td=c++17 -O3 -Wall -Wextra -O0/-O2/-O3"
    echo "seed:     12345"
    echo "----- lscpu -----"
    lscpu
} > data/env_part2.txt

make clean
make

SEED=12345
OUT2=data/part2.csv
echo "opt,type,n,rep,time_ns" > "$OUT2"

for opt in O0 O2 O3; do
  for type in int double; do
    for n in 1000000 10000000 100000000; do
      if   [ "$n" -le 1000000 ];  then reps=30
      elif [ "$n" -le 10000000 ]; then reps=20
      else                             reps=15
      fi
      echo "[part2] $opt $type n=$n reps=$reps" >&2
      ./bin/exclusivePrefix_$opt "$opt" "$type" "$n" "$reps" "$SEED" >> "$OUT2"
    done
  done
done

echo "done." >&2
echo "part2.csv:       $(wc -l < "$OUT2") lines" >&2