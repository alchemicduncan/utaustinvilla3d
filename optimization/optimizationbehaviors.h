#ifndef _OPTIMIZATION_BEHAVIORS_H
#define _OPTIMIZATION_BEHAVIORS_H

#include "../behaviors/naobehavior.h"

bool isBallMoving(const WorldModel *worldModel);

class OptimizationBehaviorFixedKick: public NaoBehavior {
    const string outputFile;

    double timeStart;
    bool hasKicked;
    bool beamChecked;
    bool backwards;
    bool ranIntoBall;
    bool fallen;

    int kick;

    double INIT_WAIT_TIME;

    VecPosition ballInitPos;
    void initKick();
    void writeFitnessToOutputFile(double fitness);

public:

    OptimizationBehaviorFixedKick(const std::string teamName, int uNum, const map<
                                  string, string>& namedParams_, const string& rsg_, const string& outputFile_);

    virtual void beam(double& beamX, double& beamY, double& beamAngle);
    virtual SkillType selectSkill();
    virtual void updateFitness();

};

/*
 * Pass optimization agent.
 *
 * The ball starts at the field centre (auto-reset by "(playMode BeforeKickOff)")
 * and the robot is beamed just behind it.  A target point is read from the
 * params file (pass_target_dist / pass_target_angle); the robot executes a
 * targeted kick and the trial is scored by how close the ball comes to rest to
 * that target.  Fitness (averaged over PASS_NUM_TRIALS) is the negative mean
 * delivery error, with extra penalties for falling / whiffing / kicking
 * backwards.
 *
 * The outer training loop (optimization/train_pass.py) writes the kick
 * parameters (the "action") and, for the contextual version, the target (the
 * "state") into the params file between episodes.
 */
class OptimizationBehaviorPass : public NaoBehavior {
    const string outputFile;

    double timeStart;
    double kickStartTime;
    double reposTime;      // two-agent: when the monitor repos was sent
    bool hasKicked;
    bool ballEverMoved;
    bool beamChecked;
    bool failedLastBeamCheck;
    bool backwards;
    bool fallen;

    int trial;
    int numTrials;
    double totalFitness;

    double INIT_WAIT_TIME;
    VecPosition passTarget;

    // Pre-positioned mode: beam the robot right at the ball (aligned with the
    // target) and fire SKILL_KICK_LEFT_LEG directly - no walk-up. Removes the
    // approach/positioning variance so training can calibrate power+aim cleanly.
    bool prekick;

    // Two-agent mode: stay in PlayOn, reposition ball + both agents via monitor.
    bool twoAgent;
    int passerUnum;
    int receiverUnum;
    string receiverTeam;

    void initTrial();
    void computeTarget();
    std::string twoAgentResetMsg();
    void writeFitnessToOutputFile(double fitness);

public:

    OptimizationBehaviorPass(const std::string teamName, int uNum, const map<
                             string, string>& namedParams_, const string& rsg_, const string& outputFile_);

    virtual void beam(double& beamX, double& beamY, double& beamAngle);
    virtual SkillType selectSkill();
    virtual void updateFitness();

};

/*
 * Receiver half of the two-agent pass task.
 *
 * Runs as a second agent (a different --unum, same team) alongside a passAgent.
 * The passAgent owns the trial FSM and the monitor (beam + playmode); this agent
 * only observes and moves.  It is beamed to the nominal rendezvous point
 * (pass_target_dist / pass_target_angle); once the ball is struck it walks to
 * the ball and tries to stop over it.
 *
 * It also owns *scoring*: each trial is scored by how close the ball actually
 * gets to this receiver (ground truth), with a completion bonus if the ball
 * arrives within pass_receiver_catch_radius.  Fitness (mean over pass_num_trials)
 * is written to this agent's --experimentout file, which the training loop reads.
 *
 * Trials are kept in sync with the passer purely through the shared playmode:
 * PlayOn -> ball tracked;  a PlayOn -> BeforeKickOff edge ends and scores a trial.
 */
class OptimizationBehaviorPassReceiver : public NaoBehavior {
    const string outputFile;

    int trial;
    int numTrials;
    double totalFitness;
    double catchRadius;
    double reach;          // how far the receiver may stray from the rendezvous

    bool wasBallCentred;   // for detecting the passer's per-trial ball reset
    bool sawFirstReset;
    double trialStartTime; // fallback so a whiffed trial can't stall the receiver
    bool ballKicked;
    double minBallDist;    // closest the ball got to the receiver
    double travel;         // how far the receiver had to move to get there

    VecPosition rendezvous;

    void resetTrial();
    void scoreTrial();
    void writeFitnessToOutputFile(double fitness);

public:

    OptimizationBehaviorPassReceiver(const std::string teamName, int uNum, const map<
                                     string, string>& namedParams_, const string& rsg_, const string& outputFile_);

    virtual void beam(double& beamX, double& beamY, double& beamAngle);
    virtual SkillType selectSkill();
    virtual void updateFitness();

};

class OptimizationBehaviorWalkForward : public NaoBehavior {
    const string outputFile;

    int run;
    double startTime;
    bool beamChecked;
    double INIT_WAIT;
    double totalWalkDist;

    void init();
    bool checkBeam();

public:

    OptimizationBehaviorWalkForward(const std::string teamName, int uNum, const map<string, string>& namedParams_, const string& rsg_, const string& outputFile_);

    virtual void beam( double& beamX, double& beamY, double& beamAngle );
    virtual SkillType selectSkill();
    virtual void updateFitness();

};

#endif
