//
//  scrollEvent.m
//  mapper-mouseEvent
//
//  Created by malloch on 11/04/2019.
//  Copyright © 2019 Graphics and Experiential Media Lab. All rights reserved.
//

#import <Foundation/Foundation.h>

void post_scroll_evt(int32_t x, int32_t y) {
    CGEventRef evt = CGEventCreateScrollWheelEvent(nil, kCGScrollEventUnitPixel, 2, y, x);
    CGEventPost(kCGHIDEventTap, evt);
}
