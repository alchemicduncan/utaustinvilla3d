#!/usr/bin/env python3
"""
Cross-Entropy Method (CEM) training loop for the pass / targeted-kick skill.

Each candidate is a vector of kick-shaping parameters (the "action"). It is
evaluated by optimization/start-pass-optimization.sh, which runs one episode
(PASS_NUM_TRIALS kicks at a target) and writes back a scalar fitness
(negative mean delivery error, with penalties).

CEM is the simplest population-based policy search: sample around a Gaussian,
keep the elite fraction, refit the Gaussian, repeat. Swap in PGPE / ARS / PPO
here without touching the C++ side.

This first version trains for a FIXED target (pass_target_dist / _angle from
paramfiles/pass_defaults.txt). For the contextual version, sample a target per
episode and write pass_target_* into the candidate file alongside the kick
params, and make PARAMS a function of the target.

Usage (inside the container):
    python3 optimization/train_pass.py --iterations 15 --pop 16 --jobs 4
"""

import argparse, concurrent.futures, os, random, statistics, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
START = os.path.join(ROOT, "optimization", "start-pass-optimization.sh")
DEFAULTS = os.path.join(ROOT, "paramfiles", "defaultParams.txt")
BEST_OUT = os.path.join(ROOT, "paramfiles", "pass_best.txt")

# kick-shaping parameters for SKILL_KICK_LEFT_LEG (see skills/kick.skl)
PARAMS = ["kick_p1", "kick_p2", "kick_p3", "kick_p4", "kick_p5",
          "kick_p6", "kick_p7", "kick_p8", "kick_p9",
          "kick_scale1", "kick_scale2", "kick_scale3"]

FAIL_FITNESS = -100.0


def read_defaults():
    vals = {}
    with open(DEFAULTS) as f:
        for line in f:
            line = line.split("//")[0].strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) >= 2:
                try:
                    vals[parts[0]] = float(parts[1])
                except ValueError:
                    pass
    return vals


def evaluate(theta, body_type):
    """Run one episode for parameter vector theta; return scalar fitness."""
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False,
                                     dir="/tmp", prefix="cand_") as cf:
        for k, v in zip(PARAMS, theta):
            cf.write(f"{k}\t{v}\n")
        cand = cf.name
    out = cand.replace("cand_", "fit_")
    try:
        r = subprocess.run([START, str(body_type), cand, out],
                           capture_output=True, text=True, timeout=300)
        if r.returncode != 0 or not os.path.exists(out):
            return FAIL_FITNESS
        with open(out) as f:
            return float(f.read().strip())
    except (subprocess.TimeoutExpired, ValueError):
        return FAIL_FITNESS
    finally:
        for p in (cand, out):
            try:
                os.remove(p)
            except OSError:
                pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iterations", type=int, default=15)
    ap.add_argument("--pop", type=int, default=16)
    ap.add_argument("--elite-frac", type=float, default=0.25)
    ap.add_argument("--jobs", type=int, default=1)
    ap.add_argument("--type", type=int, default=0, help="nao body type")
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()
    random.seed(args.seed)

    d = read_defaults()
    mu = [d[p] for p in PARAMS]
    sigma = [abs(m) * 0.15 + 0.5 for m in mu]
    n_elite = max(2, int(args.pop * args.elite_frac))

    best_theta, best_fit = list(mu), -1e9
    for it in range(args.iterations):
        pop = [[random.gauss(m, s) for m, s in zip(mu, sigma)]
               for _ in range(args.pop)]
        pop[0] = list(mu)  # always keep the current mean

        t0 = time.time()
        if args.jobs > 1:
            with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
                fits = list(ex.map(lambda th: evaluate(th, args.type), pop))
        else:
            fits = [evaluate(th, args.type) for th in pop]

        order = sorted(range(len(pop)), key=lambda i: fits[i], reverse=True)
        elite = [pop[i] for i in order[:n_elite]]
        mu = [statistics.fmean(col) for col in zip(*elite)]
        sigma = [max(0.05, statistics.pstdev(col)) for col in zip(*elite)]

        if fits[order[0]] > best_fit:
            best_fit, best_theta = fits[order[0]], list(pop[order[0]])
            with open(BEST_OUT, "w") as f:
                f.write("// best pass kick params from train_pass.py\n")
                for k, v in zip(PARAMS, best_theta):
                    f.write(f"{k}\t{v}\n")

        print(f"iter {it:2d}  best={fits[order[0]]:.3f}  "
              f"elite_mean={statistics.fmean(fits[i] for i in order[:n_elite]):.3f}  "
              f"pop_mean={statistics.fmean(fits):.3f}  "
              f"({time.time() - t0:.0f}s)", flush=True)

    print(f"\nBest fitness {best_fit:.3f}  ->  {BEST_OUT}")
    return 0 if best_fit > FAIL_FITNESS else 1


if __name__ == "__main__":
    sys.exit(main())
