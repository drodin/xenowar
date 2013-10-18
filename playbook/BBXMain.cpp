/*
 * main.cpp
 *
 *  Created on: Oct 26, 2011
 *      Author: User
 */
//#include <curses.h>
#include <assert.h>
#include <screen/screen.h>
#include <bps/accelerometer.h>
#include <bps/navigator.h>
#include <bps/screen.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <bps/dialog.h>
#include <bps/virtualkeyboard.h>

#include "input/screen_helpers.h"
#include "gestures/set.h"
#include "gestures/pinch.h"
#include "gestures/rotate.h"

#include <sys/keycodes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include "bbutil.h"

#include "BBXUtil.h"
#include "AL/alut.h"

#include "main.h"

#include <string>

struct gestures_set * set;

int exit_application = 0;
unsigned int fpsTimerLoopMS = 0; //set to 60 fps limit at the start

static screen_context_t screen_cxt;
//Query g_primaryGLX and g_primaryGLY of the window surface created by utility code
EGLint g_primaryGLX, g_primaryGLY;

bool g_bUsingAccelerometer = false;
bool g_bMousePressed = false;

int initialize()
{
	eglQuerySurface(egl_disp, egl_surf, EGL_WIDTH, &g_primaryGLX);
    eglQuerySurface(egl_disp, egl_surf, EGL_HEIGHT, &g_primaryGLY);

	EGLint err = eglGetError();
	if (err != 0x3000) {
		fprintf(stderr, "Unable to querry egl surface dimensions\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void ProcessEvents()
{
	    while (true)
	    {
	        bps_event_t* event = NULL;
	        int rc = bps_get_event(&event, 0);

	        assert(BPS_SUCCESS == rc);
	        if (rc != BPS_SUCCESS)
	        {
	         fprintf(stderr, "HUH?!");
	            break;
	        }

	        if (event == NULL)
	        {
	            // No more events in the queue
	            break;
	        }

	        int domain = bps_event_get_domain(event);
	        int code = bps_event_get_code(event);

	        if (navigator_get_domain() == domain)
	        {

	            switch(code)
	            {

	            case NAVIGATOR_ORIENTATION_CHECK:

	            	fprintf(stderr, "Device rotated, but telling device to ignore it.  Change later?");
	            	navigator_orientation_check_response(event, false); //tell it we won't rotate.. for now

	            	break;

	            case NAVIGATOR_EXIT:

	            	fprintf(stderr, "Leaving BBX app");
	            	exit_application = 1;

	            	break;

	            case NAVIGATOR_WINDOW_STATE:
	            {

	            	navigator_window_state_t winState = navigator_event_get_window_state(event);

	            	switch (winState)
	            	{
	            	case NAVIGATOR_WINDOW_FULLSCREEN:
	            		OnEnterForeground();
	            		fprintf(stderr, "Full screen");

	            		break;
	            	case NAVIGATOR_WINDOW_THUMBNAIL:

	            		fprintf(stderr, "App thumbnailed");
	                  	OnEnterBackground();

	            		break;

	            	case NAVIGATOR_WINDOW_INVISIBLE:
	            		fprintf(stderr, "App in background");
	                  	OnEnterBackground();

	            		break;

	            	}
	            }

	            	break;

	            case NAVIGATOR_WINDOW_INACTIVE:
	               // m_isPaused = true;
	               // m_handler->onPause();
		           fprintf(stderr, "Window unactive");
		           OnEnterBackground();
		           break;

	            case NAVIGATOR_WINDOW_ACTIVE:
	               fprintf(stderr, "Window active");
	               OnEnterForeground();
	               break;
	            }

	        } else if (screen_get_domain() == domain)
	        {
	        	 int screen_val, rc;

	        	    screen_event_t screen_event = screen_event_get_event(event);
	        	    mtouch_event_t mtouch_event;
	        	    rc = screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &screen_val);

	        	    if(screen_val == SCREEN_EVENT_MTOUCH_TOUCH || screen_val == SCREEN_EVENT_MTOUCH_MOVE || screen_val == SCREEN_EVENT_MTOUCH_RELEASE) {
	        	        rc = screen_get_mtouch_event(screen_event, &mtouch_event, 0);
	        	        if (rc) {
	        	            fprintf(stderr, "Error: failed to get mtouch event\n");
	        	        }
	        	        rc = gestures_set_process_event(set, &mtouch_event, NULL);

	        	        /* No gesture detected, treat as pan. */
	        	        if (!rc) {
	        	        	//fprintf(stderr, "No gesture - type:%d, id:%d, x:%f, y:%f\n", mtouch_event.event_type, mtouch_event.contact_id, mtouch_event.x, mtouch_event.y);

	        	        	if (mtouch_event.contact_id==0) {
								switch (screen_val) {
								case SCREEN_EVENT_MTOUCH_TOUCH:
									if (!g_bMousePressed)
										SendGUIEx(MESSAGE_TYPE_GUI_CLICK_START, mtouch_event.x, mtouch_event.y, 0);
									g_bMousePressed = true;
									break;
								case SCREEN_EVENT_MTOUCH_MOVE:
									if (g_bMousePressed)
										SendGUIEx(MESSAGE_TYPE_GUI_CLICK_MOVE, mtouch_event.x, mtouch_event.y, 0);
									break;
								case SCREEN_EVENT_MTOUCH_RELEASE:
									if (g_bMousePressed)
										SendGUIEx(MESSAGE_TYPE_GUI_CLICK_END, mtouch_event.x, mtouch_event.y, 0);
									g_bMousePressed = false;
									break;
								}
	        	        	} else { //more then one finger
								if (g_bMousePressed)
									SendGUIEx(MESSAGE_TYPE_GUI_CLICK_CANCEL, mtouch_event.x, mtouch_event.y, 0);
								g_bMousePressed = false;
	        	        	}
	        	        }
	        	    }

	        	    if (screen_val == SCREEN_EVENT_POINTER)
	        	    {
	        	    	int buttonState = 0;
	        	    	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_BUTTONS, &buttonState);

	        	    	int coords[2];
	        	    	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_SOURCE_POSITION, coords);

	        	    	int wheel_delta;
	        	    	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_MOUSE_WHEEL, &wheel_delta);

	        	    	if (coords[0] < 0 || coords[1] < 0)
	        	    		return;

	        	    	if (wheel_delta != 0) {
	                        SendGUIEx(MESSAGE_TYPE_GUI_GESTURE_PINCH, 0, wheel_delta, 0);
	        	    		return;
	        	    	}

	        	    	static int lastButtonState = 0;

	        	    	if (lastButtonState != buttonState)
							SendGUIEx((buttonState)?MESSAGE_TYPE_GUI_CLICK_START:MESSAGE_TYPE_GUI_CLICK_END, coords[0], coords[1], 0);
	        	    	else if (buttonState)
							SendGUIEx(MESSAGE_TYPE_GUI_CLICK_MOVE, coords[0], coords[1], 0);

	        	    	lastButtonState = buttonState;
	        	    }

	            if(screen_val ==  SCREEN_EVENT_KEYBOARD)
	          	            {
	          	            	int flags, val;
	          	            	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_FLAGS, &flags);

	          	            	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_SYM, &val);
	          					if (flags & KEY_DOWN)
	          					{
	          						SendGUI(MESSAGE_TYPE_GUI_CHAR, (float)val, (float)1);

	          						if (! (flags & KEY_REPEAT))
	          						{
	          							//if (val < 128) val = toupper(val);
	          							SendGUI(MESSAGE_TYPE_GUI_CHAR_RAW, (float)val, (float)1);
	          						}

	          					} else
	          					{
	          						//key released
	          						//if (val < 128) val = toupper(val);
	          						SendGUI(MESSAGE_TYPE_GUI_CHAR_RAW, (float)val, (float)0);
	          					}
	          	            }

	        } else if (dialog_get_domain() == domain)
	        {

	        	/*

	        	 /if (DIALOG_RESPONSE == code)
	            {
	             //   ASSERT(m_promptInProgress);
	              //  m_promptInProgress = false;
	                //m_handler->onPromptOk(dialog_event_get_prompt_input_field(event));
	            	fprintf(stderr, "Got dialog response %d", code);
	            }
	            */
	        } else {
	           fprintf(stderr, "Unrecognized and unrequested event! domain: %d, code: %d", domain, code);
	        }

	       // bps_event_destroy(event);  //Conflicting docs about needing this.. but it will crash, so no, I guess not
	    }

}

void SetFpsLimit(int fps) {
	fpsTimerLoopMS = (int) (1000.0f/fps);
}

void SetAccelerometerUpdate(int enable) {
	if (!accelerometer_is_supported())
	{
		fprintf(stderr, "Ignoring acceleremeter command, device doesn't have one");
	} else
	{
		if (enable != 0)
		{
			g_bUsingAccelerometer = true;
			//turn it on at 40hz.  The API wants enums, not custom settings (?), so I guess I'll just do this.
			accelerometer_set_update_frequency(FREQ_40_HZ);
		} else
		{
			g_bUsingAccelerometer = false;
		}
	}
}

void gesture_callback(gesture_base_t* gesture, mtouch_event_t* event, void* param, int async)
{
    if (async) {
        fprintf(stderr,"[async] ");
    }
    switch (gesture->type) {
        case GESTURE_PINCH: {
            gesture_pinch_t* pinch = (gesture_pinch_t*)gesture;
            //fprintf(stderr,"Pinch %d, %d", (pinch->last_distance.x - pinch->distance.x), (pinch->last_distance.y - pinch->distance.y));

            int dist_x = pinch->distance.x;
            int dist_y = pinch->distance.y;
            int last_dist_x = pinch->last_distance.x;
            int last_dist_y = pinch->last_distance.y;

            int reldist = sqrt((dist_x)*(dist_x) + (dist_y)*(dist_y));
            int last_reldist = sqrt((last_dist_x)*(last_dist_x) + (last_dist_y)*(last_dist_y));

            if (reldist && last_reldist) {
                SendGUIEx(MESSAGE_TYPE_GUI_GESTURE_PINCH, last_reldist, reldist, 0);
            }
            break;
        }
        case GESTURE_ROTATE: {
            gesture_rotate_t* rotate = (gesture_rotate_t*)gesture;
            //fprintf(stderr,"Rotate %d, %d", rotate->last_angle, rotate->angle);

            if (rotate->angle) {
                SendGUIEx(MESSAGE_TYPE_GUI_GESTURE_ROTATE, rotate->last_angle, rotate->angle, 0);
            }
            break;
        }
        default: {
            fprintf(stderr,"Unknown Gesture");
            break;
        }
    }
    fprintf(stderr,"\n");
}

static void init_gestures()
{
    set = gestures_set_alloc();
    if (NULL != set) {
        pinch_gesture_alloc(NULL, gesture_callback, set);
        rotate_gesture_alloc(NULL, gesture_callback, set);
    } else {
        fprintf(stderr, "Failed to allocate gestures set\n");
    }
}

static void gestures_cleanup()
{
    if (NULL != set) {
        gestures_set_free(set);
        set = NULL;
    }
}

int main(int argc, char *argv[])
{
	if (!alutInit(NULL, NULL))
		fprintf(stderr, "Can't initialize sound");

    int rc;

    //Create a screen context that will be used to create an EGL surface to to receive libscreen events
    screen_create_context(&screen_cxt, 0);

    init_gestures();

    //Initialize BPS library
	bps_initialize();

	//Use utility code to initialize EGL for 2D rendering with GL ES 1.1
	if (EXIT_SUCCESS != bbutil_init_egl(screen_cxt)) {
		fprintf(stderr, "bbutil_init_egl failed\n");
		bbutil_terminate();
		screen_destroy_context(screen_cxt);
		return 0;
	}

	//Initialize application logic
	if (EXIT_SUCCESS != initialize()) {
		fprintf(stderr, "initialize failed\n");
		bbutil_terminate();
		screen_destroy_context(screen_cxt);
		return 0;
	}

	//Signal BPS library that navigator and screen events will be requested
	if (BPS_SUCCESS != screen_request_events(screen_cxt)) {
		fprintf(stderr, "screen_request_events failed\n");
		bbutil_terminate();
		screen_destroy_context(screen_cxt);
		return 0;
	}

	if (BPS_SUCCESS != navigator_request_events(0)) {
		fprintf(stderr, "navigator_request_events failed\n");
		bbutil_terminate();
		screen_destroy_context(screen_cxt);
		return 0;
	}

	 navigator_rotation_lock(true);

	 /*
	//Signal BPS library that navigator orientation is not to be locked
	if (BPS_SUCCESS != navigator_rotation_lock(false)) {
		fprintf(stderr, "navigator_rotation_lock failed\n");
		bbutil_terminate();
		screen_destroy_context(screen_cxt);
		return 0;
	}
	*/

	//get proton going

	//SetAccelerometerUpdate(1);

	if (!Init())
	{
		fprintf(stderr, "Proton failed to init");
		return 0;
	}

   unsigned int gameTimer = 0;

   while(exit_application == 0)
    {
    	//Request and process BPS next available event

	    ProcessEvents();

	    if (exit_application != 0)
	    	{
	    		fprintf(stderr, "Exitting main game loop");
	    		break;
	    	}


	    if (fpsTimerLoopMS != 0)
	    		{
	    			while (gameTimer > GetSystemTimeTick())
	    			{
	    				usleep(1); //give control back to the system for a bit
	    			}
	    			gameTimer = GetSystemTimeTick()+fpsTimerLoopMS;
	    		}


	    if (!IsInBackground())
	    {
			if (g_bUsingAccelerometer)
			{
				//poll, and send messages
				double x,y,z;
				if (accelerometer_read_forces(&x, &y, &z) == BPS_SUCCESS)
				{
					SendGUIVec(MESSAGE_TYPE_GUI_ACCELEROMETER, x, y, z);
				}
			}

			Update();
			Draw();
			bbutil_swap();
	    }

    }

   fprintf(stderr, "Shutting down in main");


   if (IsBaseAppInitted())
   {
	  // OnEnterBackground();
	   Kill();
   }

   gestures_cleanup();

	screen_stop_events(screen_cxt);

	//Destroy libscreen context
    screen_destroy_context(screen_cxt);

    //Shut down BPS library for this process
    bps_shutdown();

    //Use utility code to terminate EGL setup
    bbutil_terminate();

    alutExit();

    return 0;
}
