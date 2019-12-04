//
//  Use this file to import your target's public headers that you would like to expose to Swift.
//

#include <mpr/mpr.h>
#import <Foundation/Foundation.h>

#define MOUSE_MOVED 0
#define LEFT_BUTTON_UP 1
#define LEFT_BUTTON_DOWN 2
#define LEFT_MOUSE_DRAGGED 3
#define RIGHT_BUTTON_UP 4
#define RIGHT_BUTTON_DOWN 5
#define RIGHT_MOUSE_DRAGGED 6
#define SCROLL_WHEEL 7

mpr_dev start_mpr_dev(const char *name);

void quit_mpr_dev(mpr_dev d);

int poll_mpr_dev(mpr_dev d);

void post_scroll_evt(int32_t x, int32_t y);
