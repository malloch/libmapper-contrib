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
#define OUT MAPPER_DIR_OUTGOING

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
    MprBone(mapper::Device &dev, int fIndex, int bIndex) {
        // init signals
        const char* fName = fNames[fIndex];
        const char* bName = bNames[bIndex];
        int numInst = fIndex == 5 ? NUM : NUM * 5;
        snprintf(str_buffer, 128, "hand/finger/%s%sstart", fName, bName);
        startSig = dev.add_signal(OUT, numInst, str_buffer,
                                  3, 'f', MM, &minPos3, &maxPos3);
        snprintf(str_buffer, 128, "hand/finger/%s%send", fName, bName);
        endSig = dev.add_signal(OUT, numInst, str_buffer,
                                3, 'f', MM, &minPos3, &maxPos3);
        snprintf(str_buffer, 128, "hand/finger/%s%sdirection", fName, bName);
        directionSig = dev.add_signal(OUT, numInst, str_buffer, 3,
                                      'f', RAD, &minDir3, &maxDir3);
    }
    void update(const Bone& bone, int id, mapper::Timetag& timetag) {
        startSig.instance(id).update(bone.prevJoint(), timetag);
        endSig.instance(id).update(bone.nextJoint(), timetag);
        directionSig.instance(id).update(bone.direction(), timetag);
    }
    void release(int id, mapper::Timetag& timetag) {
        startSig.instance(id).release(timetag);
        endSig.instance(id).release(timetag);
        directionSig.instance(id).release(timetag);
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

    void update(const Finger& finger, int id, mapper::Timetag& timetag) {
        lengthSig.instance(id).update(finger.length(), timetag);
        widthSig.instance(id).update(finger.width(), timetag);

        for (int i = 0; i < 4; ++i) {
            Bone::Type boneType = static_cast<Bone::Type>(i);
            Bone b = finger.bone(boneType);
            switch (i) {
                case 0: metacarpalBone.update(b, id, timetag);      break;
                case 1: proximalBone.update(b, id, timetag);        break;
                case 2: intermediateBone.update(b, id, timetag);    break;
                case 3: distalBone.update(b, id, timetag);          break;
            }
        }
    }
    void update(const Finger& finger, mapper::Timetag& timetag) {
        update(finger, finger.id(), timetag);
    }
    void release(int id, mapper::Timetag& timetag) {
        lengthSig.instance(id).release(timetag);
        widthSig.instance(id).release(timetag);

        metacarpalBone.release(id, timetag);
        proximalBone.release(id, timetag);
        intermediateBone.release(id, timetag);
        distalBone.release(id, timetag);
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
        isLeftSig = dev.add_signal(OUT, NUM, "hand/isLeft", 1, 'i', 0, &i0, &i1);
        isRightSig = dev.add_signal(OUT, NUM, "hand/isRight", 1, 'i', 0, &i0, &i1);

        palmPositionSig = dev.add_signal(OUT, NUM, "hand/palm/position",
                                         3, 'f', MM, &minPos3, &maxPos3);
        palmPositionStableSig = dev.add_signal(OUT, NUM, "hand/palm/position/stable",
                                        3, 'f', MM, &minPos3, &maxPos3);
        palmNormalSig = dev.add_signal(OUT, NUM, "hand/palm/normal",
                                       3, 'f', RAD, &minDir3, &maxDir3);
        palmWidthSig = dev.add_signal(OUT, NUM, "hand/palm/width",
                                      3, 'f', MM, &minPos3, &maxPos3);

        handDirectionSig = dev.add_signal(OUT, NUM, "hand/direction",
                                          3, 'f', RAD, &minDir3, &maxDir3);

        sphereCenterSig = dev.add_signal(OUT, NUM, "hand/sphere/center",
                                         3, 'f', MM, &minPos3, &maxPos3);
        sphereRadiusSig = dev.add_signal(OUT, NUM, "hand/palm/position",
                                         3, 'f', MM, &minPos3, &maxPos3);
        pinchStrengthSig = dev.add_signal(OUT, NUM, "hand/pinch/strength",
                                          1, 'f', "normalized", &f0, &f1);
        grabStrengthSig = dev.add_signal(OUT, NUM, "hand/grab/strength",
                                         1, 'f', "normalized", &f0, &f1);

        timeVisibleSig = dev.add_signal(OUT, NUM, "hand/timeVisible",
                                         1, 'f', "seconds", &f0, NULL);
        grabStrengthSig = dev.add_signal(OUT, NUM, "hand/confidence",
                                         1, 'f', "normalized", &f0, &f1);

        handDirectionSig = dev.add_signal(OUT, NUM, "arm/direction",
                                          3, 'f', RAD, &minDir3, &maxDir3);
        wristPositionSig = dev.add_signal(OUT, NUM, "arm/wrist/position",
                                          3, 'f', MM, &minPos3, &maxPos3);
        elbowPositionSig = dev.add_signal(OUT, NUM, "arm/elbow/position",
                                          3, 'f', MM, &minPos3, &maxPos3);
    }
    ~MprHand() {}

    void update(const Frame& frame, const Hand &hand, mapper::Timetag& timetag) {
        int id = hand.id();
        // left or right hand?
        isLeftSig.instance(id).update(hand.isLeft(), timetag);
        isRightSig.instance(id).update(hand.isRight(), timetag);

        palmPositionSig.instance(id).update(hand.palmPosition(), timetag);
        return;

        palmPositionStableSig.instance(id).update(hand.stabilizedPalmPosition(), timetag);
        palmNormalSig.instance(id).update(hand.palmNormal(), timetag);
        palmWidthSig.instance(id).update(hand.palmWidth(), timetag);

        handDirectionSig.instance(id).update(hand.direction(), timetag);

        sphereCenterSig.instance(id).update(hand.sphereCenter(), timetag);
        sphereRadiusSig.instance(id).update(hand.sphereRadius(), timetag);
        pinchStrengthSig.instance(id).update(hand.pinchStrength(), timetag);
        grabStrengthSig.instance(id).update(hand.grabStrength(), timetag);

        timeVisibleSig.instance(id).update(hand.timeVisible(), timetag);

        confidenceSig.instance(id).update(hand.confidence(), timetag);

        Arm arm = hand.arm();
        armDirectionSig.instance(id).update(arm.direction(), timetag);
        wristPositionSig.instance(id).update(arm.wristPosition(), timetag);
        elbowPositionSig.instance(id).update(arm.elbowPosition(), timetag);

        Finger finger;
        // first check if fingers from last fingerlist are still valid
        for (FingerList::const_iterator i = fl.begin(); i != fl.end(); ++i) {
            int id2 = (*i).id();
            finger = frame.finger(id2);
            if (!finger.isValid())
                anonymousFinger.release(id2, timetag);
        }

        const FingerList fl = hand.fingers();
        thumbFinger.update(fl[0], id, timetag);
        indexFinger.update(fl[1], id, timetag);
        middleFinger.update(fl[2], id, timetag);
        ringFinger.update(fl[3], id, timetag);
        pinkyFinger.update(fl[4], id, timetag);
        for (FingerList::const_iterator i = fl.begin(); i != fl.end(); ++i) {
            anonymousFinger.update(*i, timetag);
        }
    }
    void release(int id, mapper::Timetag& timetag) {
        isLeftSig.instance(id).release(timetag);
        isRightSig.instance(id).release(timetag);

        palmPositionSig.instance(id).release(timetag);
        palmPositionStableSig.instance(id).release(timetag);
        palmNormalSig.instance(id).release(timetag);
        palmWidthSig.instance(id).release(timetag);

        handDirectionSig.instance(id).release(timetag);

        sphereCenterSig.instance(id).release(timetag);
        sphereRadiusSig.instance(id).release(timetag);
        pinchStrengthSig.instance(id).release(timetag);
        grabStrengthSig.instance(id).release(timetag);

        timeVisibleSig.instance(id).release(timetag);

        confidenceSig.instance(id).release(timetag);

        armDirectionSig.instance(id).release(timetag);
        wristPositionSig.instance(id).release(timetag);
        elbowPositionSig.instance(id).release(timetag);

        thumbFinger.release(id, timetag);
        indexFinger.release(id, timetag);
        middleFinger.release(id, timetag);
        ringFinger.release(id, timetag);
        pinkyFinger.release(id, timetag);
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
        tipPositionSig = dev.add_signal(OUT, NUM, "tool/tip/position",
                                        3, 'f', MM, &minPos3, &maxPos3);
        directionSig = dev.add_signal(OUT, NUM, "tool/direction",
                                      3, 'f', MM, &minDir3, &maxDir3);
    }
    ~MprTool() {}

    void update(const Frame& frame, const Tool &tool, mapper::Timetag& timetag) {
        int id = tool.id();
        tipPositionSig.instance(id).update(tool.tipPosition(), timetag);
        directionSig.instance(id).update(tool.direction(), timetag);
    }
    void release(int id, mapper::Timetag& timetag) {
        tipPositionSig.instance(id).release(timetag);
        directionSig.instance(id).release(timetag);
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
        numHandsSig = dev.add_signal(OUT, 1, "global/numHands",
                                     1, 'i', 0, &i0, &inum);
        numExtendedFingersSig = dev.add_signal(OUT, 1, "global/numExtendedFingers",
                                               1, 'i', 0, &i0);
        frameRateSig = dev.add_signal(OUT, 1, "global/frameRate",
                                      1, 'f', "frames/sec", &f0);
    }
    ~MprLeap() {}

    void update(const Frame& frame) {
        // TODO: we could use frame.timestamp() reported by Leap Motion
        // is reported as int64_t in microseconds relative to arbitrary timebase
        // needs conversion: estimate timebase, apply offset
        timetag.now();
        dev.start_queue(timetag);

        numHandsSig.update(frame.hands().count(), timetag);
        numExtendedFingersSig.update(frame.fingers().extended().count(), timetag);
        frameRateSig.update(frame.currentFramesPerSecond(), timetag);

        Hand hand;
        // first check if hands from last handlist are still valid
        for (HandList::const_iterator i = hl.begin(); i != hl.end(); ++i) {
            int id = (*i).id();
            hand = frame.hand(id);
            if (!hand.isValid())
                hands.release(id, timetag);
        }
        // add or update current hands
        hl = frame.hands();
        for (HandList::const_iterator i = hl.begin(); i != hl.end(); ++i) {
            hand = *i;
            hands.update(frame, hand, timetag);
        }

        const ToolList tl = frame.tools();
        for (ToolList::const_iterator i = tl.begin(); i != tl.end(); ++i) {
            const Tool tool = *i;
            tools.update(frame, tool, timetag);
        }
        dev.send_queue(timetag);
        dev.poll(1);
    }

private:
    HandList hl;
    mapper::Device dev;
    mapper::Signal frameRateSig;
    mapper::Signal numHandsSig;
    mapper::Signal numExtendedFingersSig;
    MprHand hands;
    MprTool tools;
    mapper::Timetag timetag;
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
