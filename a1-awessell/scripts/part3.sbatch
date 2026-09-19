#!/bin/bash
#SBATCH --job-name=a1_p3
#SBATCH --partition=Centaurus      # verify the exact name with: sinfo
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --exclusive
#SBATCH --mem=8G
#SBATCH --time=01:30:00
#SBATCH --output=data/a1_p3_%j.log

# Submit from the top of a1_<niner-id>/ :  mkdir -p data && sbatch scripts/part3.sbatch
set -euo pipefail
cd "$SLURM_SUBMIT_DIR"
mkdir -p data

SEED=12345

{
  echo "date:     $(date)"
  echo "node:     $(hostname)"
  echo "compiler: $(g++ --version | head -n1)"
  echo "flags:    -std=c++17 -Wall -Wextra -O2   (matmul)"
  echo "seed:     $SEED"
  echo "----- lscpu -----"
  lscpu
} > data/env_part3.txt

# Incremental build (no "make clean") so jobs for different parts can't race on bin/.
make

OUT=data/part3.csv
echo "order,M,K,N,rep,time_ns" > "$OUT"

# "M K N:reps"  -- fewer reps where a single multiply takes seconds (minimum is 10)
SHAPES=("256 256 256:30" "512 512 512:15" "1024 1024 1024:10" "512 1024 256:15")

for s in "${SHAPES[@]}"; do
  dims=${s%%:*}
  reps=${s##*:}
  read -r M K N <<< "$dims"
  echo "[part3] M=$M K=$K N=$N reps=$reps" >&2
  ./bin/Matmul "$M" "$K" "$N" "$reps" "$SEED" >> "$OUT"
done

# 1 header + 6 orders * (30+15+10+15) reps = 421
echo "done: $(wc -l < "$OUT") lines in $OUT (expected 421)" >&2
