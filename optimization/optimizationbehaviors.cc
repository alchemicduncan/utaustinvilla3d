#include "optimizationbehaviors.h"
#include <fstream>
#include <sstream>


/*
 *
 *
 * Fixed Kick optimization agent
 *
 *
 */
OptimizationBehaviorFixedKick::OptimizationBehaviorFixedKick(const std::string teamName,
        int uNum, const map<string, string>& namedParams_, const string& rsg_,
        const string& outputFile_) :
    NaoBehavior(teamName, uNum, namedParams_, rsg_), outputFile(outputFile_),
    kick(0), INIT_WAIT_TIME(3.0)
{
    initKick();
}

void OptimizationBehaviorFixedKick::beam(double& beamX, double& beamY,
        double& beamAngle) {
    beamX = atof(namedParams.find("kick_xoffset")->second.c_str());
    beamY = atof(namedParams.find("kick_yoffset")->second.c_str());
    beamAngle = atof(namedParams.find("kick_angle")->second.c_str());
}

SkillType OptimizationBehaviorFixedKick::selectSkill() {
    double time = worldModel->getTime();
    if (timeStart < 0) {
        initKick();
        return SKILL_STAND;
    }

    // Wait a bit before attempting kick
    if (time-timeStart <= INIT_WAIT_TIME) {
        return SKILL_STAND;
    }

    if (!hasKicked) {
        hasKicked = true;
        return SKILL_KICK_LEFT_LEG; // The kick skill that we're optimizing
    }

    return SKILL_STAND;
}

void OptimizationBehaviorFixedKick::updateFitness() {
    static double totalFitness = 0.0;
    if (kick == 10) {
        writeFitnessToOutputFile(totalFitness/(double(kick)));
        return;
    }

    double time = worldModel->getTime();
    VecPosition meTruth = worldModel->getMyPositionGroundTruth();
    meTruth.setZ(0);

    if (time-timeStart <= INIT_WAIT_TIME) {
        return;
    }

    static bool failedLastBeamCheck = false;
    if(!beamChecked) {
        cout << "Checking whether beam was successful\n";
        beamChecked = true;
        LOG_STR("Checking whether beam was successful");

        meTruth.setZ(0);
        double beamX, beamY, beamAngle;
        beam(beamX, beamY, beamAngle);
        VecPosition meDesired = VecPosition(beamX, beamY, 0);
        double distance = meTruth.getDistanceTo(meDesired);
        double angle = worldModel->getMyAngDegGroundTruth();
        VecPosition ballPos = worldModel->getBallGroundTruth();
        double ballDistance = ballPos.getMagnitude();

        // Check that we're close to our expected position and angle
        // and also that the ball is close to it's exepected position
        if(distance > .1 || ballDistance > .1 || abs(angle - beamAngle) > 3) {
            cout << distance << "\t" << ballDistance << "\n";
            LOG_STR("Problem with the beam!");
            LOG(distance);
            LOG(meTruth);
            LOG(meDesired);
            if (failedLastBeamCheck) {
                kick++;
                totalFitness -= 100;
                failedLastBeamCheck = false;
            } else {
                failedLastBeamCheck = true;
            }
            initKick();
            return;
        }
        failedLastBeamCheck = false;
        string msg = "(playMode KickOff_Left)";
        setMonMessage(msg);
    }

    if (!hasKicked && worldModel->getPlayMode() == PM_PLAY_ON) {
        ranIntoBall = true;
    }

    if (!hasKicked) {
        return;
    }

    VecPosition ballTruth = worldModel->getBallGroundTruth();
    if (ballTruth.getX() < -.25) {
        backwards = true;
    }

    if (worldModel->isFallen()) {
        fallen = true;
    }

    if (worldModel->isFallen()) {
        totalFitness += -1;
        kick++;
        initKick();
        return;
    }

    if (time - (timeStart + INIT_WAIT_TIME) > 15 && !isBallMoving(this->worldModel)) {
        double angleOffset = abs(VecPosition(0, 0, 0).getAngleBetweenPoints(VecPosition(20, 0, 0), ballTruth));
        double distance = ballTruth.getX();
        double fitness = distance;

        if (backwards || distance <= 0.1 || ranIntoBall) {
            fitness = -100;
            if (backwards) {
                cout << "Detected backward kick" << endl;
            } else if (ranIntoBall) {
                cout << "Detected ranIntoBall" << endl;
            } else {
                cout << "Detected insufficient distance" << endl;
            }
        }
        cout << "Traveled distance = " << distance << endl;
        cout << "Fitness = " << fitness << endl;
        cout << "Final position = " << ballTruth.getX() << ", " << ballTruth.getY() << endl;

        totalFitness += fitness;
        kick++;
        initKick();
        return;
    }
}

void OptimizationBehaviorFixedKick::initKick() {
    hasKicked = false;
    beamChecked = false;
    backwards = false;
    ranIntoBall = false;
    timeStart = worldModel->getTime();
    initialized = false;
    initBeamed = false;
    fallen = false;
    resetSkills();

    // Beam agent and ball
    double beamX, beamY, beamAngle;
    beam(beamX, beamY, beamAngle);
    VecPosition beamPos = VecPosition(beamX, beamY, 0);
    string msg = "(playMode BeforeKickOff)";
    setMonMessage(msg);
}

void OptimizationBehaviorFixedKick::writeFitnessToOutputFile(double fitness) {
    static bool written = false;
    if (!written) {
        LOG(fitness);
        LOG(kick);
        fstream file;
        file.open(outputFile.c_str(), ios::out);
        file << fitness << endl;
        file.close();
        written = true;
        //string msg = "(killsim)";
        //setMonMessage(msg);
    }
}


/* Checks if the ball is currently moving */
bool isBallMoving(const WorldModel *worldModel) {
    static VecPosition lastBall = worldModel->getBallGroundTruth();
    static double lastTime = worldModel->getTime();

    double thisTime = worldModel->getTime();
    VecPosition thisBall = worldModel->getBallGroundTruth();

    thisBall.setZ(0);
    lastBall.setZ(0);

    if(thisBall.getDistanceTo(lastBall) > 0.01)
    {
        // the ball moved!
        lastBall = thisBall;
        lastTime = thisTime;
        return true;
    }

    if(thisTime - lastTime < 0.5)
    {
        // not sure yet if the ball has settled
        return true;
    }
    else
    {
        return false;
    }
}


void writeToOutputFile(const string &filename, const string &output) {
//  static bool written = false;
//  assert(!written);
    //  LOG(output);
    fstream file;
    file.open(filename.c_str(), ios::out);
    file << output;
    file.close();
//  written = true;
}


/*
 *
 *
 * PASS OPTIMIZATION AGENT
 *
 * Kick the ball from the field centre to a target point and score by delivery
 * error.  See optimizationbehaviors.h for the overall idea.
 *
 */

// Kicks per episode. More trials -> lower fitness variance (the per-trial
// delivery error has ~1.5 m std), at a proportional cost in wall time.
// Override with the pass_num_trials param.
static const int PASS_NUM_TRIALS_DEFAULT = 20;
// The ball counts as struck once it has left the centre by this much.
static const double PASS_BALL_MOVED_DIST = 0.5;     // m
// Per-trial time budget after the initial settle.
static const double PASS_TRIAL_TIMEOUT = 20.0;      // s

OptimizationBehaviorPass::OptimizationBehaviorPass(const std::string teamName,
        int uNum, const map<string, string>& namedParams_, const string& rsg_,
        const string& outputFile_) :
    NaoBehavior(teamName, uNum, namedParams_, rsg_), outputFile(outputFile_),
    failedLastBeamCheck(false), trial(0), totalFitness(0.0), INIT_WAIT_TIME(2.0)
{
    // Ground-truth ball / self pose for accurate scoring (see optimization/README.md).
    worldModel->setUseGroundTruthDataForLocalization(true);
    timeStart = -1;
    map<string, string>::const_iterator it = namedParams.find("pass_num_trials");
    numTrials = (it != namedParams.end()) ? atoi(it->second.c_str())
                                          : PASS_NUM_TRIALS_DEFAULT;

    it = namedParams.find("pass_prekick");
    prekick = (it != namedParams.end() && atoi(it->second.c_str()) != 0);

    double baseDist  = atof(namedParams.find("pass_target_dist")->second.c_str());
    it = namedParams.find("pass_target_dist_min");
    dMin = (it != namedParams.end()) ? atof(it->second.c_str()) : baseDist;
    it = namedParams.find("pass_target_dist_max");
    dMax = (it != namedParams.end()) ? atof(it->second.c_str()) : baseDist;
    it = namedParams.find("pass_target_angle_max");
    aMax = (it != namedParams.end()) ? atof(it->second.c_str()) : 0.0;
    it = namedParams.find("kick_power_a");
    powA = (it != namedParams.end()) ? atof(it->second.c_str()) : 1.0;
    it = namedParams.find("kick_power_b");
    powB = (it != namedParams.end()) ? atof(it->second.c_str()) : 0.0;
    it = namedParams.find("kick_power_c");
    powC = (it != namedParams.end()) ? atof(it->second.c_str()) : 0.0;
    it = namedParams.find("pass_ctx_seed");
    rngState = (it != namedParams.end()) ? (unsigned long long)atoll(it->second.c_str()) : 42ULL;

    it = namedParams.find("pass_two_agent");
    twoAgent = (it != namedParams.end() && atoi(it->second.c_str()) != 0);
    passerUnum = uNum;
    it = namedParams.find("pass_receiver_unum");
    receiverUnum = (it != namedParams.end()) ? atoi(it->second.c_str()) : 3;
    it = namedParams.find("pass_receiver_team");
    receiverTeam = (it != namedParams.end()) ? it->second : string("Right");

    initTrial();   // samples the first target
}

double OptimizationBehaviorPass::nextRand() {
    // Local LCG (SplitMix-ish) - global rand() is consumed elsewhere per cycle,
    // so we can't rely on it for a reproducible target sequence.
    rngState = rngState * 6364136223846793005ULL + 1442695040888963407ULL;
    return (double)(rngState >> 33) / (double)(1ULL << 31);
}

void OptimizationBehaviorPass::sampleTarget() {
    double baseAngle = atof(namedParams.find("pass_target_angle")->second.c_str());
    curDist  = dMin + (dMax - dMin) * nextRand();
    curAngle = baseAngle + (2.0 * nextRand() - 1.0) * aMax;
    passTarget = VecPosition(curDist * cosDeg(curAngle),
                             curDist * sinDeg(curAngle), 0);
}

double OptimizationBehaviorPass::kickPowerForDist(double d) const {
    double half = (dMax - dMin) / 2.0;
    if (half < 1e-6) half = 1.0;
    double dn = (d - (dMin + dMax) / 2.0) / half;
    double p = powA + powB * dn + powC * dn * dn;
    if (p < 0.3) p = 0.3;
    if (p > 3.0) p = 3.0;
    return p;
}

void OptimizationBehaviorPass::beam(double& beamX, double& beamY, double& beamAngle) {
    if (prekick) {
        // Beam right at the ball, aligned with THIS trial's target heading:
        // take the standard kick foot-offset and rotate it into that direction.
        double angle = curAngle;
        double offX = atof(namedParams.find("kick_xoffset")->second.c_str());
        double offY = atof(namedParams.find("kick_yoffset")->second.c_str());
        double a = Deg2Rad(angle);
        beamX = offX * cos(a) - offY * sin(a);
        beamY = offX * sin(a) + offY * cos(a);
        beamAngle = angle;
        return;
    }
    beamX     = atof(namedParams.find("pass_beam_x")->second.c_str());
    beamY     = atof(namedParams.find("pass_beam_y")->second.c_str());
    beamAngle = atof(namedParams.find("pass_beam_angle")->second.c_str());
}

std::string OptimizationBehaviorPass::twoAgentResetMsg() {
    double bx = atof(namedParams.find("pass_beam_x")->second.c_str());
    double by = atof(namedParams.find("pass_beam_y")->second.c_str());
    double heading = atof(namedParams.find("pass_beam_angle")->second.c_str());
    double rxHeading = atof(namedParams.find("pass_target_angle")->second.c_str()) + 180.0;
    // Low placement height: the repos teleports the torso and the robot drops
    // to it, so a near-standing height keeps repeated trials from destabilising
    // the walk engine.
    const double Z = 0.34;
    std::ostringstream m;
    m << "(playMode PlayOn)"
      << "(ball (pos 0 0 0.042) (vel 0 0 0))"
      << "(agent (unum " << passerUnum << ") (team " << agentTeamName
      << ") (pos " << bx << " " << by << " " << Z << ") (rot 0 0 " << heading << "))"
      << "(agent (unum " << receiverUnum << ") (team " << receiverTeam
      << ") (pos " << passTarget.getX() << " " << passTarget.getY()
      << " " << Z << ") (rot 0 0 " << rxHeading << "))";
    return m.str();
}

void OptimizationBehaviorPass::initTrial() {
    hasKicked = false;
    ballEverMoved = false;
    beamChecked = false;
    backwards = false;
    fallen = false;
    kickStartTime = -1;
    reposTime = -1;
    timeStart = worldModel->getTime();
    initialized = false;
    resetSkills();

    sampleTarget();
    // Set the kick strength for this trial's target distance, then re-parse
    // the kick skill so the swing uses it.
    reloadKickSkill(kickPowerForDist(curDist));

    if (twoAgent) {
        // Stay in PlayOn; reposition ball + both agents via the monitor.
        // (Self-beams are ignored in open play, and BeforeKickOff drags the
        // receiver's rendezvous onside.)
        initBeamed = true;
        setMonMessage(twoAgentResetMsg());
    } else {
        initBeamed = false;
        // BeforeKickOff re-centres the ball; beam() re-positions the robot.
        setMonMessage("(playMode BeforeKickOff)");
    }
}

SkillType OptimizationBehaviorPass::selectSkill() {
    if (timeStart < 0 || trial >= numTrials) {
        return SKILL_STAND;
    }

    double time = worldModel->getTime();
    if (time - timeStart <= INIT_WAIT_TIME) {
        return SKILL_STAND;
    }

    // Wait until the beam has been verified before doing anything.
    if (!beamChecked) {
        return SKILL_STAND;
    }

    // Two-agent: let the robot settle after the post-BeforeKickOff repos drop.
    if (twoAgent && (reposTime < 0 || time - reposTime < 1.0)) {
        return SKILL_STAND;
    }

    if (prekick) {
        // Already stood right at the ball, aligned with the target: just fire.
        if (!hasKicked) {
            hasKicked = true;
            kickStartTime = time;
            return SKILL_KICK_LEFT_LEG;
        }
        return SKILL_STAND;
    }

    // Walk up to the ball (if needed), line up and kick toward the target.
    return kickBall(KICK_FORWARD, passTarget);
}

void OptimizationBehaviorPass::updateFitness() {
    if (trial >= numTrials) {
        writeFitnessToOutputFile(totalFitness / double(numTrials));
        return;
    }

    double time = worldModel->getTime();
    if (time - timeStart <= INIT_WAIT_TIME) {
        return;
    }

    if (!beamChecked) {
        beamChecked = true;

        if (twoAgent) {
            // Re-send the repos (the first send in initTrial can race the
            // receiver's connection) and record when, so selectSkill() can let
            // the robot settle before kicking.
            setMonMessage(twoAgentResetMsg());
            reposTime = time;
            double ballDistance = worldModel->getBallGroundTruth().getMagnitude();
            if (ballDistance > 0.5) {
                if (failedLastBeamCheck) {
                    failedLastBeamCheck = false;
                    totalFitness += -100;
                    trial++;
                } else {
                    failedLastBeamCheck = true;
                }
                initTrial();
                return;
            }
            failedLastBeamCheck = false;
            return;
        }

        VecPosition meTruth = worldModel->getMyPositionGroundTruth();
        meTruth.setZ(0);
        double beamX, beamY, beamAngle;
        beam(beamX, beamY, beamAngle);
        VecPosition meDesired(beamX, beamY, 0);
        double angle = worldModel->getMyAngDegGroundTruth();
        double ballDistance = worldModel->getBallGroundTruth().getMagnitude();

        if (meTruth.getDistanceTo(meDesired) > 0.1 || ballDistance > 0.1 ||
                fabs(angle - beamAngle) > 3) {
            // Give the beam one more chance before counting a failure.
            if (failedLastBeamCheck) {
                failedLastBeamCheck = false;
                totalFitness += -100;
                trial++;
            } else {
                failedLastBeamCheck = true;
            }
            initTrial();
            return;
        }
        failedLastBeamCheck = false;
        setMonMessage("(playMode PlayOn)");
        return;
    }

    VecPosition ballTruth = worldModel->getBallGroundTruth();
    ballTruth.setZ(0);

    // The ball starts at the field centre; once it has clearly left, it was
    // struck. (Displacement is more reliable here than sampling absVel.)
    if (ballTruth.getMagnitude() > PASS_BALL_MOVED_DIST) {
        ballEverMoved = true;
        if (!hasKicked) {
            hasKicked = true;
            kickStartTime = time;
        }
    }

    // Distance the ball travelled along the target direction (negative = wrong way).
    VecPosition tdir = passTarget;
    tdir.normalize();
    double alongTarget = ballTruth.getX() * tdir.getX() + ballTruth.getY() * tdir.getY();
    if (alongTarget < -0.25) {
        backwards = true;
    }
    if (worldModel->isFallen()) {
        fallen = true;
    }

    // Wait until the ball has actually moved and then come to rest (the kick
    // motion itself takes several seconds; prekick fires it before the ball
    // stirs, so gate on ballEverMoved rather than a short post-fire delay).
    bool settled = ballEverMoved && !isBallMoving(this->worldModel);
    bool timedOut = time - (timeStart + INIT_WAIT_TIME) > PASS_TRIAL_TIMEOUT;

    if (!settled && !timedOut) {
        return;
    }

    double error = ballTruth.getDistanceTo(passTarget);
    double fitness = -error;

    if (!ballEverMoved || backwards) {
        // Whiffed or kicked the wrong way: worse than any legitimate miss.
        fitness = -(passTarget.getMagnitude() + 5.0);
    }
    if (fallen) {
        fitness -= 3.0;
    }

    cout << "Trial " << trial
         << "  tgt(d=" << curDist << ",a=" << curAngle << ")"
         << "  pow=" << kickPowerForDist(curDist)
         << "  ball=(" << ballTruth.getX() << ", " << ballTruth.getY() << ")"
         << "  error=" << error
         << "  moved=" << ballEverMoved << " backwards=" << backwards
         << " fell=" << fallen
         << "  fitness=" << fitness << endl;

    totalFitness += fitness;
    trial++;
    initTrial();
}

void OptimizationBehaviorPass::writeFitnessToOutputFile(double fitness) {
    static bool written = false;
    if (written) {
        return;
    }
    written = true;
    LOG(fitness);
    cout << "Mean fitness over " << numTrials << " trials = " << fitness << endl;
    fstream file;
    file.open(outputFile.c_str(), ios::out);
    file << fitness << endl;
    file.close();
}


/*
 *
 *
 * PASS RECEIVER AGENT (two-agent pass)
 *
 * See optimizationbehaviors.h. This is the second agent; it observes, moves to
 * the ball once it is struck, and owns scoring for the pass.
 *
 */
OptimizationBehaviorPassReceiver::OptimizationBehaviorPassReceiver(
        const std::string teamName, int uNum, const map<string, string>& namedParams_,
        const string& rsg_, const string& outputFile_) :
    NaoBehavior(teamName, uNum, namedParams_, rsg_), outputFile(outputFile_),
    trial(0), totalFitness(0.0), wasBallCentred(false), sawFirstReset(false)
{
    worldModel->setUseGroundTruthDataForLocalization(true);

    map<string, string>::const_iterator it = namedParams.find("pass_num_trials");
    numTrials = (it != namedParams.end()) ? atoi(it->second.c_str()) : 20;
    it = namedParams.find("pass_receiver_catch_radius");
    catchRadius = (it != namedParams.end()) ? atof(it->second.c_str()) : 0.5;
    it = namedParams.find("pass_receiver_reach");
    reach = (it != namedParams.end()) ? atof(it->second.c_str()) : 2.0;

    double dist  = atof(namedParams.find("pass_target_dist")->second.c_str());
    double angle = atof(namedParams.find("pass_target_angle")->second.c_str());
    rendezvous = VecPosition(dist * cosDeg(angle), dist * sinDeg(angle), 0);

    resetTrial();
}

void OptimizationBehaviorPassReceiver::beam(double& beamX, double& beamY, double& beamAngle) {
    beamX = rendezvous.getX();
    beamY = rendezvous.getY();
    // Face back toward the passer / incoming ball.
    beamAngle = atof(namedParams.find("pass_target_angle")->second.c_str()) + 180.0;
}

void OptimizationBehaviorPassReceiver::resetTrial() {
    minBallDist = 1e9;
    travel = 0;
    ballKicked = false;
    trialStartTime = worldModel->getTime();
    initialized = false;
    initBeamed = true;   // the passer repositions us via the monitor
    resetSkills();
}

SkillType OptimizationBehaviorPassReceiver::selectSkill() {
    if (trial >= numTrials) {
        return SKILL_STAND;
    }
    if (worldModel->getPlayMode() != PM_PLAY_ON || !ballKicked) {
        // Hold position at the rendezvous until the ball is on its way.
        return SKILL_STAND;
    }
    VecPosition ball = worldModel->getBallGroundTruth();
    ball.setZ(0);
    VecPosition me = worldModel->getMyPositionGroundTruth();
    me.setZ(0);
    // Only go for a ball that comes within reach of where we were standing, and
    // give up once we have strayed too far - a short / misdirected pass is simply
    // not collectable and should score badly (rather than being chased forever).
    if (rendezvous.getDistanceTo(ball) > reach ||
            rendezvous.getDistanceTo(me) > reach + 1.0) {
        return SKILL_STAND;
    }
    return goToTarget(ball);
}

void OptimizationBehaviorPassReceiver::scoreTrial() {
    double fitness;
    if (!ballKicked) {
        fitness = -(rendezvous.getMagnitude() + 5.0);   // whiffed
    } else {
        // Closest the ball got to the receiver, plus what it cost the receiver
        // to get there. A perfect pass arrives at the receiver's feet for free.
        fitness = -minBallDist - 0.5 * travel;
        if (minBallDist < catchRadius) {
            fitness += 2.0;                              // pass connected
        }
    }

    cout << "Trial " << trial
         << "  minBallDist=" << minBallDist
         << "  receiverTravel=" << travel
         << "  kicked=" << ballKicked
         << "  fitness=" << fitness << endl;

    totalFitness += fitness;
}

void OptimizationBehaviorPassReceiver::updateFitness() {
    if (trial >= numTrials) {
        writeFitnessToOutputFile(totalFitness / double(numTrials));
        return;
    }

    VecPosition me = worldModel->getMyPositionGroundTruth();
    me.setZ(0);
    VecPosition ball = worldModel->getBallGroundTruth();
    ball.setZ(0);

    if (!ballKicked && ball.getMagnitude() > 0.5) {
        ballKicked = true;
    }
    if (ballKicked) {
        // Distance from the ball to the receiver. Fall back to the fixed
        // rendezvous point when the receiver's own ground-truth read is
        // implausible (transient during a teleport), so a bad read can never
        // blow up the fitness.
        double t = rendezvous.getDistanceTo(me);
        double dRcv = (t < reach + 3.0) ? me.getDistanceTo(ball) : 1e9;
        double dRdv = rendezvous.getDistanceTo(ball);
        double d = (dRcv < dRdv) ? dRcv : dRdv;
        if (d < minBallDist) {
            minBallDist = d;
        }
        if (t < reach + 3.0 && t > travel) {
            travel = t;
        }
    }

    // The passer teleports the ball to exactly the centre at the start of every
    // trial. A rising edge of "ball is dead centre" marks a trial boundary.
    // A whiffed trial leaves the ball near the centre already (no rising edge),
    // so also advance on a generous per-trial timeout.
    bool ballCentred = ball.getMagnitude() < 0.05;
    bool boundary = (ballCentred && !wasBallCentred);
    bool timedOut = (worldModel->getTime() - trialStartTime > 30.0);

    if (boundary || timedOut) {
        if (!sawFirstReset && boundary) {
            sawFirstReset = true;          // first reset: nothing to score yet
        } else {
            scoreTrial();
            trial++;
            resetTrial();
        }
    }
    wasBallCentred = ballCentred;
}

void OptimizationBehaviorPassReceiver::writeFitnessToOutputFile(double fitness) {
    static bool written = false;
    if (written) {
        return;
    }
    written = true;
    LOG(fitness);
    cout << "Receiver mean fitness over " << numTrials << " trials = " << fitness << endl;
    fstream file;
    file.open(outputFile.c_str(), ios::out);
    file << fitness << endl;
    file.close();
}


/*
 *
 *
 * WALK FORWARD OPTIMIZATION AGENT
 *
 *
 *
 */
OptimizationBehaviorWalkForward::
OptimizationBehaviorWalkForward( const std::string teamName,
                                 int uNum,
                                 const map<string, string>& namedParams_,
                                 const string& rsg_,
                                 const string& outputFile_)
    : NaoBehavior( teamName,
                   uNum,
                   namedParams_,
                   rsg_ ),
    outputFile( outputFile_ ) {


    INIT_WAIT = 3;
    run = 0;
    totalWalkDist = 0;

    // Use ground truth localization for behavior
    worldModel->setUseGroundTruthDataForLocalization(true);

    init();
}

void OptimizationBehaviorWalkForward::init() {
    startTime = worldModel->getTime();
    initialized = false;
    initBeamed = false;
    beamChecked = false;
    string msg = "(playMode BeforeKickOff)";
    setMonMessage(msg);
}

void OptimizationBehaviorWalkForward::
beam( double& beamX, double& beamY, double& beamAngle ) {
    beamX = -HALF_FIELD_X+3;
    beamY = 0;
    beamAngle = 0;
}

bool OptimizationBehaviorWalkForward::checkBeam() {
    LOG_STR("Checking whether beam was successful");
    VecPosition meTruth = worldModel->getMyPositionGroundTruth();
    meTruth.setZ(0);
    double beamX, beamY, beamAngle;
    beam(beamX, beamY, beamAngle);
    VecPosition meDesired = VecPosition(beamX, beamY, 0);
    double distance = meTruth.getDistanceTo(meDesired);
    double angleOffset = abs(worldModel->getMyAngDegGroundTruth()-beamAngle);
    if(distance > 0.05 || angleOffset > 5) {
        LOG_STR("Problem with the beam!");
        LOG(distance);
        LOG(meTruth);
        return false;
    }
    beamChecked = true;
    return true;
}

SkillType OptimizationBehaviorWalkForward::
selectSkill() {
    double currentTime = worldModel->getTime();
    if (currentTime-startTime < INIT_WAIT || startTime < 0) {
        return SKILL_STAND;
    }

    return goToTarget(VecPosition(HALF_FIELD_X, 0, 0));
}

void OptimizationBehaviorWalkForward::
updateFitness() {
    static bool written = false;

    if (run == 10) {
        if (!written) {
            double fitness = totalWalkDist/(double)run;
            fstream file;
            file.open(outputFile.c_str(), ios::out );
            file << fitness << endl;
            file.close();
            written = true;
        }
        return;
    }

    if (startTime < 0) {
        init();
        return;
    }

    double currentTime = worldModel->getTime();
    if (currentTime-startTime < INIT_WAIT) {
        return;
    }

    if (!beamChecked) {
        static bool failedLastBeamCheck = false;
        if (!checkBeam()) {
            // Beam failed so reinitialize everything
            if (failedLastBeamCheck) {
                // Probably something bad happened if we failed the beam twice in
                // a row (perhaps the agent can't stand) so give a bad score and
                // move on
                totalWalkDist -= 100;
                run++;
            }
            failedLastBeamCheck = true;
            init();
            return;
        } else {
            failedLastBeamCheck = false;
            // Set playmode to PlayOn to start run and move ball out of the way
            string msg = "(playMode PlayOn) (ball (pos 0 -9 0) (vel 0 0 0))";
            setMonMessage(msg);
        }
    }

    if (currentTime-startTime >= 10.0+INIT_WAIT) {
        VecPosition me = worldModel->getMyPositionGroundTruth();
        double beamX, beamY, beamAngle;
        beam(beamX, beamY, beamAngle);
        VecPosition start = VecPosition(beamX, beamY, 0);

        double walkdist = (me-start).getX();
        cout << "Run " << run << " distance walked: " << walkdist << endl;
        totalWalkDist += walkdist;
        run++;
        init();
    }
}
