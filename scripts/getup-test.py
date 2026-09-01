#!/usr/bin/env python3
# Integration test for the fall-detection / get-up behaviour (behaviors/checkfall.cc).
#
# Assumes rcssserver3d is on :3100 (agent) / :3200 (monitor) and an agent is
# connected as (team Left, unum 1). Sets PlayOn, repositions the torso into the
# ground to force a fall, then watches the monitor stream: PASS if the torso
# height collapses (< 0.30 m) and then recovers (> 0.45 m).
#
# Driven by: scripts/build-in-docker.sh getup

import socket, struct, time, re, sys

def conn(port):
    s = socket.socket(); s.connect(("127.0.0.1", port)); s.settimeout(5); return s

def read_msg(s, buf):
    while len(buf) < 4:
        buf += s.recv(4096)
    n = struct.unpack("!I", buf[:4])[0]; buf = buf[4:]
    while len(buf) < n:
        buf += s.recv(65536)
    return buf[:n].decode("latin1"), buf[n:]

def send_cmd(s, cmd):
    b = cmd.encode()
    s.sendall(struct.pack("!I", len(b)) + b)

SLT = re.compile(r"\(SLT((?:\s+-?[\d.eE+-]+){16})\)")

def torso_z(msg):
    """max Z over the agent's body-part transforms (a big cluster of nodes);
    excludes the static marker sitting at exactly z=0.4."""
    zs = []
    for m in SLT.finditer(msg):
        v = [float(x) for x in m.group(1).split()]
        x, y, z = v[12], v[13], v[14]
        if abs(z - 0.4) < 0.02:      # static field marker, ignore
            continue
        if 0.0 < z < 1.3 and -25 < x < 25 and -20 < y < 20:
            zs.append(z)
    return (max(zs), len(zs)) if zs else (None, 0)

mon = conn(3200); buf = b""
full, buf = read_msg(mon, buf)
send_cmd(mon, "(playMode PlayOn)")
print("playMode -> PlayOn")

t0 = time.time()
knocked = False; knock_t = None
fell = False; stood_again_at = None; low = 9.9
while time.time() - t0 < 45:
    m2 = conn(3200); b2 = b""
    scene, b2 = read_msg(m2, b2); m2.close()
    z, n = torso_z(scene)
    el = time.time() - t0
    tag = ""
    if n < 8:
        print(f"t={el:5.1f}s  (only {n} nodes, skip)"); time.sleep(1.0); continue
    if not knocked and el > 5:
        for _ in range(5):
            send_cmd(mon, "(agent (unum 1) (team Left) (pos -13 0 0.04))")
        knocked = True; knock_t = el; tag = "  <-- KNOCK"
    if knocked and el > knock_t + 0.5:
        low = min(low, z)
        if z < 0.30:
            fell = True; tag = tag or "  fallen"
        if fell and z > 0.45 and stood_again_at is None and el > knock_t + 2:
            stood_again_at = el; tag = "  <-- STOOD UP"
    print(f"t={el:5.1f}s  torsoZ={z:.3f}  nodes={n}{tag}")
    time.sleep(1.0)

print()
print(f"lowest torsoZ after knock : {low:.3f}   (fallen threshold 0.30)")
print(f"stood back up at          : {stood_again_at}")
ok = fell and stood_again_at is not None
print("GETUP TEST PASSED - agent fell and stood back up" if ok
      else "GETUP TEST INCONCLUSIVE")
sys.exit(0 if ok else 1)
