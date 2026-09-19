#!/bin/bash
#SBATCH --job-name=a1_p5
#SBATCH --partition=Centaurus      # verify the exact name with: sinfo
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --exclusive
#SBATCH --mem=16G
#SBATCH --time=00:30:00
#SBATCH --output=data/a1_p5_%j.log

# Submit from the top of a1_<niner-id>/ :  mkdir -p data && sbatch scripts/part5.sbatch
# The BFS summary lines (graph size, min/median/max TEPS) go to the .log file.
set -euo pipefail
cd "$SLURM_SUBMIT_DIR"
mkdir -p data graphs

SEED=12345
NSRC=16
REPS=10

{
  echo "date:     $(date)"
  echo "node:     $(hostname)"
  echo "compiler: $(g++ --version | head -n1)"
  echo "flags:    -std=c++17 -Wall -Wextra -O3   (bfs)"
  echo "seed:     $SEED   sources: $NSRC   reps per source: $REPS"
  echo "----- lscpu -----"
  lscpu
} > data/env_part5.txt

make


OUT=data/part5.csv
FR=data/part5_frontiers.csv
echo "graph,src,rep,time_ns,edges,levels,reached" > "$OUT"
echo "graph,src,level,size" > "$FR"

for g in er rmat; do
  echo "[part5] $g" >&2
  ./bin/Bfs "$g" "graphs/$g.mtx" "$NSRC" "$REPS" "$SEED" "$FR" >> "$OUT"
done

# 1 header + 2 graphs * 16 sources * 10 reps = 321
echo "done: $(wc -l < "$OUT") lines in $OUT (expected 321)" >&2
