#!/bin/bash
#
# Run one pass-optimization episode: start rcssserver3d + a passAgent, wait for
# the agent to write its fitness, then tear everything down.
#
#   ./optimization/start-pass-optimization.sh <body_type> <params_file> <output_file>
#
# <params_file> is layered on top of the defaults and typically contains the
# kick parameters being evaluated (and, for contextual training, pass_target_*).
#
# Designed to run inside the Ubuntu + SimSpark container (scripts/build-in-docker.sh).
set -u

TYPE="${1:?body type}"
PARAMS_FILE="${2:?params file}"
OUTPUT_FILE="${3:?output file}"

DIR_SCRIPT="$(cd "$(dirname "$0")" && pwd)"
ROOT="$DIR_SCRIPT/.."
PARAMS_FILE="$(cd "$(dirname "$PARAMS_FILE")" && pwd)/$(basename "$PARAMS_FILE")"
OUTPUT_FILE="$(cd "$(dirname "$OUTPUT_FILE")" && pwd)/$(basename "$OUTPUT_FILE")"

SPARK="${SPARK_DIR:-/opt/simspark}/share"

# --- one-time sim config for optimization: ground truth, no beam noise, fast ---
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

# random high ports so multiple episodes can run concurrently
AGENTPORT=$(( (RANDOM % 20000) + 20000 ))
SERVERPORT=$(( AGENTPORT + 1 ))

rm -f "$OUTPUT_FILE"

rcssserver3d --agent-port "$AGENTPORT" --server-port "$SERVERPORT" \
    >/tmp/pass_server_$AGENTPORT.log 2>&1 &
SERVER_PID=$!
sleep 3

cd "$ROOT"
./agentspark --unum 2 --type "$TYPE" \
    --paramsfile paramfiles/defaultParams.txt \
    --paramsfile "paramfiles/defaultParams_t$TYPE.txt" \
    --paramsfile paramfiles/pass_defaults.txt \
    --paramsfile "$PARAMS_FILE" \
    --experimentout "$OUTPUT_FILE" \
    --optimize passAgent \
    --port "$AGENTPORT" --mport "$SERVERPORT" \
    >/tmp/pass_agent_$AGENTPORT.log 2>&1 &
AGENT_PID=$!

# wait for fitness (or give up)
MAX_WAIT=180
waited=0
while [ ! -f "$OUTPUT_FILE" ] && [ "$waited" -lt "$MAX_WAIT" ]; do
    sleep 1
    waited=$((waited + 1))
done

kill -s INT "$AGENT_PID"  2>/dev/null
kill -s INT "$SERVER_PID" 2>/dev/null
sleep 1
kill -9 "$AGENT_PID" "$SERVER_PID" 2>/dev/null

if [ ! -f "$OUTPUT_FILE" ]; then
    echo "TIMED OUT after ${waited}s (no fitness written)" >&2
    echo "--- agent log tail ---" >&2; tail -20 "/tmp/pass_agent_$AGENTPORT.log" >&2
    exit 1
fi
