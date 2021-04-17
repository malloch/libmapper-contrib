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
#include "mapper/mapper_cpp.h"

#define NUM 4
#define MM "mm"
#define RAD "radians"
#define OUT Direction::OUT

float f0 = 0.f, f1 = 1.f;
float minPos3[3] = {-100.f, -100.f, -100.f};
float maxPos3[3] = {100.f, 100.f, 100.f};
float minDir3[3] = {-M_PI, -M_PI, -M_PI};
float maxDir3[3] = {M_PI, M_PI, M_PI};

int i0 = 0, i1 = 1, inum = NUM;

char str_buffer[128];

using namespace Leap;
using namespace mapper;

const char* fNames[] = {"thumb/", "index/", "middle/", "ring/", "pinky/", ""};
const char* bNames[] = {"metacarpal/", "proximal/", "intermediate/", "distal/"};

class MprBone {
public:
    MprBone(mapper::Device &dev, int fIndex, int bIndex) {
        // init signals
        const char* fName = fNames[fIndex];
        const char* bName = bNames[bIndex];
        int numInst = fIndex == 5 ? NUM : NUM * 5;
        snprintf(str_buffer, 128, "hand/finger/%s%sstart", fName, bName);
        startSig = dev.add_signal(OUT, str_buffer, 3, Type::FLOAT, MM, &minPos3, &maxPos3, &numInst);
        snprintf(str_buffer, 128, "hand/finger/%s%send", fName, bName);
        endSig = dev.add_signal(OUT, str_buffer, 3, Type::FLOAT, MM, &minPos3, &maxPos3, &numInst);
        snprintf(str_buffer, 128, "hand/finger/%s%sdirection", fName, bName);
        directionSig = dev.add_signal(OUT, str_buffer, 3, Type::FLOAT, RAD, &minDir3,
                                      &maxDir3, &numInst);
    }
    void update(const Bone& bone, int id) {
        startSig.instance(id).set_value(bone.prevJoint().toFloatPointer(), 3);
        endSig.instance(id).set_value(bone.nextJoint().toFloatPointer(), 3);
        directionSig.instance(id).set_value(bone.direction().toFloatPointer(), 3);
    }
    void release(int id) {
        startSig.instance(id).release();
        endSig.instance(id).release();
        directionSig.instance(id).release();
    }

private:
    mapper::Signal startSig;
    mapper::Signal endSig;
    mapper::Signal directionSig;
};

class MprFinger {
public:
    MprFinger(mapper::Device &dev, int fingerIndex = 5)
    : metacarpalBone(dev, fingerIndex, 0)
    , proximalBone(dev, fingerIndex, 1)
    , intermediateBone(dev, fingerIndex, 2)
    , distalBone(dev, fingerIndex, 3)
    {}
    ~MprFinger() {}

    void update(const Finger& finger, int id) {
        lengthSig.instance(id).set_value(finger.length());
        widthSig.instance(id).set_value(finger.width());

        for (int i = 0; i < 4; ++i) {
            Bone::Type boneType = static_cast<Bone::Type>(i);
            Bone b = finger.bone(boneType);
            switch (i) {
                case 0: metacarpalBone.update(b, id);      break;
                case 1: proximalBone.update(b, id);        break;
                case 2: intermediateBone.update(b, id);    break;
                case 3: distalBone.update(b, id);          break;
            }
        }
    }
    void update(const Finger& finger) {
        update(finger, finger.id());
    }
    void release(int id) {
        lengthSig.instance(id).release();
        widthSig.instance(id).release();

        metacarpalBone.release(id);
        proximalBone.release(id);
        intermediateBone.release(id);
        distalBone.release(id);
    }

private:
    mapper::Signal lengthSig;
    mapper::Signal widthSig;

    MprBone metacarpalBone;
    MprBone proximalBone;
    MprBone intermediateBone;
    MprBone distalBone;
};

class MprHand {
public:
    MprHand(mapper::Device &dev)
    : anonymousFinger(dev)
    , thumbFinger(dev, 0)
    , indexFinger(dev, 1)
    , middleFinger(dev, 2)
    , ringFinger(dev, 3)
    , pinkyFinger(dev, 4)
    {
        // init signals
        int numInst = NUM;
        isLeftSig = dev.add_signal(OUT, "hand/isLeft", 1, Type::INT32, 0, &i0, &i1, &numInst);
        isRightSig = dev.add_signal(OUT, "hand/isRight", 1, Type::INT32, 0, &i0, &i1, &numInst);

        palmPositionSig = dev.add_signal(OUT, "hand/palm/position", 3, Type::FLOAT, MM,
                                         &minPos3, &maxPos3, &numInst);
        palmPositionStableSig = dev.add_signal(OUT, "hand/palm/position/stable", 3,
                                               Type::FLOAT, MM, &minPos3, &maxPos3, &numInst);
        palmNormalSig = dev.add_signal(OUT, "hand/palm/normal", 3, Type::FLOAT, RAD,
                                       &minDir3, &maxDir3, &numInst);
        palmWidthSig = dev.add_signal(OUT, "hand/palm/width", 3, Type::FLOAT, MM,
                                      &minPos3, &maxPos3, &numInst);

        handDirectionSig = dev.add_signal(OUT, "hand/direction", 3, Type::FLOAT, RAD,
                                          &minDir3, &maxDir3, &numInst);

        sphereCenterSig = dev.add_signal(OUT, "hand/sphere/center", 3, Type::FLOAT, MM,
                                         &minPos3, &maxPos3, &numInst);
        sphereRadiusSig = dev.add_signal(OUT, "hand/palm/position", 3, Type::FLOAT, MM,
                                         &minPos3, &maxPos3, &numInst);
        pinchStrengthSig = dev.add_signal(OUT, "hand/pinch/strength", 1, Type::FLOAT,
                                          "normalized", &f0, &f1, &numInst);
        grabStrengthSig = dev.add_signal(OUT, "hand/grab/strength", 1, Type::FLOAT,
                                         "normalized", &f0, &f1, &numInst);

        timeVisibleSig = dev.add_signal(OUT, "hand/timeVisible", 1, Type::FLOAT,
                                        "seconds", &f0, NULL, &numInst);
        grabStrengthSig = dev.add_signal(OUT, "hand/confidence", 1, Type::FLOAT,
                                         "normalized", &f0, &f1, &numInst);

        handDirectionSig = dev.add_signal(OUT, "arm/direction", 3, Type::FLOAT, RAD,
                                          &minDir3, &maxDir3, &numInst);
        wristPositionSig = dev.add_signal(OUT, "arm/wrist/position", 3, Type::FLOAT,
                                          MM, &minPos3, &maxPos3, &numInst);
        elbowPositionSig = dev.add_signal(OUT, "arm/elbow/position", 3, Type::FLOAT,
                                          MM, &minPos3, &maxPos3, &numInst);
    }
    ~MprHand() {}

    void update(const Frame& frame, const Hand &hand) {
        int id = hand.id();
        // left or right hand?
        int side = hand.isLeft();
        isLeftSig.instance(id).set_value(side);
        side = hand.isRight();
        isRightSig.instance(id).set_value(side);
        palmPositionSig.instance(id).set_value(hand.palmPosition().toFloatPointer(), 3);
        return;

        palmPositionStableSig.instance(id).set_value(hand.stabilizedPalmPosition().toFloatPointer(), 3);
        palmNormalSig.instance(id).set_value(hand.palmNormal().toFloatPointer(), 3);
        palmWidthSig.instance(id).set_value(hand.palmWidth());

        handDirectionSig.instance(id).set_value(hand.direction().toFloatPointer(), 3);

        sphereCenterSig.instance(id).set_value(hand.sphereCenter().toFloatPointer(), 3);
        sphereRadiusSig.instance(id).set_value(hand.sphereRadius());
        pinchStrengthSig.instance(id).set_value(hand.pinchStrength());
        grabStrengthSig.instance(id).set_value(hand.grabStrength());

        timeVisibleSig.instance(id).set_value(hand.timeVisible());

        confidenceSig.instance(id).set_value(hand.confidence());

        Arm arm = hand.arm();
        armDirectionSig.instance(id).set_value(arm.direction().toFloatPointer(), 3);
        wristPositionSig.instance(id).set_value(arm.wristPosition().toFloatPointer(), 3);
        elbowPositionSig.instance(id).set_value(arm.elbowPosition().toFloatPointer(), 3);

        Finger finger;
        // first check if fingers from last fingerlist are still valid
        for (FingerList::const_iterator i = fl.begin(); i != fl.end(); ++i) {
            int id2 = (*i).id();
            finger = frame.finger(id2);
            if (!finger.isValid())
                anonymousFinger.release(id2);
        }

        const FingerList fl = hand.fingers();
        thumbFinger.update(fl[0], id);
        indexFinger.update(fl[1], id);
        middleFinger.update(fl[2], id);
        ringFinger.update(fl[3], id);
        pinkyFinger.update(fl[4], id);

        for (FingerList::const_iterator i = fl.begin(); i != fl.end(); ++i) {
            anonymousFinger.update(*i);
        }
    }
    void release(int id) {
        isLeftSig.instance(id).release();
        isRightSig.instance(id).release();

        palmPositionSig.instance(id).release();
        palmPositionStableSig.instance(id).release();
        palmNormalSig.instance(id).release();
        palmWidthSig.instance(id).release();

        handDirectionSig.instance(id).release();

        sphereCenterSig.instance(id).release();
        sphereRadiusSig.instance(id).release();
        pinchStrengthSig.instance(id).release();
        grabStrengthSig.instance(id).release();

        timeVisibleSig.instance(id).release();

        confidenceSig.instance(id).release();

        armDirectionSig.instance(id).release();
        wristPositionSig.instance(id).release();
        elbowPositionSig.instance(id).release();

        thumbFinger.release(id);
        indexFinger.release(id);
        middleFinger.release(id);
        ringFinger.release(id);
        pinkyFinger.release(id);
    }

private:
    FingerList fl;

    mapper::Signal isLeftSig;
    mapper::Signal isRightSig;

    mapper::Signal palmPositionSig;
    mapper::Signal palmPositionStableSig;
    mapper::Signal palmNormalSig;
    mapper::Signal palmWidthSig;

    mapper::Signal handDirectionSig;

    mapper::Signal sphereCenterSig;
    mapper::Signal sphereRadiusSig;
    mapper::Signal pinchStrengthSig;
    mapper::Signal grabStrengthSig;

    mapper::Signal timeVisibleSig;
    mapper::Signal confidenceSig;

    mapper::Signal armDirectionSig;
    mapper::Signal wristPositionSig;
    mapper::Signal elbowPositionSig;

    MprFinger anonymousFinger;
    MprFinger thumbFinger;
    MprFinger indexFinger;
    MprFinger middleFinger;
    MprFinger ringFinger;
    MprFinger pinkyFinger;
};

class MprTool {
public:
    MprTool(mapper::Device& dev) {
        // init signals
        int numInst = NUM;
        tipPositionSig = dev.add_signal(OUT, "tool/tip/position", 3, Type::FLOAT, MM,
                                        &minPos3, &maxPos3, &numInst);
        directionSig = dev.add_signal(OUT, "tool/direction", 3, Type::FLOAT, MM,
                                      &minDir3, &maxDir3, &numInst);
    }
    ~MprTool() {}

    void update(const Frame& frame, const Tool &tool) {
        int id = tool.id();
        tipPositionSig.instance(id).set_value(tool.tipPosition().toFloatPointer(), 3);
        directionSig.instance(id).set_value(tool.direction().toFloatPointer(), 3);
    }
    void release(int id) {
        tipPositionSig.instance(id).release();
        directionSig.instance(id).release();
    }

private:
    mapper::Signal tipPositionSig;
    mapper::Signal directionSig;
};


class MprLeap {
public:
    MprLeap()
    : dev("leap_motion")
    , hands(dev)
    , tools(dev)
    {
        numHandsSig = dev.add_signal(OUT, "global/numHands", 1, Type::INT32, 0, &i0, &inum);
        numExtendedFingersSig = dev.add_signal(OUT, "global/numExtendedFingers",
                                               1, Type::INT32, 0, &i0);
        frameRateSig = dev.add_signal(OUT, "global/frameRate", 1, Type::FLOAT, "frames/sec", &f0);
    }
    ~MprLeap() {}

    void update(const Frame& frame) {
        // TODO: we could use frame.timestamp() reported by Leap Motion
        // is reported as int64_t in microseconds relative to arbitrary timebase
        // needs conversion: estimate timebase, apply offset

        numHandsSig.set_value(frame.hands().count());
        numExtendedFingersSig.set_value(frame.fingers().extended().count());
        frameRateSig.set_value(frame.currentFramesPerSecond());

        Hand hand;
        // first check if hands from last handlist are still valid
        for (HandList::const_iterator i = hl.begin(); i != hl.end(); ++i) {
            int id = (*i).id();
            hand = frame.hand(id);
            if (!hand.isValid())
                hands.release(id);
        }

        // add or update current hands
        hl = frame.hands();
        for (HandList::const_iterator i = hl.begin(); i != hl.end(); ++i) {
            hand = *i;
            hands.update(frame, hand);
        }

        const ToolList tl = frame.tools();
        for (ToolList::const_iterator i = tl.begin(); i != tl.end(); ++i) {
            const Tool tool = *i;
            tools.update(frame, tool);
        }
        dev.poll(0);
    }

private:
    HandList hl;
    mapper::Device dev;
    mapper::Signal frameRateSig;
    mapper::Signal numHandsSig;
    mapper::Signal numExtendedFingersSig;
    MprHand hands;
    MprTool tools;
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
