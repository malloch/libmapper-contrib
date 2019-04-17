#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <math.h>
#include <lo/lo.h>
#include <mapper/mapper_cpp.h>

#define MAX_TOUCH 10
#define MAX_OBJECT 10
#define TWOPI (M_PI * 2)

lo_server server;
mapper::Device dev("tuio");

// touch signals
mapper::Signal touchCount = 0;
mapper::Signal touchPosition = 0;
mapper::Signal touchAggregateTranslation = 0;
mapper::Signal touchAggregateGrowth = 0;
mapper::Signal touchAggregateRotation = 0;

// object signals
mapper::Signal objectType = 0;
mapper::Signal objectPosition = 0;
mapper::Signal objectAngle = 0;

mapper::Timetag tt_queue;

int done = 0;

class Point
{
public:
    Point(float _x, float _y) { set(_x, _y); }
    Point() {}
    Point& set(float _x, float _y) { x = _x; y = _y; return (*this); }
    Point operator+(const Point& p) const { return Point(x+p.x, y+p.y); }
    Point operator-(const Point& p) const { return Point(x-p.x, y-p.y); }
    Point& operator+=(const Point& p) { x += p.x; y += p.y; return (*this); }
    Point& operator-=(const Point& p) { x -= p.x; y -= p.y; return (*this); }
    Point& operator/=(const Point& p) { x /= p.x; y /= p.y; return (*this); }
    Point& operator/=(float divisor) { x /= divisor; y /= divisor; return (*this); }
    Point& operator=(const Point& p) { x = p.x; y = p.y; return (*this); }

    union {
        struct {
            float x;
            float y;
        };
        float coord[2];
    };
    private:
};

class Box
{
public:
    Box(float minx, float miny, float maxx, float maxy) : min(minx, miny), max(maxx, maxy) {}
    Box(float x, float y) : min(x, y), max(x, y) {}
    Point min;
    Point max;

    Box& updateExtrema(Point p) {
        if (p.x < min.x) min.x = p.x;
        if (p.x > max.x) max.x = p.x;
        if (p.y < min.y) min.y = p.y;
        if (p.y > max.y) max.y = p.y;
        return (*this);
    }

    float width() { return max.x > min.x ? max.x - min.x : min.x - max.x; }
    float height() { return max.y > min.y ? max.y - min.y : min.y - max.y; }
};

class Touch
{
public:
    Touch() { sessionId = -1; }
    int sessionId;
    Point currentPosition;
    Point lastPosition;
};

class Object
{
public:
    Object() { sessionId = -1; }
    int sessionId;
    int objectId;
    Point currentPosition;
    Point lastPosition;
    float angle;
};

Touch touches[MAX_TOUCH];
Object objects[MAX_OBJECT];

void errorHandler(int num, const char *msg, const char *where)
{
    printf("liblo server error %d in path %s: %s\n", num, where, msg);
}

int bundleStartHandler(lo_timetag tt, void *user_data)
{
    tt_queue.now();
    dev.start_queue(tt_queue);
    return 0;
}

int bundleEndHandler(void *user_data)
{
    // calculate aggregate centre
    int i, count = 0;

    Point meanPosition(0.f, 0.f), deltaPosition(0.f, 0.f);
    Box lastBox(1.f, 0.f), currentBox(1.f, 0.f);

    for (i = 0; i < MAX_TOUCH; i++) {
        Touch t = touches[i];
        if (t.sessionId == -1)
            continue;
        meanPosition += t.currentPosition;
        deltaPosition += (t.currentPosition - t.lastPosition);
        ++count;

        lastBox.updateExtrema(t.lastPosition);
        currentBox.updateExtrema(t.currentPosition);
    }
    if (count > 0) {
        meanPosition /= count;

        // calculate aggregate translation
        deltaPosition /= count;
        touchAggregateTranslation.update(deltaPosition.coord, tt_queue);

        // find bounding box dimensions and calculate growth
        float dw = currentBox.width() - lastBox.width();
        float dh = currentBox.height() - lastBox.height();
        float growth = sqrtf(dw*dw + dh*dh);
        touchAggregateGrowth.update(growth, tt_queue);

        // find aggregate rotation
        float aggrot = 0;
        for (i = 0; i < MAX_TOUCH; i++) {
            Point p = touches[i].lastPosition - meanPosition;
            float a1 = atan2f(p.y, p.x);
            p = touches[i].currentPosition - meanPosition;
            float a2 = atan2f(p.y, p.x);
            float diff = a2 - a1;
            if (diff > M_PI)
                diff -= TWOPI;
            else if (diff < -M_PI)
                diff += TWOPI;
            aggrot += diff;
        }
        aggrot /= count;
        touchAggregateRotation.update(aggrot, tt_queue);

        printf("TRANSLATION: [%f, %f], GROWTH: %f, ROTATION: %f\n",
               deltaPosition.x, deltaPosition.y, growth, aggrot);
    }

    dev.send_queue(tt_queue);
    return 0;
}

int touchHandler(const char *path, const char *types, lo_arg ** argv,
                 int argc, lo_message msg, void *user_data)
{
    int i, j, found;
    const char *msgType = &argv[0]->s;
    if (msgType[0] == 'a') {
        touchCount.update(argc-1, tt_queue);
        // "alive" message: check to see if any of our touches have disappeared
        for (i = 0; i < MAX_TOUCH; i++) {
            found = 0;
            for (j = 1; j < argc; j++) {
                if (touches[i].sessionId == argv[j]->i) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                touchPosition.instance(touches[i].sessionId).release(tt_queue);
                touches[i].sessionId = -1;
            }
        }
    }
    else if (msgType[0] == 's' && msgType[1] == 'e') {
        // "set" message
        int sessionId = argv[1]->i, spare = -1;
        for (i = 0; i < MAX_TOUCH; i++) {
            if (touches[i].sessionId == sessionId)
                break;
            else if (touches[i].sessionId == -1)
                spare = i;
        }
        if (i < MAX_TOUCH) {
            touches[i].lastPosition = touches[i].currentPosition;
            touches[i].currentPosition.set(argv[2]->f, argv[3]->f);
        }
        else if (spare > -1) {
            touches[spare].sessionId = sessionId;
            touches[spare].currentPosition.set(argv[2]->f, argv[3]->f);
            touches[spare].lastPosition = touches[spare].currentPosition;
        }
        else {
            printf("no indexes available for touch %d\n", sessionId);
            return 0;
        }
        touchPosition.instance(sessionId).update(touches[i].currentPosition.coord,
                                                 tt_queue);
    }
    return 0;
}

int objectHandler(const char *path, const char *types, lo_arg ** argv,
                  int argc, lo_message msg, void *user_data)
{
    int i, j, found;
    const char *msgType = &argv[0]->s;
    if (msgType[0] == 'a') {
        // "alive" message: check to see if any of our objects have disappeared
        for (i = 0; i < MAX_OBJECT; i++) {
            found = 0;
            for (j = 1; j < argc; j++) {
                if (objects[i].sessionId == argv[j]->i) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                objectPosition.instance(objects[i].sessionId).release(tt_queue);
                objects[i].sessionId = -1;
                objects[i].objectId = -1;
            }
        }
    }
    else if (msgType[0] == 's' && msgType[1] == 'e') {
        // "set" message
        int sessionId = argv[1]->i, spare = -1;
        for (i = 0; i < MAX_OBJECT; i++) {
            if (objects[i].sessionId == sessionId)
                break;
            else if (objects[i].sessionId == -1)
                spare = i;
        }
        if (i < MAX_OBJECT) {
            objects[i].lastPosition = objects[i].currentPosition;
            objects[i].currentPosition.set(argv[3]->f, argv[4]->f);
        }
        else if (spare > -1) {
            i = spare;
            objects[i].sessionId = sessionId;
            objects[i].currentPosition.set(argv[3]->f, argv[4]->f);
            objects[i].lastPosition = objects[i].currentPosition;
        }
        else {
            printf("no indexes available for object %d\n", sessionId);
            return 0;
        }

        objects[i].objectId = argv[2]->i;
        objectType.instance(sessionId).update(objects[i].objectId, tt_queue);

        objects[i].angle = argv[5]->f;
        objectAngle.instance(sessionId).update(objects[i].angle, tt_queue);

        objectPosition.instance(sessionId).update(objects[i].currentPosition.coord,
                                                  tt_queue);
    }
    return 0;
}

void startup(const char *tuio_port) {
    // initialise touch array
    int i;
    for (i = 0; i < MAX_TOUCH; i++)
        touches[i].sessionId = -1;

    printf("starting OSC server with port %s\n", tuio_port);
    server = lo_server_new(tuio_port, errorHandler);
    lo_server_add_method(server, "/tuio/2Dcur", NULL, touchHandler, NULL);
    lo_server_add_method(server, "/tuio/2Dobj", NULL, objectHandler, NULL);
    lo_server_add_bundle_handlers(server, bundleStartHandler, bundleEndHandler, NULL);

    float min[2] = {0.f, 0.f}, max[2] = {1.f, 1.f};
    touchPosition = dev.add_signal(MAPPER_DIR_OUTGOING, MAX_TOUCH,
                                   "touch/position", 2, 'f', "normalized",
                                   min, max, 0, 0);
    touchAggregateTranslation = dev.add_signal(MAPPER_DIR_OUTGOING, 1,
                                               "touch/aggregate/translation", 2, 'f',
                                               "normalized", min, max, 0, 0);
    touchAggregateGrowth = dev.add_signal(MAPPER_DIR_OUTGOING, 1,
                                          "touch/aggregate/growth", 1, 'f',
                                          "normalized", min, max, 0, 0);
    min[0] = -M_PI;
    max[0] = M_PI;
    touchAggregateRotation = dev.add_signal(MAPPER_DIR_OUTGOING, 1,
                                            "touch/aggregate/rotation", 1, 'f',
                                            "radians", &min, &max, 0, 0);
}

void cleanup() {
    printf("freeing OSC server... ");
    lo_server_free(server);
    printf("done.\n");
}

void ctrlc(int sig)
{
    done = 1;
}

int main() {
    signal(SIGINT, ctrlc);
    printf("Ctrl-C to abort\n");

    startup("3333");

    while (!done) {
        lo_server_recv_noblock(server, 0);
        dev.poll(10);
    }

    cleanup();
    return 0;
}
