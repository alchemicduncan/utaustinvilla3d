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
    bool hasKicked;
    bool beamChecked;
    bool failedLastBeamCheck;
    bool backwards;
    bool fallen;

    int trial;
    double totalFitness;

    double INIT_WAIT_TIME;
    VecPosition passTarget;

    void initTrial();
    void computeTarget();
    void writeFitnessToOutputFile(double fitness);

public:

    OptimizationBehaviorPass(const std::string teamName, int uNum, const map<
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
