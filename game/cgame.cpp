/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../grinliz/gldebug.h"
#include "cgame.h"
#include "game.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#define resourceFile "uforesource.db"

#include "../grinliz/glstringutil.h"


class CheckThread
{
public:
	CheckThread()	{ GLASSERT( active == false ); active = true; }
	~CheckThread()	{ GLASSERT( active == true ); active = false; }
private:
	static bool active;
};

bool CheckThread::active = false;

static char* modDatabases[ GAME_MAX_MOD_DATABASES ] = { 0 };


void* NewGame( int width, int height, int rotation, const char* path, bool tvMode )
{
	CheckThread check;

	SetTVMode( tvMode );
	Game* game = new Game( width, height, rotation, path );
	GLOUTPUT(( "NewGame.\n" ));

	for( int i=0; i<GAME_MAX_MOD_DATABASES; ++i ) {
		if ( modDatabases[i] ) {
			//GLOUTPUT(( "adding database '%s'\n", modDatabases[i] ));
			game->AddDatabase( modDatabases[i] );
		}
	}

	return game;
}


void DeleteGame( void* handle )
{
	CheckThread check;

	GLOUTPUT(( "DeleteGame. handle=%x\n", handle ));
	if ( handle ) {
		Game* game = (Game*)handle;
		delete game;
	}
	for( int i=0; i<GAME_MAX_MOD_DATABASES; ++i ) {
		if ( modDatabases[i] ) {
			free( modDatabases[i] );
			modDatabases[i] = 0;
		}
	}
}



void GameResize( void* handle, int width, int height, int rotation ) {
	CheckThread check;

	GLOUTPUT(( "GameResize. handle=%x\n", handle ));
	Game* game = (Game*)handle;
	game->Resize( width, height, rotation );
}


void GameSave( void* handle ) {
	CheckThread check;

	GLOUTPUT(( "GameSave. handle=%x\n", handle ));
	Game* game = (Game*)handle;
	game->Save( 0, true, true );
}


void GameDeviceLoss( void* handle )
{
	CheckThread check;

	GLOUTPUT(( "GameDeviceLoss. handle=%x\n", handle ));
	Game* game = (Game*)handle;
	game->DeviceLoss();
}


void GameZoom( void* handle, int style, float delta )
{
	CheckThread check;

	Game* game = (Game*)handle;
	game->Zoom( style, delta );
}


void GameCameraRotate( void* handle, float degrees )
{
	CheckThread check;

	Game* game = (Game*)handle;
	game->Rotate( degrees );
}


void GameTap( void* handle, int action, int x, int y )
{
	CheckThread check;

	Game* game = (Game*)handle;
	game->Tap( action, x, y );
}


void GameDoTick( void* handle, unsigned int timeInMSec )
{
	CheckThread check;

	Game* game = (Game*)handle;
	game->DoTick( timeInMSec );
}


void GameCameraGet( void* handle, int param, float* value ) 
{
	CheckThread check;

	Game* game = (Game*)handle;
	switch( param ) {
		case GAME_CAMERA_TILT:
			*value = game->engine->camera.GetTilt();
			break;
		case GAME_CAMERA_YROTATE:
			*value = game->engine->camera.GetYRotation();
			break;
		case GAME_CAMERA_ZOOM:
			*value = game->engine->GetZoom();
			break;
		default:
			GLASSERT( 0 );
	}
}


void GameMoveCamera( void* handle, float dx, float dy, float dz )
{
	CheckThread check;

	Game* game = (Game*)handle;
	game->engine->camera.DeltaPosWC( dx, dy, dz );
}


void GameHotKey( void* handle, int mask )
{
	CheckThread check;

	Game* game = (Game*)handle;
	return game->HandleHotKeyMask( mask );
}



void PlatformPathToResource( char* buffer, int bufferLen )
{
	grinliz::GLString resourcePath = "";
    char* basePath = SDL_GetBasePath();
    if (basePath)
    	resourcePath.append(basePath);
    resourcePath.append(resourceFile);
    grinliz::StrNCpy( buffer, resourcePath.c_str(), bufferLen );
}


const char* PlatformName()
{
	if ( TVMode() ) {
		return "tv";
	}
#if defined( __MOBILE__ )
	return "android";
#else
    return "pc";
#endif
}


void GameAddDatabase( const char* path )
{
	for( int i=0; i<GAME_MAX_MOD_DATABASES; ++i ) {
		if ( modDatabases[i] == 0 ) {
			int len = strlen( path ) + 1;
			modDatabases[i] = (char*)malloc( len );
			strcpy( modDatabases[i], path );
			break;
		}
	}
}


int GamePopSound( void* handle, int* database, int* offset, int* size )
{
	CheckThread check;

	Game* game = (Game*)handle;
	bool result = game->PopSound( database, offset, size );	
	return (result) ? 1 : 0;
}

void GameJoyButton( void* handle, int id, bool down )
{
	static const char* bNames[9] = {
		"none",
		"buttonDown",
		"buttonLeft",
		"buttonUp",
		"buttonRight",
		"L1",
		"R1",
		"L2",
		"R2"
	};

	if ( id >= 0 && id <9 ) {
		//GLOUTPUT(( "JoyButton %s %s\n", bNames[id], down ? "down" : "up" ));
		Game* game = (Game*)handle;
		game->JoyButton( id, down );
	}
}


void GameJoyDPad( void* handle, int dir )
{
	/*
	GLOUTPUT(( "DPad: " ));
	if ( dir & GAME_JOY_DPAD_UP )		GLOUTPUT(( "up " ));
	if ( dir & GAME_JOY_DPAD_DOWN )		GLOUTPUT(( "down " ));
	if ( dir & GAME_JOY_DPAD_LEFT )		GLOUTPUT(( "left " ));
	if ( dir & GAME_JOY_DPAD_RIGHT )	GLOUTPUT(( "right " ));
	if ( dir == 0 )						GLOUTPUT(( "center" ));
	GLOUTPUT(( "\n" ));
	*/
	Game* game = (Game*)handle;
	game->JoyDPad( dir );
}

void GameJoyStick( void* handle, int id, float x, float y )
{
	float DEAD_ZONE = 0.30f;
	if ( x > -DEAD_ZONE && x < DEAD_ZONE ) x = 0;
	if ( y > -DEAD_ZONE && y < DEAD_ZONE ) y = 0;

	if ( x || y ) {
		Game* game = (Game*)handle;
		game->JoyStick( id, x, y );
	}
}


