#!/bin/bash
#SBATCH --job-name=a1_p4
#SBATCH --partition=Centaurus      # verify the exact name with: sinfo
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --exclusive
#SBATCH --mem=16G
#SBATCH --time=04:00:00            # lower it if your partition caps wall time
#SBATCH --output=data/a1_p4_%j.log

# Submit from the top of a1_<niner-id>/ :  mkdir -p data && sbatch scripts/part4.sbatch
set -euo pipefail
cd "$SLURM_SUBMIT_DIR"
mkdir -p data

SEED=12345

{
  echo "date:     $(date)"
  echo "node:     $(hostname)"
  echo "compiler: $(g++ --version | head -n1)"
  echo "flags:    -std=c++17 -Wall -Wextra -O3   (mergesort)"
  echo "seed:     $SEED"
  echo "----- lscpu -----"
  lscpu
} > data/env_part4.txt

make

OUT=data/part4.csv
echo "variant,input,n,rep,time_ns" > "$OUT"

# variants: alloc (malloc per call), shared (one buffer), stdsort, qsort
# inputs:   R random, A ascending, D descending, E all equal
for n in 1000000 10000000 100000000; do
  if   [ "$n" -le 1000000 ];  then reps=30
  elif [ "$n" -le 10000000 ]; then reps=15
  else                             reps=10
  fi
  for input in R A D E; do
    for variant in alloc shared stdsort qsort; do
      echo "[part4] $variant $input n=$n reps=$reps" >&2
      ./bin/Mergesort "$variant" "$input" "$n" "$reps" "$SEED" >> "$OUT"
    done
  done
done

# 1 header + 16 (variant,input) combos * (30+15+10) reps = 881
echo "done: $(wc -l < "$OUT") lines in $OUT (expected 881)" >&2
