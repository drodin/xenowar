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

#ifndef UFOATTACK_SCENE_INCLUDED
#define UFOATTACK_SCENE_INCLUDED

#include "../grinliz/glvector.h"
#include "../grinliz/glgeometry.h"
#include "../grinliz/glrectangle.h"
#include "../gamui/gamui.h"
#include "../engine/uirendering.h"
#include "gamelimits.h"
#include <tinyxml2.h>

#include <stdio.h>

class Game;
class Engine;
class TiXmlElement;
class Unit;

#ifdef _MSC_VER
#pragma warning ( push )
#pragma warning ( disable : 4100 )	// un-referenced formal parameter
#endif

class SceneData
{
public:
	SceneData()				{}
	virtual ~SceneData()	{}
};


/**

	Saving/Loading Notes
	- A scene is self-saving, and has it's own save file (simpler)
	- Game.Save() saves the current top scene, if that scene CanSave()
	
	For XenoWar:
	- GeoScene and BattleScene CanSave()
	
	Geo		Tac
	yes		no		geo scene
	yes		yes		geo game, but in tactical scene. geo loads, then pushes BattleScene. When ChildActivated, loads Battlescene
	no		yes		fast battle game
	no		no		no game in progress

	Actions:
	Geo -> Tac
		- geo: saves geo.xml
		- geo: creates tac.xml
	Tac-> Geo
		- geo: saves geo.xml
		- geo: deletes tac.xml
*/
class Scene
{
public:
	Scene( Game* _game );
	virtual ~Scene()											{}

	virtual void Activate()										{}
	virtual void DeActivate()									{}

	// UI
	virtual void Tap(	int action, 
						const grinliz::Vector2F& screen,
						const grinliz::Ray& world )				{}
	virtual void Zoom( int style, float normal )				{}
	virtual void Rotate( float degrees )						{}
	virtual void CancelInput()									{}

	virtual void JoyButton( int id, bool down )					{}
	// id=0 the actual dpad, id=1 the virtual dpad (left stick)
	virtual void JoyDPad( int id, int dir )						{}
	virtual void JoyStick( int id, const grinliz::Vector2F& value )		{}

	virtual SavePathType CanSave()								{ return SAVEPATH_NONE; }
	virtual void Save( tinyxml2::XMLPrinter* )					{}
	virtual void Load( const tinyxml2::XMLElement* doc )		{}
	virtual void HandleHotKeyMask( int mask )					{}
	virtual void Resize()										{}

	virtual void SceneResult( int sceneID, int result )			{}
	virtual void ChildActivated( int childID, Scene* childScene, SceneData* data )		{}

	// Rendering
	enum {
		RENDER_2D = 0x01,
		RENDER_3D = 0x02,
	};

	virtual int RenderPass( grinliz::Rectangle2I* clip3D, grinliz::Rectangle2I* clip2D )	
	{ 
		clip3D->SetInvalid(); 
		clip2D->SetInvalid(); 
		return 0; 
	}

	void RenderGamui2D()	{ gamui2D.Render(); }
	void RenderGamui3D()	{ gamui3D.Render(); }

	// Perspective rendering.
	virtual void DoTick( U32 currentTime, U32 deltaTime )		{}
	// 2D overlay rendering.
	virtual void DrawHUD()										{}

	// Put in debugging output into the 3D stream, or other
	// scene specific 3D elements
	virtual void Draw3D()										{}

	Engine* GetEngine();
	Game* GetGame() { return game; }

protected:

	Game*			game;
	UIRenderer		uiRenderer;
	gamui::Gamui	gamui2D, gamui3D;
};


// Utility class for standard display of the Unit name / rank / weapon
struct NameRankUI {
	gamui::Image		face;
	gamui::Image		rank;
	gamui::TextLabel	name;

	enum { FACE_SIZE = 50 };
	enum {	DISPLAY_FACE = 0x01,
		    DISPLAY_RANK = 0x02,
			DISPLAY_WEAPON = 0x04,
			DISPLAY_ALL = 0xff
		 };

	void Init( gamui::Gamui*, Game* game );
	void SetFaceSize( float s )	{ faceSize = s; }
	void SetRank( int rank );

	void Set( float x, float y, const Unit* unit, int display );

	void SetVisible( bool visible ) {
		face.SetVisible( visible );
		rank.SetVisible( visible );
		name.SetVisible( visible );
	}
private:
	Game*			game;
	int				display;
	float			faceSize;
};


struct BackgroundUI {
	gamui::Image	background;
	gamui::Image	backgroundText;

	void Init( Game* game, gamui::Gamui*, bool logo );
};


struct OKCancelUI {
	gamui::PushButton	okayButton;
	gamui::PushButton	cancelButton;

	void Init( Game* game, gamui::Gamui*, float size );
	void SetVisible( bool v ) {
		okayButton.SetVisible( v );
		cancelButton.SetVisible( v );
	}
};

#ifdef _MSC_VER
#pragma warning ( pop )
#endif

#endif // UFOATTACK_SCENE_INCLUDED