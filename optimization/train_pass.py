#!/usr/bin/env python3
"""
Population-based training loop for the pass / targeted-kick skill.

Each candidate is a vector of kick-shaping parameters (the "action"). It is
evaluated by an episode-runner script (default optimization/start-pass-optimization.sh,
override with --script) which runs one episode of `pass_num_trials` kicks and
writes back a scalar fitness (negative mean delivery error, with penalties).

Optimizers:
  --optimizer cmaes   CMA-ES (needs the `cma` package; handles noisy /
                      ill-conditioned objectives - this is what UT Austin Villa
                      actually used).
  --optimizer cem     Cross-Entropy Method (stdlib only).

Robustness for long / remote runs:
  --out-dir DIR   writes history.csv (every episode), incumbent.txt (best params
                  so far), and checkpoint.json each iteration.
  --resume        continue from DIR/checkpoint.json.
  --reeval N      re-evaluate the distribution mean N times each iteration and
                  log the averaged score, so "incumbent" isn't a lucky draw.
  --fixed-file F  extra params prepended to every candidate (e.g. pass_prekick).

Usage (inside the container):
  python3 optimization/train_pass.py --optimizer cmaes --iterations 40 --pop 24 \
      --jobs 6 --fixed-file paramfiles/pass_prekick.txt --out-dir runs/prekick1
"""

import argparse, base64, concurrent.futures, json, os, pickle, random
import statistics, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULTS = os.path.join(ROOT, "paramfiles", "defaultParams.txt")
START = os.path.join(ROOT, "optimization", "start-pass-optimization.sh")

DEFAULT_PARAMS = ["kick_p1", "kick_p2", "kick_p3", "kick_p4", "kick_p5",
                  "kick_p6", "kick_p7", "kick_p8", "kick_p9",
                  "kick_scale1", "kick_scale2", "kick_scale3"]
# Contextual run also searches the power->distance policy coefficients.
CONTEXTUAL_PARAMS = DEFAULT_PARAMS + ["kick_power_a", "kick_power_b", "kick_power_c"]
# Starting values for params not present in defaultParams.txt.
PARAM_SEED = {"kick_power_a": 1.0, "kick_power_b": 0.3, "kick_power_c": 0.0}
FAIL_FITNESS = -100.0


def read_defaults():
    vals = {}
    with open(DEFAULTS) as f:
        for line in f:
            line = line.split("//")[0].strip()
            parts = line.split()
            if len(parts) >= 2:
                try:
                    vals[parts[0]] = float(parts[1])
                except ValueError:
                    pass
    return vals


class Evaluator:
    def __init__(self, script, body_type, params, fixed_text, ep_timeout):
        self.script, self.body_type = script, body_type
        self.params, self.fixed_text = params, fixed_text
        self.ep_timeout = ep_timeout

    def __call__(self, theta):
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False,
                                         dir="/tmp", prefix="cand_") as cf:
            cf.write(self.fixed_text)
            for k, v in zip(self.params, theta):
                cf.write(f"{k}\t{v}\n")
            cand = cf.name
        out = cand.replace("cand_", "fit_")
        try:
            r = subprocess.run([self.script, str(self.body_type), cand, out],
                               capture_output=True, text=True, timeout=self.ep_timeout)
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


def eval_batch(evaluator, thetas, jobs):
    if jobs <= 1:
        return [evaluator(t) for t in thetas]
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
        return list(ex.map(evaluator, thetas))


# --------------------------------------------------------------------------- #
class CEM:
    def __init__(self, mu, sigma, pop, elite_frac, seed):
        self.mu, self.sigma, self.pop = list(mu), list(sigma), pop
        self.n_elite = max(2, int(pop * elite_frac))
        self.rng = random.Random(seed)

    def ask(self):
        return [[self.rng.gauss(m, s) for m, s in zip(self.mu, self.sigma)]
                for _ in range(self.pop)]

    def tell(self, X, fits):
        order = sorted(range(len(X)), key=lambda i: fits[i], reverse=True)
        elite = [X[i] for i in order[:self.n_elite]]
        self.mu = [statistics.fmean(c) for c in zip(*elite)]
        self.sigma = [max(0.02, statistics.pstdev(c)) for c in zip(*elite)]

    def state(self):
        return {"mu": self.mu, "sigma": self.sigma}

    def load(self, s):
        self.mu, self.sigma = s["mu"], s["sigma"]


class CMAES:
    def __init__(self, mu, sigma0, pop, seed):
        import cma
        self.es = cma.CMAEvolutionStrategy(
            list(mu), sigma0,
            {"popsize": pop, "seed": seed, "verbose": -9})

    @property
    def mu(self):
        return list(self.es.mean)

    def ask(self):
        return [list(x) for x in self.es.ask()]

    def tell(self, X, fits):
        self.es.tell(X, [-f for f in fits])   # cma minimises

    def state(self):
        return {"pickle": base64.b64encode(pickle.dumps(self.es)).decode()}

    def load(self, s):
        self.es = pickle.loads(base64.b64decode(s["pickle"]))
# --------------------------------------------------------------------------- #


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--optimizer", choices=["cem", "cmaes"], default="cmaes")
    ap.add_argument("--iterations", type=int, default=30)
    ap.add_argument("--pop", type=int, default=24)
    ap.add_argument("--elite-frac", type=float, default=0.25)   # CEM only
    ap.add_argument("--sigma0", type=float, default=0.2,
                    help="initial search std as a fraction of |param value|")
    ap.add_argument("--jobs", type=int, default=1)
    ap.add_argument("--type", type=int, default=0)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--script", default=START)
    ap.add_argument("--params", default=",".join(DEFAULT_PARAMS),
                    help="comma-separated param names to optimise "
                         "('contextual' = the kick shape + power-policy set)")
    ap.add_argument("--fixed-file", default=None,
                    help="params file prepended to every candidate")
    ap.add_argument("--ep-timeout", type=float, default=300.0)
    ap.add_argument("--reeval", type=int, default=3,
                    help="episodes to average when scoring the incumbent")
    ap.add_argument("--out-dir", default=None)
    ap.add_argument("--resume", action="store_true")
    args = ap.parse_args()

    params = (CONTEXTUAL_PARAMS if args.params == "contextual"
              else args.params.split(","))
    d = read_defaults()
    mu0 = [d.get(p, PARAM_SEED.get(p, 0.0)) for p in params]
    sigma0_vec = [abs(m) * args.sigma0 + 0.3 for m in mu0]

    fixed_text = ""
    if args.fixed_file:
        with open(args.fixed_file) as f:
            fixed_text = f.read().rstrip() + "\n"

    ev = Evaluator(args.script, args.type, params, fixed_text, args.ep_timeout)

    if args.optimizer == "cmaes":
        opt = CMAES(mu0, statistics.fmean(sigma0_vec), args.pop, args.seed)
    else:
        opt = CEM(mu0, sigma0_vec, args.pop, args.elite_frac, args.seed)

    start_iter, best_score, best_theta = 0, -1e18, list(mu0)
    hist = None
    if args.out_dir:
        os.makedirs(args.out_dir, exist_ok=True)
        ckpt = os.path.join(args.out_dir, "checkpoint.json")
        if args.resume and os.path.exists(ckpt):
            s = json.load(open(ckpt))
            opt.load(s["opt"]); start_iter = s["iter"] + 1
            best_score, best_theta = s["best_score"], s["best_theta"]
            print(f"resumed from iter {start_iter}", flush=True)
        hist = open(os.path.join(args.out_dir, "history.csv"), "a")
        if hist.tell() == 0:
            hist.write("iter,kind,idx,fitness\n")

    def save_incumbent():
        if not args.out_dir:
            return
        with open(os.path.join(args.out_dir, "incumbent.txt"), "w") as f:
            f.write(f"// incumbent  score={best_score:.4f}\n" + fixed_text)
            for k, v in zip(params, best_theta):
                f.write(f"{k}\t{v}\n")
        json.dump({"iter": it, "opt": opt.state(), "best_score": best_score,
                   "best_theta": best_theta},
                  open(os.path.join(args.out_dir, "checkpoint.json"), "w"))

    for it in range(start_iter, args.iterations):
        t0 = time.time()
        X = opt.ask()
        fits = eval_batch(ev, X, args.jobs)
        opt.tell(X, fits)
        if hist:
            for i, f in enumerate(fits):
                hist.write(f"{it},pop,{i},{f:.4f}\n")

        # Score the distribution mean on fresh episodes.
        reeval = eval_batch(ev, [opt.mu] * args.reeval, min(args.jobs, args.reeval)) \
            if args.reeval else [max(fits)]
        mean_score = statistics.fmean(reeval)
        if hist:
            for i, f in enumerate(reeval):
                hist.write(f"{it},reeval,{i},{f:.4f}\n")
            hist.flush()
        if mean_score > best_score:
            best_score, best_theta = mean_score, list(opt.mu)
        save_incumbent()

        ok = [f for f in fits if f > FAIL_FITNESS]
        print(f"iter {it:3d}  incumbent={mean_score:7.3f}  best={best_score:7.3f}  "
              f"pop_best={max(fits):7.3f}  pop_med={statistics.median(fits):7.3f}  "
              f"fails={len(fits) - len(ok)}/{len(fits)}  ({time.time() - t0:.0f}s)",
              flush=True)

    print(f"\nbest incumbent score {best_score:.3f}"
          + (f"  ->  {args.out_dir}/incumbent.txt" if args.out_dir else ""))
    return 0 if best_score > FAIL_FITNESS else 1


if __name__ == "__main__":
    sys.exit(main())
