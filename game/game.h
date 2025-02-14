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

#ifndef UFOATTACK_GAME_INCLUDED
#define UFOATTACK_GAME_INCLUDED

#include "../grinliz/gldebug.h"
#include "../grinliz/gltypes.h"
#include "../engine/engine.h"
#include "../engine/surface.h"
#include "../engine/texture.h"
#include "../engine/model.h"
#include "../engine/uirendering.h"
#include "../grinliz/glperformance.h"
#include "../engine/ufoutil.h"
#include <tinyxml2.h>
#include "../shared/gamedbreader.h"
#include "../gamui/gamui.h"
#include "../faces/faces.h"
#include "item.h"
#include "unit.h"
#include "cgame.h"
#include "battledata.h"

#include <limits.h>

class ParticleSystem;
class Scene;
class SceneData;
class ISceneResult;
class ItemDef;
class TiXmlDocument;
class Stats;
class Unit;
class Research;

static const float ONE8  = 1.0f / 8.0f;
static const float ONE16 = 1.0f / 16.0f;

static const float TRANSLUCENT_WHITE	= ONE8*0.0f + ONE16;
static const float TRANSLUCENT_GREEN	= ONE8*1.0f + ONE16;
static const float TRANSLUCENT_BLUE		= ONE8*2.0f + ONE16;
static const float TRANSLUCENT_RED		= ONE8*3.0f + ONE16;
static const float TRANSLUCENT_YELLOW	= ONE8*4.0f + ONE16;
static const float TRANSLUCENT_GREY		= ONE8*5.0f + ONE16;


struct TileSetDesc {
	const char* set;	// "FARM"
	int size;			// 16, 32, 64
	const char* type;	// "TILE"
	int variation;		// 0-99
};


/*
	Assets
	---------------------------------

	Database:
	Read only. (Constructed by the builder.)

	The database (uforesource.db) contains everything the game needs to 
	run. This is primarily to ease the installation on the phones. One big
	wad file is easier to manage and take care of. All assets are compressed 
	in the database so the size is quite good.

	Models		- binary
	Textures	- binary
	Map			- XML

	Serialization (Saving and Loading)
	----------------------------------
	Everything at runtime is saved as XML. This includes:
	Map (copied from the database, but then changed as the game plays)
	Engine
	Units
	Camera
	etc.

	MapMaker
	-------------------------------
	The evil mapmaker hack...the mapmaker works directly on XML files in the
	resource-in directory (resin). Since the MapMaker is also the BattleScene
	in a slightly different flavor, this leads to some circular code and compilation.
*/

class Game : public ITextureCreator 
{
public:
	Game( int width, int height, int rotation, const char* savepath );
	Game( int width, int height, int rotation, const char* path, const TileSetDesc& tileSetDesc );
	~Game();

	void DeviceLoss();
	void Resize( int width, int height, int rotation );
	void DoTick( U32 msec );

	void Tap( int action, int x, int y );
	void Zoom( int style, float distance );
	void Rotate( float degreesFromStart );
	void CancelInput();

	void JoyButton( int id, bool down );
	void JoyDPad( int dir );
	void JoyStick( int id, float x, float y );
	
	const ItemDefArr&	GetItemDefArr() const	{ return itemDefArr; }

	// debugging / testing / mapmaker
	void MouseMove( int x, int y );
	void HandleHotKeyMask( int mask );
	void ToggleTV();

	void RotateSelection( int delta );
	void DeleteAtSelection();
	void DeltaCurrentMapItem( int d );

	// MapMaker methods
	void ShowPathing( int show )	{ mapmaker_showPathing = show; }
	int ShowingPathing()			{ return mapmaker_showPathing; }
	void SetLightMap( float r, float g, float b );

	enum {	MAX_NUM_LIGHT_MAPS = 16,

			BATTLE_SCENE = 0,
			CHARACTER_SCENE,
			INTRO_SCENE,
			END_SCENE,
			UNIT_SCORE_SCENE,
			HELP_SCENE,
			DIALOG_SCENE,
			GEO_SCENE,
			GEO_END_SCENE,
			BASETRADE_SCENE,
			BUILDBASE_SCENE,
			FASTBATTLE_SCENE,
			RESEARCH_SCENE,
			SETTING_SCENE,
			SAVE_LOAD_SCENE,
			NEW_TAC_OPTIONS,
			NEW_GEO_OPTIONS,
			NUM_SCENES,
		 };

	void PushScene( int sceneID, SceneData* data );
	void PopScene( int result = INT_MAX );
	void PopAllAndLoad( int slot );

	bool IsScenePushed() const		{ return sceneQueued.sceneID != NUM_SCENES; }

	U32 CurrentTime() const	{ return currentTime; }
	U32 DeltaTime() const	{ return currentTime-previousTime; }

	void SuppressText( bool suppress )	{ suppressText = suppress; }
	bool IsTextSuppressed() const		{ return suppressText; }

	void SetDebugLevel( int level )		{ debugLevel = (level%4); }
	int GetDebugLevel() const			{ return debugLevel; }

	FILE* GameSavePath( SavePathType type, SavePathMode mode, int slot ) const;
	bool HasSaveFile( SavePathType type, int slot ) const;
	void DeleteSaveFile( SavePathType type, int slot );
	void SavePathTimeStamp( SavePathType type, int slot, grinliz::GLString* stamp );
	int LoadSlot() const				{ return loadSlot; }

	void Load( const tinyxml2::XMLDocument& doc );
	void Save( int slot, bool saveGeo, bool saveTac );

	bool PopSound( int* database, int* offset, int* size );

	const Research* GetResearch();
	const gamedb::Reader* GetDatabase()	{ return database0; }

	BattleData battleData;

	enum {
		ATOM_TEXT, ATOM_TEXT_D,
		ATOM_TACTICAL_BACKGROUND,
		ATOM_TACTICAL_BACKGROUND_TEXT,

		ATOM_GEO_VICTORY,
		ATOM_GEO_DEFEAT,

		ATOM_GREEN_BUTTON_UP, ATOM_GREEN_BUTTON_UP_D, ATOM_GREEN_BUTTON_DOWN, ATOM_GREEN_BUTTON_DOWN_D,
		ATOM_BLUE_BUTTON_UP, ATOM_BLUE_BUTTON_UP_D, ATOM_BLUE_BUTTON_DOWN, ATOM_BLUE_BUTTON_DOWN_D,
		ATOM_RED_BUTTON_UP, ATOM_RED_BUTTON_UP_D, ATOM_RED_BUTTON_DOWN, ATOM_RED_BUTTON_DOWN_D,
		ATOM_BLUE_TAB_BUTTON_UP, ATOM_BLUE_TAB_BUTTON_UP_D, ATOM_BLUE_TAB_BUTTON_DOWN, ATOM_BLUE_TAB_BUTTON_DOWN_D,

		ATOM_COUNT
	};
	const gamui::RenderAtom& GetRenderAtom( int id );
	enum {
		GREEN_BUTTON,
		BLUE_BUTTON,
		BLUE_TAB_BUTTON,
		RED_BUTTON,
		LOOK_COUNT
	};
	const gamui::ButtonLook& GetButtonLook( int id );

	Texture* CalcFaceTexture( const Unit* unit, grinliz::Rectangle2F* uv );

	static gamui::RenderAtom CalcDecoAtom( int id, bool enabled=true );
	static gamui::RenderAtom CalcControllerAtom( bool dpad, int id, bool enabled, float* widthRatio );
	static gamui::RenderAtom CalcParticleAtom( int id, bool enabled=true );
	static gamui::RenderAtom CalcIconAtom( int id, bool enabled=true );
	static gamui::RenderAtom CalcIcon2Atom( int id, bool enabled=true );
	enum {
		PALETTE_GREEN, PALETTE_BLUE, PALETTE_RED, PALETTE_YELLOW, PALETTE_GREY,
		PALETTE_NORM=0, PALETTE_BRIGHT, PALETTE_DARK
	};
	static gamui::RenderAtom CalcPaletteAtom( int c0, int c1, int blend, bool enable=true );

	// For creating some required textures:
	virtual void CreateTexture( Texture* t );

	struct Palette
	{
		const char* name;
		int dx;
		int dy;
		CArray< grinliz::Color4U8, 128 > colors;
	};
	const Palette* GetPalette( const char* name ) const;
	grinliz::Color4U8 MainPaletteColor( int x, int y );

	void AddDatabase( const char* path );
	const grinliz::GLString* GetModDatabasePaths() const { return modDatabase; }		
	void LoadModDatabase( const char* path, bool preload );

//private:
	Screenport screenport;
public:
	Engine* engine;
private:
	// Face Generation
	FaceGenerator faceGen;
	Surface faceSurface;
	Surface oneFaceSurface;
	struct FaceCache {
		U32 seed;
	};
	int faceCacheSlot;
	FaceCache faceCache[MAX_TERRANS];

	// Color palettes
	CDynArray< Palette > palettes;
	const Palette* mainPalette;
	void LoadPalettes();

	grinliz::GLString modDatabase[GAME_MAX_MOD_DATABASES];

	Surface surface;

	void PushPopScene();

	bool scenePopQueued;
	int loadSlot;

	void Init();
	void LoadTextures();
	void LoadModels();
	void LoadModel( const char* name );
	void LoadItemResources();
	void LoadAtoms();

	void DumpWeaponInfo( FILE* fp, float range, const Stats& stats, int count );

	int currentFrame;
	U32 markFrameTime;
	U32 frameCountsSinceMark;
	float framesPerSecond;
	int debugLevel;
	bool suppressText;
	grinliz::Vector2F joyStickAccum;

	ModelLoader* modelLoader;
	gamedb::Reader* database0;		// the basic, complete database
	gamedb::Reader* database1;		// the mod database

	struct SceneNode {
		Scene*			scene;
		int				sceneID;
		SceneData*		data;
		int				result;

		SceneNode() : scene( 0 ), sceneID( NUM_SCENES ), data( 0 ), result( INT_MIN )	{}

		void Free();
		void SendResult();
	};
	void CreateScene( const SceneNode& in, SceneNode* node );
	SceneNode sceneQueued;
	CStack<SceneNode> sceneStack;

	U32 currentTime;
	U32 previousTime;
	bool isDragging;

	int rotTestStart;
	int rotTestCount;
	grinliz::GLString savePath;
	CDynArray< char > resourceBuf;

	gamui::RenderAtom renderAtoms[ATOM_COUNT];
	gamui::ButtonLook buttonLooks[LOOK_COUNT];
	
	int		mapmaker_showPathing;	// 0 off, 1 path, 2 vis
	grinliz::GLString		mapmaker_xmlFile;
	ItemDefArr itemDefArr;
};




#endif
