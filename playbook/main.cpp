#include <stdio.h>
#include "main.h"

#include "BBXUtil.h"
#include "Sound.h"

/**************************/
#include "../game/cgame.h"

int   gAppAlive   = 1;

static long lastClockTime = 0;
static long gameTime = 0;
#define MAX_TIME_DELTA 100

static void* game = 0;
bool paused = false;

Sound snd;
/**************************/
ALCcontext *context;
ALCdevice *device;

bool Init(int width, int height) {
	if ( game ) {
		GameDeviceLoss( game );
	}

    gAppAlive    = 1;

	if ( game == 0 ) {
		game = NewGame( width, height, 0, playbookSavePath );
	}

	return true;
}

void Kill() {
	if ( game )
		DeleteGame( game );
	game = 0;
}

void Update() {
	int databaseID=0, length=0, mem=0;
	while ( GamePopSound( game, &databaseID, &mem, &length ) ) {
		snd.unload();
		snd.load( "", (ALvoid*)mem, length );
		snd.play();
	}
}

void Draw() {
    long currentClockTime;
	long delta;

	currentClockTime = GetSystemTimeTick();

	// It is very hard to keep track of paused state, so we don't.
	// If a render comes in, just total up the current time from the
	// delta time. Cap it in case there was a pause.
	//
	delta = currentClockTime - lastClockTime;
	if ( delta > MAX_TIME_DELTA ) {
		delta = MAX_TIME_DELTA;
	}

	gameTime += delta;
	lastClockTime = currentClockTime;

	GameDoTick( game, gameTime );
}

void OnEnterForeground() {
	if ( game )
		GameDeviceLoss( game );
	paused = false;
}

void OnEnterBackground() {
	if ( game )
		GameSave( game );
	paused = true;
}

bool IsInBackground() {
	if (paused)
		return true;
	return false;
}

bool IsBaseAppInitted() {
	if (game)
		return true;
	return false;
}

void SendGUI( eMessageType type, float parm1, float parm2) {

}

void SendGUIVec( eMessageType type, double parm1, double parm2, double param3) {

}

void SendGUIEx( eMessageType type, float param1, float param2, int finger) {
	if ( game ) {
		switch (type) {
		case MESSAGE_TYPE_GUI_CLICK_START:
			GameTap( game, MESSAGE_TYPE_GUI_CLICK_START, (int)param1, (int)param2 );
			break;
		case MESSAGE_TYPE_GUI_CLICK_MOVE:
			GameTap( game, MESSAGE_TYPE_GUI_CLICK_MOVE, (int)param1, (int)param2 );
			break;
		case MESSAGE_TYPE_GUI_CLICK_END:
			GameTap( game, MESSAGE_TYPE_GUI_CLICK_END, (int)param1, (int)param2 );
			break;
		case MESSAGE_TYPE_GUI_CLICK_CANCEL:
			GameTap( game, MESSAGE_TYPE_GUI_CLICK_CANCEL, (int)param1, (int)param2 );
			break;
		case MESSAGE_TYPE_GUI_GESTURE_PINCH:
			GameZoom( game, 0, (param1-param2)/100 );
			break;
		case MESSAGE_TYPE_GUI_GESTURE_ROTATE:
			GameCameraRotate( game, param2-param1 );
			break;
		}
	}
}

