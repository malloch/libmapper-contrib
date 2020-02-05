/******************************************************************************\
* Copyright (C) 2012-2014 Leap Motion, Inc. All rights reserved.               *
* Leap Motion proprietary and confidential. Not for distribution.              *
* Use subject to the terms of the Leap Motion SDK Agreement available at       *
* https://developer.leapmotion.com/sdk_agreement, or another agreement         *
* between Leap Motion and you, your company or other organization.             *
\******************************************************************************/

#include <iostream>
#include <cstring>
#include "Leap.h"
#include "mpr/mpr_cpp.h"

#define NUM 4
#define MM "mm"
#define RAD "radians"
#define OUT MPR_DIR_OUT

float f0 = 0.f, f1 = 1.f;
float minPos3[3] = {-100.f, -100.f, -100.f};
float maxPos3[3] = {100.f, 100.f, 100.f};
float minDir3[3] = {-M_PI, -M_PI, -M_PI};
float maxDir3[3] = {M_PI, M_PI, M_PI};

int i0 = 0, i1 = 1, inum = NUM;

char str_buffer[128];

using namespace Leap;

const char* fNames[] = {"thumb/", "index/", "middle/", "ring/", "pinky/", ""};
const char* bNames[] = {"metacarpal/", "proximal/", "intermediate/", "distal/"};

class MprBone {
public:
    MprBone(mpr::Device &dev, int fIndex, int bIndex) {
        // init signals
        const char* fName = fNames[fIndex];
        const char* bName = bNames[bIndex];
        int numInst = fIndex == 5 ? NUM : NUM * 5;
        snprintf(str_buffer, 128, "hand/finger/%s%sstart", fName, bName);
        startSig = dev.add_sig(OUT, str_buffer, 3, MPR_FLT, MM, &minPos3,
                               &maxPos3, &numInst);
        snprintf(str_buffer, 128, "hand/finger/%s%send", fName, bName);
        endSig = dev.add_sig(OUT, str_buffer, 3, MPR_FLT, MM, &minPos3, &maxPos3,
                             &numInst);
        snprintf(str_buffer, 128, "hand/finger/%s%sdirection", fName, bName);
        directionSig = dev.add_sig(OUT, str_buffer, 3, MPR_FLT, RAD,
                                   &minDir3, &maxDir3, &numInst);
    }
    void update(const Bone& bone, int id, mpr::Time& time) {
        startSig.instance(id).set_value(bone.prevJoint(), time);
        endSig.instance(id).set_value(bone.nextJoint(), time);
        directionSig.instance(id).set_value(bone.direction(), time);
    }
    void release(int id, mpr::Time& time) {
        startSig.instance(id).release(time);
        endSig.instance(id).release(time);
        directionSig.instance(id).release(time);
    }

private:
    mpr::Signal startSig;
    mpr::Signal endSig;
    mpr::Signal directionSig;
};

class MprFinger {
public:
    MprFinger(mpr::Device &dev, int fingerIndex = 5)
    : metacarpalBone(dev, fingerIndex, 0)
    , proximalBone(dev, fingerIndex, 1)
    , intermediateBone(dev, fingerIndex, 2)
    , distalBone(dev, fingerIndex, 3)
    {}
    ~MprFinger() {}

    void update(const Finger& finger, int id, mpr::Time& time) {
        lengthSig.instance(id).set_value(finger.length(), time);
        widthSig.instance(id).set_value(finger.width(), time);

        for (int i = 0; i < 4; ++i) {
            Bone::Type boneType = static_cast<Bone::Type>(i);
            Bone b = finger.bone(boneType);
            switch (i) {
                case 0: metacarpalBone.update(b, id, time);      break;
                case 1: proximalBone.update(b, id, time);        break;
                case 2: intermediateBone.update(b, id, time);    break;
                case 3: distalBone.update(b, id, time);          break;
            }
        }
    }
    void update(const Finger& finger, mpr::Time& time) {
        update(finger, finger.id(), time);
    }
    void release(int id, mpr::Time& time) {
        lengthSig.instance(id).release(time);
        widthSig.instance(id).release(time);

        metacarpalBone.release(id, time);
        proximalBone.release(id, time);
        intermediateBone.release(id, time);
        distalBone.release(id, time);
    }

private:
    mpr::Signal lengthSig;
    mpr::Signal widthSig;

    MprBone metacarpalBone;
    MprBone proximalBone;
    MprBone intermediateBone;
    MprBone distalBone;
};

class MprHand {
public:
    MprHand(mpr::Device &dev)
    : anonymousFinger(dev)
    , thumbFinger(dev, 0)
    , indexFinger(dev, 1)
    , middleFinger(dev, 2)
    , ringFinger(dev, 3)
    , pinkyFinger(dev, 4)
    {
        // init signals
        int numInst = NUM;
        isLeftSig = dev.add_sig(OUT, "hand/isLeft", 1, MPR_INT32, 0, &i0, &i1, &numInst);
        isRightSig = dev.add_sig(OUT, "hand/isRight", 1, MPR_INT32, 0, &i0, &i1, &numInst);

        palmPositionSig = dev.add_sig(OUT, "hand/palm/position", 3, MPR_FLT, MM,
                                      &minPos3, &maxPos3, &numInst);
        palmPositionStableSig = dev.add_sig(OUT, "hand/palm/position/stable", 3,
                                            MPR_FLT, MM, &minPos3, &maxPos3, &numInst);
        palmNormalSig = dev.add_sig(OUT, "hand/palm/normal", 3, MPR_FLT, RAD,
                                    &minDir3, &maxDir3, &numInst);
        palmWidthSig = dev.add_sig(OUT, "hand/palm/width", 3, MPR_FLT, MM,
                                   &minPos3, &maxPos3, &numInst);

        handDirectionSig = dev.add_sig(OUT, "hand/direction", 3, MPR_FLT, RAD,
                                       &minDir3, &maxDir3, &numInst);

        sphereCenterSig = dev.add_sig(OUT, "hand/sphere/center", 3, MPR_FLT, MM,
                                      &minPos3, &maxPos3, &numInst);
        sphereRadiusSig = dev.add_sig(OUT, "hand/palm/position", 3, MPR_FLT, MM,
                                      &minPos3, &maxPos3, &numInst);
        pinchStrengthSig = dev.add_sig(OUT, "hand/pinch/strength", 1, MPR_FLT,
                                      "normalized", &f0, &f1, &numInst);
        grabStrengthSig = dev.add_sig(OUT, "hand/grab/strength", 1, MPR_FLT,
                                      "normalized", &f0, &f1, &numInst);

        timeVisibleSig = dev.add_sig(OUT, "hand/timeVisible", 1, MPR_FLT,
                                     "seconds", &f0, NULL, &numInst);
        grabStrengthSig = dev.add_sig(OUT, "hand/confidence", 1, MPR_FLT,
                                      "normalized", &f0, &f1, &numInst);

        handDirectionSig = dev.add_sig(OUT, "arm/direction", 3, MPR_FLT, RAD,
                                       &minDir3, &maxDir3, &numInst);
        wristPositionSig = dev.add_sig(OUT, "arm/wrist/position", 3, MPR_FLT,
                                       MM, &minPos3, &maxPos3, &numInst);
        elbowPositionSig = dev.add_sig(OUT, "arm/elbow/position", 3, MPR_FLT,
                                       MM, &minPos3, &maxPos3, &numInst);
    }
    ~MprHand() {}

    void update(const Frame& frame, const Hand &hand, mpr::Time& time) {
        int id = hand.id();
        // left or right hand?
        isLeftSig.instance(id).set_value(hand.isLeft(), time);
        isRightSig.instance(id).set_value(hand.isRight(), time);

        palmPositionSig.instance(id).set_value(hand.palmPosition(), time);
        return;

        palmPositionStableSig.instance(id).set_value(hand.stabilizedPalmPosition(), time);
        palmNormalSig.instance(id).set_value(hand.palmNormal(), time);
        palmWidthSig.instance(id).set_value(hand.palmWidth(), time);

        handDirectionSig.instance(id).set_value(hand.direction(), time);

        sphereCenterSig.instance(id).set_value(hand.sphereCenter(), time);
        sphereRadiusSig.instance(id).set_value(hand.sphereRadius(), time);
        pinchStrengthSig.instance(id).set_value(hand.pinchStrength(), time);
        grabStrengthSig.instance(id).set_value(hand.grabStrength(), time);

        timeVisibleSig.instance(id).set_value(hand.timeVisible(), time);

        confidenceSig.instance(id).set_value(hand.confidence(), time);

        Arm arm = hand.arm();
        armDirectionSig.instance(id).set_value(arm.direction(), time);
        wristPositionSig.instance(id).set_value(arm.wristPosition(), time);
        elbowPositionSig.instance(id).set_value(arm.elbowPosition(), time);

        Finger finger;
        // first check if fingers from last fingerlist are still valid
        for (FingerList::const_iterator i = fl.begin(); i != fl.end(); ++i) {
            int id2 = (*i).id();
            finger = frame.finger(id2);
            if (!finger.isValid())
                anonymousFinger.release(id2, time);
        }

        const FingerList fl = hand.fingers();
        thumbFinger.update(fl[0], id, time);
        indexFinger.update(fl[1], id, time);
        middleFinger.update(fl[2], id, time);
        ringFinger.update(fl[3], id, time);
        pinkyFinger.update(fl[4], id, time);
        for (FingerList::const_iterator i = fl.begin(); i != fl.end(); ++i) {
            anonymousFinger.update(*i, time);
        }
    }
    void release(int id, mpr::Time& time) {
        isLeftSig.instance(id).release(time);
        isRightSig.instance(id).release(time);

        palmPositionSig.instance(id).release(time);
        palmPositionStableSig.instance(id).release(time);
        palmNormalSig.instance(id).release(time);
        palmWidthSig.instance(id).release(time);

        handDirectionSig.instance(id).release(time);

        sphereCenterSig.instance(id).release(time);
        sphereRadiusSig.instance(id).release(time);
        pinchStrengthSig.instance(id).release(time);
        grabStrengthSig.instance(id).release(time);

        timeVisibleSig.instance(id).release(time);

        confidenceSig.instance(id).release(time);

        armDirectionSig.instance(id).release(time);
        wristPositionSig.instance(id).release(time);
        elbowPositionSig.instance(id).release(time);

        thumbFinger.release(id, time);
        indexFinger.release(id, time);
        middleFinger.release(id, time);
        ringFinger.release(id, time);
        pinkyFinger.release(id, time);
    }

private:
    FingerList fl;

    mpr::Signal isLeftSig;
    mpr::Signal isRightSig;

    mpr::Signal palmPositionSig;
    mpr::Signal palmPositionStableSig;
    mpr::Signal palmNormalSig;
    mpr::Signal palmWidthSig;

    mpr::Signal handDirectionSig;

    mpr::Signal sphereCenterSig;
    mpr::Signal sphereRadiusSig;
    mpr::Signal pinchStrengthSig;
    mpr::Signal grabStrengthSig;

    mpr::Signal timeVisibleSig;
    mpr::Signal confidenceSig;

    mpr::Signal armDirectionSig;
    mpr::Signal wristPositionSig;
    mpr::Signal elbowPositionSig;

    MprFinger anonymousFinger;
    MprFinger thumbFinger;
    MprFinger indexFinger;
    MprFinger middleFinger;
    MprFinger ringFinger;
    MprFinger pinkyFinger;
};

class MprTool {
public:
    MprTool(mpr::Device& dev) {
        // init signals
        int numInst = NUM;
        tipPositionSig = dev.add_sig(OUT, "tool/tip/position", 3, MPR_FLT, MM,
                                     &minPos3, &maxPos3, &numInst);
        directionSig = dev.add_sig(OUT, "tool/direction", 3, MPR_FLT, MM,
                                   &minDir3, &maxDir3, &numInst);
    }
    ~MprTool() {}

    void update(const Frame& frame, const Tool &tool, mpr::Time& time) {
        int id = tool.id();
        tipPositionSig.instance(id).set_value(tool.tipPosition(), time);
        directionSig.instance(id).set_value(tool.direction(), time);
    }
    void release(int id, mpr::Time& time) {
        tipPositionSig.instance(id).release(time);
        directionSig.instance(id).release(time);
    }

private:
    mpr::Signal tipPositionSig;
    mpr::Signal directionSig;
};


class MprLeap {
public:
    MprLeap()
    : dev("leap_motion")
    , hands(dev)
    , tools(dev)
    {
        numHandsSig = dev.add_sig(OUT, "global/numHands", 1, MPR_INT32, 0,
                                  &i0, &inum);
        numExtendedFingersSig = dev.add_sig(OUT, "global/numExtendedFingers",
                                            1, MPR_INT32, 0, &i0);
        frameRateSig = dev.add_sig(OUT, "global/frameRate",
                                   1, MPR_FLT, "frames/sec", &f0);
    }
    ~MprLeap() {}

    void update(const Frame& frame) {
        // TODO: we could use frame.timestamp() reported by Leap Motion
        // is reported as int64_t in microseconds relative to arbitrary timebase
        // needs conversion: estimate timebase, apply offset
        time.now();
        dev.start_queue(time);

        numHandsSig.set_value(frame.hands().count(), time);
        numExtendedFingersSig.set_value(frame.fingers().extended().count(), time);
        frameRateSig.set_value(frame.currentFramesPerSecond(), time);

        Hand hand;
        // first check if hands from last handlist are still valid
        for (HandList::const_iterator i = hl.begin(); i != hl.end(); ++i) {
            int id = (*i).id();
            hand = frame.hand(id);
            if (!hand.isValid())
                hands.release(id, time);
        }
        // add or update current hands
        hl = frame.hands();
        for (HandList::const_iterator i = hl.begin(); i != hl.end(); ++i) {
            hand = *i;
            hands.update(frame, hand, time);
        }

        const ToolList tl = frame.tools();
        for (ToolList::const_iterator i = tl.begin(); i != tl.end(); ++i) {
            const Tool tool = *i;
            tools.update(frame, tool, time);
        }
        dev.send_queue(time);
        dev.poll(1);
    }

private:
    HandList hl;
    mpr::Device dev;
    mpr::Signal frameRateSig;
    mpr::Signal numHandsSig;
    mpr::Signal numExtendedFingersSig;
    MprHand hands;
    MprTool tools;
    mpr::Time time;
};

class SampleListener : public Listener {
public:
    virtual void onInit(const Controller&);
    virtual void onConnect(const Controller&);
    virtual void onDisconnect(const Controller&);
    virtual void onExit(const Controller&);
    virtual void onFrame(const Controller&);
    virtual void onFocusGained(const Controller&);
    virtual void onFocusLost(const Controller&);
    virtual void onDeviceChange(const Controller&);
    virtual void onServiceConnect(const Controller&);
    virtual void onServiceDisconnect(const Controller&);

private:
};

MprLeap mprLeap;

void SampleListener::onInit(const Controller& controller) {
  std::cout << "Initialized" << std::endl;
}

void SampleListener::onConnect(const Controller& controller) {
    std::cout << "Connected" << std::endl;
}

void SampleListener::onDisconnect(const Controller& controller) {
  // Note: not dispatched when running in a debugger.
  std::cout << "Disconnected" << std::endl;
}

void SampleListener::onExit(const Controller& controller) {
  std::cout << "Exited" << std::endl;
}

void SampleListener::onFrame(const Controller& controller) {
    const Frame frame = controller.frame();

    std::cout << "\e[2J\e[0;0H";
    std::cout << "==== LEAP MOTION MAPPER ===="  << std::endl
            << "Frame id: " << frame.id() << std::endl
            << "Timestamp: " << frame.timestamp() << std::endl
            << "FrameRate: " << frame.currentFramesPerSecond() << std::endl
            << "Hands: " << frame.hands().count() << std::endl
            << "Extended fingers: " << frame.fingers().extended().count() << std::endl
            << "Tools: " << frame.tools().count() << std::endl;

    mprLeap.update(frame);
}

void SampleListener::onFocusGained(const Controller& controller) {
  std::cout << "Focus Gained" << std::endl;
}

void SampleListener::onFocusLost(const Controller& controller) {
  std::cout << "Focus Lost" << std::endl;
}

void SampleListener::onDeviceChange(const Controller& controller) {
  std::cout << "Device Changed" << std::endl;
  const DeviceList devices = controller.devices();

  for (int i = 0; i < devices.count(); ++i) {
    std::cout << "id: " << devices[i].toString() << std::endl;
    std::cout << "  isStreaming: " << (devices[i].isStreaming() ? "true" : "false") << std::endl;
  }
}

void SampleListener::onServiceConnect(const Controller& controller) {
  std::cout << "Service Connected" << std::endl;
}

void SampleListener::onServiceDisconnect(const Controller& controller) {
  std::cout << "Service Disconnected" << std::endl;
}

int main(int argc, char** argv) {
    // Create a sample listener and controller
    SampleListener listener;
    Controller controller;

    // Have the sample listener receive events from the controller
    controller.addListener(listener);

    // Enable receiving events when application is not in focus
    controller.setPolicy(Leap::Controller::POLICY_BACKGROUND_FRAMES);

    // Keep this process running until Enter is pressed
    std::cout << "Press Enter to quit..." << std::endl;
    std::cin.get();

    // Remove the sample listener
    controller.removeListener(listener);

  return 0;
}
