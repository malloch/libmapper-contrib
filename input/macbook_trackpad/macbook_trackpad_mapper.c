/* Adaptation of MacBook Multitouch code by  Erling Ellingsen:      *
 * http://www.steike.com/code/multitouch/                           */

#include <math.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mapper/mapper.h>

#define NUMTOUCHES 10

typedef struct { float x,y; } mtPoint;
typedef struct { mtPoint pos,vel; } mtReadout;

typedef struct {
  int frame;
  double timestamp;
  int identifier, state, foo3, foo4;
  mtReadout normalized;
  float size;
  int zero1;
  float angle, majorAxis, minorAxis; // ellipsoid
  mtReadout mm;
  int zero2[2];
  float unk2;
} Finger;

typedef void *MTDeviceRef;
typedef int (*MTContactCallbackFunction)(int,Finger*,int,double,int);

MTDeviceRef MTDeviceCreateDefault();
void MTRegisterContactFrameCallback(MTDeviceRef, MTContactCallbackFunction);
void MTDeviceStart(MTDeviceRef, int); // thanks comex

mapper_device mdev = 0;
mapper_signal countSig = 0;
mapper_signal angleSig = 0;
mapper_signal ellipseSig = 0;
mapper_signal positionSig = 0;
mapper_signal velocitySig = 0;
mapper_signal areaSig = 0;

int done = 0;

int callback(int device, Finger *data, int nFingers, double timestamp, int frame) {
    mapper_timetag_t now;
    mapper_timetag_now(&now);
    mapper_device_start_queue(mdev, now);
    float pair[2];

    printf("Touch count: %d\n", nFingers);
    mapper_signal_update(countSig, &nFingers, 1, now);

    Finger *f = &data[0];
    printf("Frame %7d: ID %2d, Angle %4.2f, ellipse %5.2f x%5.2f, "
           "position %5.3f, %5.3f, vel %6.3f, %6.3f, area %6.3f\n",
           f->frame,
           f->identifier,
           f->angle,
           f->majorAxis,
           f->minorAxis,
           f->normalized.pos.x,
           f->normalized.pos.y,
           f->normalized.vel.x,
           f->normalized.vel.y,
           f->size);

    for (int i = 0; i < nFingers; i++) {
        Finger *f = &data[i];
        printf("               ID %2d, Angle %4.2f, ellipse %5.2f x%5.2f, "
               "position %5.3f, %5.3f, vel %6.3f, %6.3f, area %6.3f\n",
               f->identifier,
               f->angle,
               f->majorAxis,
               f->minorAxis,
               f->normalized.pos.x,
               f->normalized.pos.y,
               f->normalized.vel.x,
               f->normalized.vel.y,
               f->size);

        if (f->size > 0) {
            // update libmapper signals
            mapper_device_start_queue(mdev, now);

            mapper_signal_instance_update(angleSig, f->identifier, &f->angle, 1, now);

            pair[0] = f->majorAxis;
            pair[1] = f->minorAxis;
            mapper_signal_instance_update(ellipseSig, f->identifier, pair, 1, now);

            pair[0] = f->normalized.pos.x;
            pair[1] = f->normalized.pos.y;
            mapper_signal_instance_update(positionSig, f->identifier, pair, 1, now);

            pair[0] = f->normalized.vel.x;
            pair[1] = f->normalized.vel.y;
            mapper_signal_instance_update(velocitySig, f->identifier, pair, 1, now);

            mapper_signal_instance_update(areaSig, f->identifier, &f->size, 1, now);

            mapper_device_send_queue(mdev, now);
        }
        else {
            mapper_signal_instance_release(angleSig, f->identifier, now);
            mapper_signal_instance_release(ellipseSig, f->identifier, now);
            mapper_signal_instance_release(positionSig, f->identifier, now);
            mapper_signal_instance_release(velocitySig, f->identifier, now);
            mapper_signal_instance_release(areaSig, f->identifier, now);
        }
    }

    mapper_device_send_queue(mdev, now);
    return 0;
}

void add_signals()
{
    int mini = 0, maxi = NUMTOUCHES;
    countSig = mapper_device_add_signal(mdev, MAPPER_DIR_OUTGOING, 1,
                                        "touch/count", 1, 'i', NULL,
                                        &mini, &maxi, 0, 0);

    float minf[2] = {0.f, 0.f};
    float maxf[2] = {M_PI, M_PI};
    angleSig = mapper_device_add_signal(mdev, MAPPER_DIR_OUTGOING, NUMTOUCHES,
                                        "touch/angle", 1, 'f', "radians",
                                        minf, maxf, 0, 0);

    ellipseSig = mapper_device_add_signal(mdev, MAPPER_DIR_OUTGOING, NUMTOUCHES,
                                          "touch/ellipse", 2, 'f', "mm",
                                          0, 0, 0, 0);

    minf[0] = 0.f; minf[1] = 0.f;
    maxf[0] = 1.f; maxf[1] = 1.f;
    positionSig = mapper_device_add_signal(mdev, MAPPER_DIR_OUTGOING, NUMTOUCHES,
                                           "touch/position", 2, 'f',
                                           "normalized", minf, maxf, 0, 0);

    velocitySig = mapper_device_add_signal(mdev, MAPPER_DIR_OUTGOING, NUMTOUCHES,
                                           "touch/velocity", 2, 'f',
                                           "normalized", 0, 0, 0, 0);

    areaSig = mapper_device_add_signal(mdev, MAPPER_DIR_OUTGOING, NUMTOUCHES,
                                       "touch/area", 1, 'f', 0, 0, 0, 0, 0);
}

void ctrlc(int sig)
{
    done = 1;
}

int main() {
    signal(SIGINT, ctrlc);

    mdev = mapper_device_new("touchpad", 0, 0);
    add_signals();
    while (!mapper_device_ready(mdev)) {
        mapper_device_poll(mdev, 0);
    }
    
    MTDeviceRef dev = MTDeviceCreateDefault();
    MTRegisterContactFrameCallback(dev, callback);
    MTDeviceStart(dev, 0);
    printf("Ctrl-C to abort\n");

    while (!done) {
        mapper_device_poll(mdev, 10);
    }

    printf("freeing libmapper device... ");
    mapper_device_free(mdev);
    printf("done.\n");
    return 0;
}
