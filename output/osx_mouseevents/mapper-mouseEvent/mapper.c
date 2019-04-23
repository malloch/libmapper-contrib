//
//  mapper.c
//  mapper-mouseEvent
//
//  Created by malloch on 08/04/2019.
//  Copyright © 2019 Graphics and Experiential Media Lab. All rights reserved.
//

#include "mapper.h"

extern void emit_mouse_evt(int, float, float, int);

mapper_device device = NULL;

int updated;
float epsilon = 0.001;

int leftButtonStatus = 0;
int rightButtonStatus = 0;

typedef struct {
    float x;
    float y;
} point;

point current;
point last;

void errorHandler(int num, const char *msg, const char *where)
{
    printf("liblo server error %d in path %s: %s\n", num, where, msg);
}

int sign(int value)
{
    return value > 0 ? 1 : -1;
}

double get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + tv.tv_usec / 1000000.0;
}

// grab time when button down pressed, if within 1sec increment click count
int click_count(int evt)
{
    static int click_counter = 1;
    static double timetag = 0;

    if (evt == LEFT_BUTTON_DOWN || evt == RIGHT_BUTTON_DOWN) {
        double now = get_current_time();
        if ((now - timetag) < 1.0)
            click_counter = 2;
        else
            click_counter = 1;
        timetag = now;
    }
    return click_counter;
}

int emit()
{
    float dx = current.x - last.x;
    float dy = current.y - last.y;

    // handle buttons
    int evt = 0;
    if (leftButtonStatus > 1)
        evt = LEFT_BUTTON_DOWN;
    else if (leftButtonStatus < -1)
        evt = LEFT_BUTTON_UP;
    else if (rightButtonStatus > 1)
        evt = RIGHT_BUTTON_DOWN;
    else if (rightButtonStatus < -1)
        evt = RIGHT_BUTTON_UP;

    if (evt) {
        emit_mouse_evt(evt, current.x, current.y, click_count(evt));
        leftButtonStatus = sign(leftButtonStatus);
        rightButtonStatus = sign(rightButtonStatus);
        return 0;
    }

    if (fabsf(dx) < epsilon && fabsf(dy) < epsilon)
        return 0;

    last.x = current.x;
    last.y = current.y;

    if (leftButtonStatus > 0) {
        // left mouse drag
        emit_mouse_evt(LEFT_MOUSE_DRAGGED, current.x, current.y, 0);
    }
    else if (rightButtonStatus > 0) {
        // right mouse drag
        emit_mouse_evt(RIGHT_MOUSE_DRAGGED, current.x, current.y, 0);
    }
    else {
        // mouse move
        emit_mouse_evt(MOUSE_MOVED, current.x, current.y, 0);
    }
    return 0;
}

void inst_events(mapper_signal sig, mapper_id instance,
                 mapper_instance_event event, mapper_timetag_t *tt)
{
    printf("got instance event %d\n", event);
}

void cursor_handler(mapper_signal sig, mapper_id instance, const void *value,
                    int count, mapper_timetag_t *timetag)
{
    float *valf = (float*)value;
    current.x = valf[0];
    current.y = valf[1];
    updated = 1;
}

void left_button_handler(mapper_signal sig, mapper_id instance, const void *value,
                         int count, mapper_timetag_t *timetag)
{
    int status = sign(*(int*)value);
    if (status == sign(leftButtonStatus)) // no change
        return;
    leftButtonStatus += status * 2;
    updated = 1;
}

void right_button_handler(mapper_signal sig, mapper_id instance, const void *value,
                          int count, mapper_timetag_t *timetag)
{
    int status = sign(*(int*)value);
    if (status == sign(rightButtonStatus)) // no change
        return;
    rightButtonStatus += status * 2;
    updated = 1;
}

void scroll_handler(mapper_signal sig, mapper_id instance, const void *value,
                    int count, mapper_timetag_t *timetag)
{
    float *position = (float*)value;
    emit_mouse_evt(SCROLL_WHEEL, position[0], position[1], 1);
}

void zoom_handler(mapper_signal sig, mapper_id instance, const void *value,
                  int count, mapper_timetag_t *timetag)
{
    ;
}

void rotation_handler(mapper_signal sig, mapper_id instance, const void *value,
                      int count, mapper_timetag_t *timetag)
{
    ;
}

mapper_device start_mapper_device(const char *name) {
    printf("starting mapper_device with name %s\n", name);
    mapper_device d = mapper_device_new(name, 0, 0);
    int mini = 0, maxi = 1;
    float minf[2] = {0.f, 0.f}, maxf[2] = {1.f, 1.f};
    mapper_device_add_signal(d, MAPPER_DIR_INCOMING, 1, "position", 2, 'f',
                             "normalized", minf, maxf, cursor_handler, NULL);
    mapper_device_add_signal(d, MAPPER_DIR_INCOMING, 1, "button/left", 1, 'i',
                             NULL, &mini, &maxi, left_button_handler, NULL);
    mapper_device_add_signal(d, MAPPER_DIR_INCOMING, 1, "button/right", 1, 'i',
                             NULL, &mini, &maxi, right_button_handler, NULL);
    mapper_device_add_signal(d, MAPPER_DIR_INCOMING, 1, "scrollWheel", 2, 'f',
                             "normalized", minf, maxf, scroll_handler, 0);

//    mapper_device_add_signal(d, MAPPER_DIR_INCOMING, 1, "zoom", 1, 'f',
//                             "normalized", min, max, zoom_handler, NULL);
//    min[0] = -M_PI;
//    max[0] = M_PI;
//    mapper_device_add_signal(d, MAPPER_DIR_INCOMING, 1, "rotation", 1, 'f',
//                             "radians", min, max, rotation_handler, NULL);
    return d;
}

int poll_mapper_device(mapper_device d) {
    updated = 0;
    mapper_device_poll(d, 1);
    if (updated)
        emit();
    return 0;
}

void quit_mapper_device(mapper_device d) {
    printf("freeing mapper device %s\n", mapper_device_name(d));
    mapper_device_free(d);
}
