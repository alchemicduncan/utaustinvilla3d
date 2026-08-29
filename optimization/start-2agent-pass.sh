#!/bin/bash
#
# Run one two-agent pass episode: rcssserver3d + a passAgent (unum 2) that kicks
# and a passReceiverAgent (unum 3) that moves to the ball and scores the pass.
# The receiver writes the fitness we care about (mean over pass_num_trials).
#
#   ./optimization/start-2agent-pass.sh <body_type> <params_file> <output_file>
#
set -u

TYPE="${1:?body type}"
PARAMS_FILE="${2:?params file}"
OUTPUT_FILE="${3:?output file}"

DIR_SCRIPT="$(cd "$(dirname "$0")" && pwd)"
ROOT="$DIR_SCRIPT/.."
PARAMS_FILE="$(cd "$(dirname "$PARAMS_FILE")" && pwd)/$(basename "$PARAMS_FILE")"
OUTPUT_FILE="$(cd "$(dirname "$OUTPUT_FILE")" && pwd)/$(basename "$OUTPUT_FILE")"

SPARK="${SPARK_DIR:-/opt/simspark}/share"

configure_sim() {
  local rsg="$SPARK/rcssserver3d/rsg/agent/nao/naoneckhead.rsg"
  sed -i 's/(setSenseMyPos false)/(setSenseMyPos true)/;
          s/(setSenseMyOrien false)/(setSenseMyOrien true)/;
          s/(setSenseBallPos false)/(setSenseBallPos true)/' "$rsg"
  sed -i "s/addSoccerVar('BeamNoiseXY',[0-9.]*)/addSoccerVar('BeamNoiseXY',0.0)/;
          s/addSoccerVar('BeamNoiseAngle',[0-9.]*)/addSoccerVar('BeamNoiseAngle',0.0)/" \
          "$SPARK/rcssserver3d/naosoccersim.rb"
  sed -i 's/\$enableRealTimeMode = true/$enableRealTimeMode = false/' \
          "$SPARK/rcssserver3d/rcssserver3d.rb"
  sed -i 's/\$agentSyncMode = false/$agentSyncMode = true/' "$SPARK/simspark/spark.rb"
}
configure_sim

AGENTPORT=$(( (RANDOM % 20000) + 20000 ))
SERVERPORT=$(( AGENTPORT + 1 ))
TAG=$AGENTPORT

rm -f "$OUTPUT_FILE"

rcssserver3d --agent-port "$AGENTPORT" --server-port "$SERVERPORT" \
    >/tmp/pass2_server_$TAG.log 2>&1 &
SERVER_PID=$!
sleep 3

cd "$ROOT"
common_args=(
  --type "$TYPE"
  --paramsfile paramfiles/defaultParams.txt
  --paramsfile "paramfiles/defaultParams_t$TYPE.txt"
  --paramsfile paramfiles/pass_defaults.txt
  --paramsfile paramfiles/pass_2agent.txt
  --paramsfile "$PARAMS_FILE"
  --port "$AGENTPORT" --mport "$SERVERPORT"
)

# Passer (team Left, owns the trial FSM + monitor); its own fitness file is ignored.
./agentspark --unum 2 --team Left "${common_args[@]}" \
    --experimentout "/tmp/pass2_passer_$TAG.txt" --optimize passAgent \
    >/tmp/pass2_passer_$TAG.log 2>&1 &
PASSER_PID=$!
sleep 2

# Receiver (same team; in PlayOn there is no half restriction so (6,0) is fine).
./agentspark --unum 3 --team Left "${common_args[@]}" \
    --experimentout "$OUTPUT_FILE" --optimize passReceiverAgent \
    >/tmp/pass2_receiver_$TAG.log 2>&1 &
RECEIVER_PID=$!

MAX_WAIT=240
waited=0
while [ ! -f "$OUTPUT_FILE" ] && [ "$waited" -lt "$MAX_WAIT" ]; do
    sleep 1
    waited=$((waited + 1))
done

kill -s INT "$RECEIVER_PID" "$PASSER_PID" "$SERVER_PID" 2>/dev/null
sleep 1
kill -9 "$RECEIVER_PID" "$PASSER_PID" "$SERVER_PID" 2>/dev/null

if [ ! -f "$OUTPUT_FILE" ]; then
    echo "TIMED OUT after ${waited}s" >&2
    echo "--- passer log tail ---"   >&2; tail -15 "/tmp/pass2_passer_$TAG.log"   >&2
    echo "--- receiver log tail ---" >&2; tail -15 "/tmp/pass2_receiver_$TAG.log" >&2
    exit 1
fi
