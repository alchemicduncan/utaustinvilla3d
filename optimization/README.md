# Optimization
---

A couple example agents for executing a task and recording how well the agent did on the task (its fitness).  The [sample_start-optimization.sh](sample_start-optimization.sh) script runs rcssserver3d and an optimization behavior agent (fixedKickAgent or walkForwardAgent), with a provided file of parameters to test, that either attempts 10 kicks in a row (fixedKickAgent) or measures how far it can walk in 10 seconds (walkForwardAgent).  To choose between the kick or walk optimization task set the `task` variable at the beginning of the [sample_start-optimization.sh](sample_start-optimization.sh) script to either `"kick"` or `"walk"`.  Once an optimization task is completed the agent writes a fitness score to an output file.  As soon as the script detects that the output file is written it then kills the agent and server.

##### Example usage:
```bash
rm <output_file>;
./sample_start-optimization.sh <agent_body_type> <parameter_file> <output_file>;
cat <output_file>
```

Optimization behaviors use the `updateFitness()` method, which is called every simulation cycle, to monitor the progress of the agent and evaluate how well the agent is doing at the given task it is attempting.  Agents can control the state of the world (such as the playmode and positions of agents and the ball) by sending commands to the training command parser through the `setMonMessage()` method.

Remember to turn on ground truth information when running optimizations for accurate measurements and correct values for the `worldModel->getMyPositionGroundTruth()`, `worldModel->getMyAngDegGroundTruth()`, and `worldModel->getBallGroundTruth()` methods.  To do this you need to edit the *&lt;server_install_dir&gt;/share/rcssserver3d/rsg/agent/nao/naoneckhead.rsg* file and change the `setSenseMyPos`, `setSenseMyOrien`, and `setSenseBallPos` values to `true`.  You might want to call `worldModel->setUseGroundTruthDataForLocalization(true)` if the agent needs to always know exactly where it is on the field (such as might be the case when optimizing a walk and needing the agent to purposely walk to a specific target point on the field). 

Also a good idea is to turn off real-time mode and turn on sync mode for faster runs.  To do this set `$agentSyncMode` to `true` in *~/.simspark/spark.rb* and set `$enableRealTimeMode` to `false` in *&lt;server_install_dir&gt;/share/rcssserver3d/rcssserver3d.rb*.  Additionally you might want to turn off beam noise if the position of a beamed agent is being checked (as is done in the example optimization tasks).  To turn off beam noise set `BeamNoiseXY` and `BeamNoiseAngle` to `0` in *&lt;server_install_dir&gt;/share/rcssserver3d/naosoccersim.rb*.

---

## Pass / targeted-kick task (`passAgent`)

`OptimizationBehaviorPass` (in [optimizationbehaviors.cc](optimizationbehaviors.cc)) trains a kick that delivers the ball **to a target point**, not just as far as possible. The ball starts at the field centre; the robot is beamed just behind it (`pass_beam_*` in [../paramfiles/pass_defaults.txt](../paramfiles/pass_defaults.txt)); a target is read from `pass_target_dist` / `pass_target_angle`; the robot runs `kickBall(KICK_FORWARD, target)` and each trial is scored by `-‖ball_rest − target‖`, with extra penalties for falling, whiffing, or kicking backwards. Fitness is the mean over `PASS_NUM_TRIALS` kicks.

* **One episode:** `./start-pass-optimization.sh <body_type> <params_file> <output_file>` — self-configures the installed SimSpark for optimization (ground truth on, beam noise off, real-time off, sync on) and layers `<params_file>` on top of the defaults + `pass_defaults.txt`.
* **Training:** [train_pass.py](train_pass.py) — a Cross-Entropy Method loop over the kick-shaping parameters (`kick_p1..p9`, `kick_scale1..3`). Swap in PGPE / ARS / PPO here; the C++ side is unchanged.
* **In the container:** `./scripts/build-in-docker.sh pass` (one baseline episode) or `./scripts/build-in-docker.sh pass-train --iterations 15 --pop 16 --jobs 4`.

**Contextual (adaptive) version:** sample a different target per episode, write `pass_target_dist` / `pass_target_angle` into the candidate params file alongside the kick params, and make the policy a function of the target — i.e. learn `π(kick_params | target)` rather than a single fixed kick.

### Two-agent pass (`passReceiverAgent`)

A second agent (`OptimizationBehaviorPassReceiver`, same team, `--unum 3`) is beamed to the rendezvous point, holds position, and — once the ball is on its way and comes within `pass_receiver_reach` — walks to it. It also owns scoring: each trial is `-min(ball→receiver distance) - 0.5·(receiver travel)`, plus a `+2` bonus if the ball got within `pass_receiver_catch_radius`. This rewards passes that actually *arrive at a teammate*, penalises short / misdirected ones, and charges for how far the receiver had to chase.

In two-agent mode the passer stays in `PlayOn` and repositions the ball + both agents each trial with monitor `repos` commands (`pass_two_agent` / `pass_2agent.txt`), because self-beams are ignored outside dead-ball playmodes and `BeforeKickOff` would drag the rendezvous onside.

* **One episode:** `./start-2agent-pass.sh <body_type> <params_file> <output_file>`
* **In the container:** `./scripts/build-in-docker.sh pass2` or `pass2-train [--iterations N --pop M --jobs J]`

Known limitation: the monitor `repos` teleports the torso but does not fully reset joint state, so over a long episode the passer occasionally destabilises and whiffs the last few trials (scored as a miss). Keep `pass_num_trials` modest, or add a brief `(playMode BeforeKickOff)` blip for a full reset.

**Next:** make the receiver learned too (interception timing, first touch); or move the rendezvous each episode so the passer must *lead* a receiver that is already moving.
