#!/usr/bin/env bash
# Scaled pass-kick training run. Meant to run INSIDE the container
# (scripts/build-in-docker.sh scaled-train, or directly on a bigger box).
#
# Auto-sizes --jobs from the core count, resumable, writes to runs/<name>/.
#
#   ./optimization/run-scaled-training.sh <run-name> [extra train_pass.py args]
#
# Resume:  ./optimization/run-scaled-training.sh <same-name> --resume
set -euo pipefail
cd "$(dirname "$0")/.."

NAME="${1:?run name, e.g. prekick-cmaes-1}"; shift || true
OUT="runs/$NAME"
mkdir -p "$OUT"

CORES="$(nproc)"
# single-agent prekick episode = 2 processes, mostly CPU-bound in sync mode.
JOBS=$(( CORES > 2 ? CORES - 1 : 1 ))

echo "run=$NAME  cores=$CORES  jobs=$JOBS  out=$OUT"
[ -x ./agentspark ] || { rm -f CMakeCache.txt; cmake . -DCMAKE_BUILD_TYPE=Release >/dev/null; make -j"$CORES" >/dev/null; }

exec python3 optimization/train_pass.py \
  --optimizer cmaes \
  --iterations 40 --pop 24 --sigma0 0.25 \
  --jobs "$JOBS" \
  --fixed-file paramfiles/pass_prekick.txt \
  --reeval 4 \
  --out-dir "$OUT" \
  "$@"
