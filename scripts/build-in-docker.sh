#!/usr/bin/env bash
# Build agentspark inside the Ubuntu + SimSpark container.
#
#   ./scripts/build-in-docker.sh            # configure + build -> ./agentspark
#   ./scripts/build-in-docker.sh shell      # interactive shell in the container
#   ./scripts/build-in-docker.sh <cmd...>   # run an arbitrary command
#
# Needs a running Linux container engine (colima or Docker Desktop).
# The repo is bind-mounted at /src, so ./agentspark lands in the working tree.
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE=utaustinvilla3d-dev

docker build -t "$IMAGE" -f Dockerfile .

run() {
  if [ -t 0 ]; then
    docker run --rm -it -v "$PWD":/src -w /src "$IMAGE" "$@"
  else
    docker run --rm -v "$PWD":/src -w /src "$IMAGE" "$@"
  fi
}

build() { run bash -c 'rm -f CMakeCache.txt && cmake . -DCMAKE_BUILD_TYPE=Release && make -j"$(nproc)"'; }

smoke() {
  run bash -c '
    set -e
    [ -x ./agentspark ] || { rm -f CMakeCache.txt; cmake . -DCMAKE_BUILD_TYPE=Release >/dev/null; make -j"$(nproc)" >/dev/null; }
    rcssserver3d --agent-port 3100 --server-port 3200 >/tmp/server.log 2>&1 &
    sleep 5
    timeout 15 ./agentspark --host=127.0.0.1 --port 3100 --team SmokeTest --unum 1 --type 0 \
      --paramsfile paramfiles/defaultParams.txt --paramsfile paramfiles/defaultParams_t0.txt \
      >/tmp/agent.log 2>&1 || true
    grep -q "SimControlNode .AgentControl. registered" /tmp/server.log \
      && grep -q "Loading rsg" /tmp/agent.log \
      && echo "SMOKE TEST PASSED: agent connected to rcssserver3d and loaded the NAO model" \
      || { echo "SMOKE TEST FAILED"; echo "--- agent ---"; cat /tmp/agent.log; echo "--- server ---"; tail -20 /tmp/server.log; exit 1; }
  '
}

getup() {
  run bash -c '
    set -e
    [ -x ./agentspark ] || { rm -f CMakeCache.txt; cmake . -DCMAKE_BUILD_TYPE=Release >/dev/null; make -j"$(nproc)" >/dev/null; }
    rcssserver3d --agent-port 3100 --server-port 3200 >/tmp/server.log 2>&1 &
    sleep 4
    ./agentspark --host=127.0.0.1 --port 3100 --team Left --unum 1 --type 0 \
      --paramsfile paramfiles/defaultParams.txt --paramsfile paramfiles/defaultParams_t0.txt \
      >/tmp/agent.log 2>&1 &
    sleep 10
    python3 scripts/getup-test.py
  '
}

ensure_build='[ -x ./agentspark ] || { rm -f CMakeCache.txt; cmake . -DCMAKE_BUILD_TYPE=Release >/dev/null; make -j"$(nproc)" >/dev/null; }'

pass() {
  # one baseline pass episode with the stock kick params
  run bash -c "
    set -e
    $ensure_build
    ./optimization/start-pass-optimization.sh 0 paramfiles/pass_defaults.txt /tmp/pass_fitness.txt
    echo; echo -n 'mean fitness (negative delivery error): '; cat /tmp/pass_fitness.txt
  "
}

pass_train() {
  run bash -c "$ensure_build; python3 optimization/train_pass.py ${*:-}"
}

case "${1:-build}" in
  build)       build ;;
  shell)       run bash ;;
  smoke|test)  smoke ;;
  getup)       getup ;;
  pass)        pass ;;
  pass-train)  shift; pass_train "$@" ;;
  *)           run "$@" ;;
esac
