#!/usr/bin/env bash
# Scaled pass-kick training run. Meant to run INSIDE the container
# (scripts/build-in-docker.sh scaled-train ...) or directly on a bigger box.
#
# Auto-sizes --jobs from the core count, resumable, writes to runs/<name>/.
#
#   ./optimization/run-scaled-training.sh <name> [fixed|contextual] [extra args]
#   ./optimization/run-scaled-training.sh <name> --resume
#
#   fixed       one target (6 m straight); searches the 12 kick-shape params.
#   contextual  random target 3-12 m, +/-40 deg each trial; also searches the
#               power->distance policy (kick_power_a/b/c). Aim is geometric.
set -euo pipefail
cd "$(dirname "$0")/.."

NAME="${1:?run name}"; shift || true
MODE="contextual"
case "${1:-}" in
  fixed|contextual) MODE="$1"; shift ;;
esac

OUT="runs/$NAME"; mkdir -p "$OUT"
CORES="$(nproc)"
JOBS=$(( CORES > 2 ? CORES - 1 : 1 ))

if [ "$MODE" = contextual ]; then
  FIXED=paramfiles/pass_contextual.txt
  PARAMS=contextual
  ITERS=70
else
  FIXED=paramfiles/pass_prekick.txt
  PARAMS=kick_p1,kick_p2,kick_p3,kick_p4,kick_p5,kick_p6,kick_p7,kick_p8,kick_p9,kick_scale1,kick_scale2,kick_scale3
  ITERS=40
fi

echo "run=$NAME  mode=$MODE  cores=$CORES  jobs=$JOBS  out=$OUT"
[ -x ./agentspark ] || { rm -f CMakeCache.txt; cmake . -DCMAKE_BUILD_TYPE=Release >/dev/null; make -j"$CORES" >/dev/null; }

exec python3 optimization/train_pass.py \
  --optimizer cmaes \
  --iterations "$ITERS" --pop 24 --sigma0 0.25 \
  --jobs "$JOBS" \
  --params "$PARAMS" \
  --fixed-file "$FIXED" \
  --reeval 4 \
  --out-dir "$OUT" \
  "$@"
