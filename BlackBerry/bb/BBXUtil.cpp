#include <assert.h>
#include <screen/screen.h>
#include <bps/accelerometer.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <bps/dialog.h>
#include <bps/virtualkeyboard.h>
#include <sys/keycodes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "bbutil.h"

#include "BBXUtil.h"

#include <string>

// convert the timespec into milliseconds //thanks, cocos2d-x
long time2millis(struct timespec *times)
{
    return times->tv_sec*1000 + times->tv_nsec/1000000;
}


unsigned int GetSystemTimeTick()
{
        /*

         //yeah, this works great in the simulator, too bad it's all goofy ass on the real device
        struct timeval tv;
                gettimeofday(&tv, NULL);
                return tv.tv_usec/1000 + tv.tv_sec*1000;
        */

        static timespec lastTime;
        static bool bRanFirstTime = false;
        static unsigned int t = 0;

        if (!bRanFirstTime)
        {
                clock_gettime(CLOCK_REALTIME, &lastTime);
                bRanFirstTime = true;
        }

        timespec curTime;
        clock_gettime(CLOCK_REALTIME, &curTime);

        t +=  (time2millis(&curTime)-time2millis(&lastTime)) ;
        lastTime = curTime;
        return t;
}
