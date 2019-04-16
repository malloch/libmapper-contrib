//#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <lo/lo.h>
#include <mapper/mapper.h>

#define MAX_TOUCH 10

lo_server server;
mapper_device tuio = 0;
mapper_signal touchPos = 0;
mapper_signal touchAggTrans = 0;
mapper_signal touchAggGrowth = 0;
mapper_signal touchAggRot = 0;
mapper_timetag_t tt_queue;

int done = 0;

typedef struct {
    union {
        float coord[2];
        struct {
            float x;
            float y;
        };
    };
} point_t, *point;

typedef struct {
    point_t min;
    point_t max;
} box_t, *box;

typedef struct {
    point_t current;
    point_t last;
    float theta;
    int id;
} touch_t, *touch;
touch_t touches[MAX_TOUCH];

void errorHandler(int num, const char *msg, const char *where)
{
    printf("liblo server error %d in path %s: %s\n", num, where, msg);
}

int bundleStartHandler(lo_timetag tt, void *user_data)
{
    mapper_timetag_now(&tt_queue);
    mapper_device_start_queue(tuio, tt_queue);
    return 0;
}

void init_point(point p, float x, float y)
{
    if (!p)
        return;
    p->x = x;
    p->y = y;
}

void init_box(box b, float minx, float miny, float maxx, float maxy)
{
    if (!b)
        return;
    b->min.x = minx;
    b->min.y = miny;
    b->max.x = maxx;
    b->max.y = maxy;
}

void box_update_extrema(box b, point p)
{
    if (p->x < b->min.x)
        b->min.x = p->x;
    if (p->x > b->max.x)
        b->max.x = p->x;
    if (p->y < b->min.y)
        b->min.y = p->y;
    if (p->y > b->max.y)
        b->max.y = p->y;
}

int bundleEndHandler(void *user_data)
{
    // calculate aggregate centre
    int i, count = 0;

    point_t mean, delta;
    init_point(&mean, 0.f, 0.f);
    init_point(&delta, 0.f, 0.f);

    box_t last, current;
    init_box(&last, 1.f, 1.f, 0.f, 0.f);
    init_box(&current, 1.f, 1.f, 0.f, 0.f);

    for (i = 0; i < MAX_TOUCH; i++) {
        touch t = &touches[i];
        if (t->id == -1)
            continue;
        mean.x += t->current.x;
        mean.y += t->current.y;
        delta.x += t->current.x - t->last.x;
        delta.y += t->current.y - t->last.y;
        ++count;

        box_update_extrema(&last, &t->last);
        box_update_extrema(&current, &t->current);
    }
    if (count > 0) {
        mean.x /= count;
        mean.y /= count;

        // calculate aggregate translation
        delta.x /= count;
        delta.y /= count;
        mapper_signal_update(touchAggTrans, delta.coord, 1, tt_queue);

        // find bounding box dimensions and calculate growth
        float dw = (current.max.x - current.min.x) - (last.max.x - last.min.x);
        float dh = (current.max.y - current.min.y) - (last.max.y - last.min.y);

        float growth = sqrtf(dw*dw + dh*dh);

        mapper_signal_update(touchAggGrowth, &growth, 1, tt_queue);
    }

    mapper_device_send_queue(tuio, tt_queue);
    return 0;
}

int messageHandler(const char *path, const char *types, lo_arg ** argv,
                   int argc, lo_message msg, void *user_data)
{
    int i, j, found;
    const char *msgType = &argv[0]->s;
    if (msgType[0] == 'a') {
        // "alive" message: check to see if any of our touches have disappeared
        for (i = 0; i < MAX_TOUCH; i++) {
            found = 0;
            for (j = 1; j < argc; j++) {
                if (touches[i].id == argv[j]->i) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                mapper_signal_instance_release(touchPos, touches[i].id, tt_queue);
                touches[i].id = -1;
            }
        }
    }
    else if (msgType[0] == 's' && msgType[1] == 'e') {
        // "set" message
        int id = argv[1]->i, spare = -1;
        for (i = 0; i < MAX_TOUCH; i++) {
            if (touches[i].id == id)
                break;
            else if (touches[i].id == -1)
                spare = i;
        }
        if (i < MAX_TOUCH) {
            touches[i].last.x = touches[i].current.x;
            touches[i].last.y = touches[i].current.y;
            touches[i].current.x = argv[2]->f;
            touches[i].current.y = argv[3]->f;
        }
        else if (spare > -1) {
            touches[spare].id = id;
            touches[spare].last.x = touches[spare].current.x = argv[2]->f;
            touches[spare].last.y = touches[spare].current.y = argv[3]->f;
        }
        else {
            printf("no indexes available for touch %d\n", id);
            return 0;
        }
        mapper_signal_instance_update(touchPos, id, touches[i].current.coord,
                                      1, tt_queue);
    }
    return 0;
}

void startup(const char *tuio_port) {
    // initialise touch array
    int i;
    for (i = 0; i < MAX_TOUCH; i++)
        touches[i].id = -1;

    printf("starting OSC server with port %s\n", tuio_port);
    server = lo_server_new(tuio_port, errorHandler);
    lo_server_add_method(server, "/tuio/2Dcur", NULL, messageHandler, NULL);
    lo_server_add_bundle_handlers(server, bundleStartHandler, bundleEndHandler, NULL);

    tuio = mapper_device_new("tuio", 0, 0);
    float min = 0.f, max = 1.f;
    touchPos = mapper_device_add_signal(tuio, MAPPER_DIR_OUTGOING, MAX_TOUCH,
                                             "touch/position", 2, 'f', "normalized",
                                             &min, &max, 0, 0);
    touchAggTrans = mapper_device_add_signal(tuio, MAPPER_DIR_OUTGOING, MAX_TOUCH,
                                             "touch/aggregate/translation", 2, 'f',
                                             "normalized", &min, &max, 0, 0);
    touchAggGrowth = mapper_device_add_signal(tuio, MAPPER_DIR_OUTGOING, MAX_TOUCH,
                                              "touch/aggregate/growth", 2, 'f',
                                              "normalized", &min, &max, 0, 0);
    touchAggRot = mapper_device_add_signal(tuio, MAPPER_DIR_OUTGOING, MAX_TOUCH,
                                           "touch/aggregate/rotation", 1, 'f',
                                           "normalized", &min, &max, 0, 0);
}

void cleanup() {
    printf("freeing OSC server... ");
    lo_server_free(server);
    printf("done.\n");

    printf("freeing libmapper device... ");
    mapper_device_free(tuio);
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
        mapper_device_poll(tuio, 10);
    }

    cleanup();
    return 0;
}
