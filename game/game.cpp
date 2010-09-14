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

#include "game.h"
#include "cgame.h"
#include "scene.h"

#include "battlescene.h"
#include "characterscene.h"
#include "tacticalintroscene.h"
#include "tacticalendscene.h"
#include "tacticalunitscorescene.h"
#include "helpscene.h"
#include "dialogscene.h"

#include "../engine/text.h"
#include "../engine/model.h"
#include "../engine/uirendering.h"
#include "../engine/particle.h"
#include "../engine/gpustatemanager.h"

#include "../grinliz/glmatrix.h"
#include "../grinliz/glutil.h"
#include "../grinliz/glperformance.h"
#include "../grinliz/glstringutil.h"
#include "../tinyxml/tinyxml.h"
#include "../version.h"
#include "ufosound.h"

using namespace grinliz;

extern long memNewCount;

Game::Game( int width, int height, int rotation, const char* path ) :
	screenport( width, height, rotation ),
	markFrameTime( 0 ),
	frameCountsSinceMark( 0 ),
	framesPerSecond( 0 ),
	trianglesPerSecond( 0 ),
	trianglesSinceMark( 0 ),
	debugTextOn( false ),
	previousTime( 0 ),
	isDragging( false )
{
	savePath = path;
	char c = savePath[savePath.size()-1];
	if ( c != '\\' && c != '/' ) {	
#ifdef WIN32
		savePath += "\\";
#else
		savePath += "/";
#endif
	}	
	
	Init();
	Map* map = engine->GetMap();

	engine->camera.SetPosWC( -12.f, 45.f, 52.f );	// standard test

	PushScene( INTRO_SCENE, 0 );
	loadRequested = -1;
	loadCompleted = false;
	PushPopScene();
}


Game::Game( int width, int height, int rotation, const char* path, const TileSetDesc& base ) :
	screenport( width, height, rotation ),
	markFrameTime( 0 ),
	frameCountsSinceMark( 0 ),
	framesPerSecond( 0 ),
	trianglesPerSecond( 0 ),
	trianglesSinceMark( 0 ),
	debugTextOn( false ),
	previousTime( 0 ),
	isDragging( false )
{
	GLASSERT( Engine::mapMakerMode == true );
	savePath = path;
	char c = savePath[savePath.size()-1];
	if ( c != '\\' && c != '/' ) {	
#ifdef WIN32
		savePath += '\\';
#else
		savePath += '/';
#endif
	}	
	
	Init();
	Map* map = engine->GetMap();
	ImageManager* im = ImageManager::Instance();

	map->SetSize( base.size, base.size );

	char buffer[128];
	SNPrintf( buffer, 128, "%4s_%2d_%4s_%02d", base.set, base.size, base.type, base.variation );

	mapmaker_xmlFile  = std::string( path );
	mapmaker_xmlFile += std::string( buffer );
	mapmaker_xmlFile += std::string( ".xml" );
	std::string texture  = std::string( buffer ); 
				texture += std::string( "_TEX" );
	std::string dayMap   = std::string( buffer );
				dayMap  += std::string( "_DAY" );
	std::string nightMap  = std::string( buffer );
				nightMap += std::string( "_NGT" );

	engine->camera.SetPosWC( -25.f, 45.f, 30.f );	// standard test
	engine->camera.SetYRotation( -60.f );

	PushScene( BATTLE_SCENE, 0 );
	loadRequested = -1;
	loadCompleted = false;
	PushPopScene();

	mapmaker_showPathing = false;

	TiXmlDocument doc( mapmaker_xmlFile.c_str() );
	doc.LoadFile();
	if ( !doc.Error() )
		engine->GetMap()->Load( doc.FirstChildElement( "Map" ), GetItemDefArr() );
}


void Game::Init()
{
	scenePopQueued = false;
	currentFrame = 0;
	memset( &profile, 0, sizeof( ProfileData ) );
	surface.Set( Surface::RGBA16, 256, 256 );		// All the memory we will ever need (? or that is the intention)

	// Load the database.
	char buffer[260];
	int offset;
	int length;
	PlatformPathToResource( buffer, 260, &offset, &length );
	database = new gamedb::Reader();
	bool okay = database->Init( buffer, offset );
	GLASSERT( okay );

	SoundManager::Create( database );
	engine = new Engine( &screenport, engineData, database );

	LoadTextures();
	modelLoader = new ModelLoader();
	LoadModels();
	LoadMapResources();
	LoadItemResources();
	LoadAtoms();

	delete modelLoader;
	modelLoader = 0;

	Texture* textTexture = TextureManager::Instance()->GetTexture( "stdfont2" );
	GLASSERT( textTexture );
	UFOText::InitTexture( textTexture );
	UFOText::InitScreen( &screenport );
}


Game::~Game()
{
	if ( Engine::mapMakerMode ) {
		TiXmlDocument doc( mapmaker_xmlFile.c_str() );
		TiXmlElement map( "Map" );
		doc.InsertEndChild( map );
		engine->GetMap()->Save( doc.FirstChildElement( "Map" ) );
		doc.SaveFile();
	}

	// Roll up to the main scene before saving.
	while( sceneStack.Size() > 1 ) {
		PopScene();
		PushPopScene();
	}

	sceneStack.Top().Free();
	sceneStack.Pop();


	for( unsigned i=0; i<EL_MAX_ITEM_DEFS; ++i ) {
		delete itemDefArr[i];
	}
	delete engine;
	SoundManager::Destroy();
	delete database;
}


void Game::SceneNode::Free()
{
	sceneID = Game::NUM_SCENES;
	delete scene;	scene = 0;
	delete data;	data = 0;
	result = INT_MIN;
}



void Game::ProcessLoadRequest()
{
	if ( loadRequested == 0 )	// continue
	{
		GLString path = GameSavePath();
		TiXmlDocument doc;
		doc.LoadFile( path.c_str() );
		GLASSERT( !doc.Error() );
		if ( !doc.Error() ) {
			Load( doc );
			loadCompleted = true;
		}
	}
	else if ( loadRequested == 1 )	// new game
	{
		Load( newGameXML );
		loadCompleted = true;
	}
	loadRequested = -1;
}


void Game::PushScene( int sceneID, SceneData* data )
{
	GLASSERT( sceneQueued.sceneID == NUM_SCENES );
	GLASSERT( sceneQueued.scene == 0 );

	sceneQueued.sceneID = sceneID;
	sceneQueued.data = data;
}


void Game::PopScene( int result )
{
	GLASSERT( scenePopQueued == false );
	scenePopQueued = true;
	if ( result != INT_MAX )
		sceneStack.Top().result = result;
}


void Game::PushPopScene() 
{
	if ( scenePopQueued || sceneQueued.sceneID != NUM_SCENES ) {
		TextureManager::Instance()->ContextShift();
	}

	if ( scenePopQueued ) {
		GLASSERT( !sceneStack.Empty() );

		sceneStack.Top().scene->DeActivate();
		int result = sceneStack.Top().result;

		sceneStack.Top().Free();
		sceneStack.Pop();

		GLASSERT( sceneQueued.sceneID !=NUM_SCENES || !sceneStack.Empty() );

		if ( sceneStack.Size() ) {
			sceneStack.Top().scene->Activate();
			if ( result != INT_MIN ) {
				sceneStack.Top().scene->SceneResult( result );
			}
		}
	}
	if ( sceneQueued.sceneID != NUM_SCENES ) {
		GLASSERT( sceneQueued.sceneID < NUM_SCENES );

		if ( sceneStack.Size() ) {
			sceneStack.Top().scene->DeActivate();
		}
		SceneNode node;
		CreateScene( sceneQueued, &node );
		sceneQueued.data = 0;
		sceneQueued.Free();

		sceneStack.Push( node );
		node.scene->Activate();
		if ( loadRequested >= 0 ) {
			ProcessLoadRequest();
		}
	}

	sceneQueued.Free();
	scenePopQueued = false;
}


void Game::CreateScene( const SceneNode& in, SceneNode* node )
{
	Scene* scene = 0;
	switch ( in.sceneID ) {
		case BATTLE_SCENE:		scene = new BattleScene( this );													break;
		case CHARACTER_SCENE:	scene = new CharacterScene( this, (CharacterSceneData*)in.data );					break;
		case INTRO_SCENE:		scene = new TacticalIntroScene( this );												break;
		case END_SCENE:			scene = new TacticalEndScene( this, (const TacticalEndSceneData*) in.data );		break;
		case UNIT_SCORE_SCENE:	scene = new TacticalUnitScoreScene( this, (const TacticalEndSceneData*) in.data );	break;
		case HELP_SCENE:		scene = new HelpScene( this, (const HelpSceneData*)in.data );						break;
		case DIALOG_SCENE:		scene = new DialogScene( this, (const DialogSceneData*)in.data );					break;
		default:
			GLASSERT( 0 );
			break;
	}
	node->scene = scene;
	node->sceneID = in.sceneID;
	node->data = in.data;
	node->result = INT_MIN;
}


const gamui::RenderAtom& Game::GetRenderAtom( int id )
{
	GLASSERT( id >= 0 && id < ATOM_COUNT );
	GLASSERT( renderAtoms[id].textureHandle );
	return renderAtoms[id];
}


const gamui::ButtonLook& Game::GetButtonLook( int id )
{
	GLASSERT( id >= 0 && id < LOOK_COUNT );
	return buttonLooks[id];
}


void Game::Load( const TiXmlDocument& doc )
{
	// Already pushed the BattleScene. Note that the
	// BOTTOM of the stack saves and loads. (BattleScene or GeoScene).
	const TiXmlElement* game = doc.RootElement();
	GLASSERT( StrEqual( game->Value(), "Game" ) );
	sceneStack.Bottom().scene->Load( game );
}


void Game::Save()
{
	if ( loadCompleted ) {
		TiXmlDocument doc;
		Save( &doc );
		GLString path = GameSavePath();
		doc.SaveFile( path.c_str() );
	}

}


void Game::Save( TiXmlDocument* doc )
{
	TiXmlElement sceneElement( "Scene" );
	sceneElement.SetAttribute( "id", 0 );

	TiXmlElement gameElement( "Game" );
	gameElement.SetAttribute( "version", VERSION );
	gameElement.InsertEndChild( sceneElement );

	doc->InsertEndChild( gameElement );
	sceneStack.Bottom().scene->Save( doc->RootElement() );
}


void Game::DoTick( U32 _currentTime )
{
	currentTime = _currentTime;
	if ( previousTime == 0 ) {
		previousTime = currentTime-1;
	}
	U32 deltaTime = currentTime - previousTime;

	if ( markFrameTime == 0 ) {
		markFrameTime			= currentTime;
		frameCountsSinceMark	= 0;
		framesPerSecond			= 0.0f;
		trianglesPerSecond		= 0;
		trianglesSinceMark		= 0;
	}
	else {
		++frameCountsSinceMark;
		if ( currentTime - markFrameTime > 500 ) {
			framesPerSecond		= 1000.0f*(float)(frameCountsSinceMark) / ((float)(currentTime - markFrameTime));
			// actually K-tris/second
			trianglesPerSecond  = trianglesSinceMark / (currentTime - markFrameTime);
			markFrameTime		= currentTime;
			frameCountsSinceMark = 0;
			trianglesSinceMark = 0;
		}
	}

	GPUShader::ResetState();
	GPUShader::Clear();

	Scene* scene = sceneStack.Top().scene;
	scene->DoTick( currentTime, deltaTime );

	Rectangle2I clip2D, clip3D;
	int renderPass = scene->RenderPass( &clip3D, &clip2D );
	GLASSERT( renderPass );
	
	if ( renderPass & Scene::RENDER_3D ) {
		//	r.Set( 100, 50, 300, 50+200*320/480 );
		//	r.Set( 100, 50, 300, 150 );
		screenport.SetPerspective(	2.f, 
									240.f, 
									20.f*(screenport.UIWidth()/screenport.UIHeight())*320.0f/480.0f, 
									clip3D.IsValid() ? &clip3D : 0 );

		if ( Engine::mapMakerMode ) {
			if ( mapmaker_showPathing ) 
				engine->EnableMap( false );

			engine->Draw();

			if ( mapmaker_showPathing ) {
				engine->GetMap()->DrawPath();
				engine->EnableMap( true );
			}
		}
		else {
			engine->Draw();
			scene->Debug3D();
		}
		
		const grinliz::Vector3F* eyeDir = engine->camera.EyeDir3();
		ParticleSystem* particleSystem = ParticleSystem::Instance();
		particleSystem->Update( deltaTime, currentTime );
		particleSystem->Draw( eyeDir, &engine->GetMap()->GetFogOfWar() );
	}

	// UI Pass
	screenport.SetUI( clip2D.IsValid() ? &clip2D : 0 ); 
	if ( renderPass & Scene::RENDER_3D ) {
		scene->RenderGamui3D();
	}
	if ( renderPass & Scene::RENDER_2D ) {
		screenport.SetUI( clip2D.IsValid() ? &clip2D : 0 );
		scene->DrawHUD();
		scene->RenderGamui2D();
	}

	const int Y = 305;
	#ifndef GRINLIZ_DEBUG_MEM
	const int memNewCount = 0;
	#endif
#if 1
	if ( debugTextOn ) {
		UFOText::Draw(	0,  Y, "UFO#%d %5.1ffps %4.1fK/f %3ddc/f %4dK/s", 
						VERSION,
						framesPerSecond, 
						(float)GPUShader::TrianglesDrawn()/1000.0f,
						GPUShader::DrawCalls(),
						trianglesPerSecond );

		#if defined(DEBUG)
		if ( !Engine::mapMakerMode )  {
			UFOText::Draw(  0, Y-15, "new=%d Tex(%d/%d) %dK/%dK mis=%d re=%d hit=%d",
							memNewCount,
							TextureManager::Instance()->NumTextures(),
							TextureManager::Instance()->NumGPUResources(),
							TextureManager::Instance()->CalcTextureMem()/1024,
							TextureManager::Instance()->CalcGPUMem()/1024,
							TextureManager::Instance()->CacheMiss(),
							TextureManager::Instance()->CacheReuse(),
							TextureManager::Instance()->CacheHit() );		
		}
		#endif
	}
	else {
		UFOText::Draw(	0,  Y, "UFO#%d %5.1ffps", VERSION, framesPerSecond );
	}
#endif
	GPUShader::ResetTriCount();

#ifdef EL_SHOW_MODELS
	int k=0;
	while ( k < nModelResource ) {
		int total = 0;
		for( unsigned i=0; i<modelResource[k].nGroups; ++i ) {
			total += modelResource[k].atom[i].trisRendered;
		}
		UFODrawText( 0, 12+12*k, "%16s %5d K", modelResource[k].name, total );
		++k;
	}
#endif

#ifdef GRINLIZ_PROFILE
	const int SAMPLE = 8;
	if ( (currentFrame & (SAMPLE-1)) == 0 ) {
		memcpy( &profile, &Performance::GetData( false ), sizeof( ProfileData ) );
		Performance::Clear();
	}
	for( unsigned i=0; i<profile.count; ++i ) {
		UFOText::Draw( 0, 20+i*12, "%20s %6.1f %4d", 
					  profile.item[i].name, 
					  100.0f*profile.NormalTime( profile.item[i].functionTime ),
					  profile.item[i].functionCalls/SAMPLE );
	}
#endif

	SoundManager::Instance()->PlayQueuedSounds();

	previousTime = currentTime;
	++currentFrame;

	PushPopScene();
}


void Game::Tap( int action, int wx, int wy )
{
	// The tap is in window coordinate - need to convert to view.
	Vector2F window = { (float)wx, (float)wy };
	Vector2F view;
	screenport.WindowToView( window, &view );

	grinliz::Ray world;
	screenport.ViewToWorld( view, 0, &world );

#if 0
	{
		Vector2F ui;
		screenport.ViewToUI( view, &ui );
		if ( action != GAME_TAP_MOVE )
			GLOUTPUT(( "Tap: action=%d window(%.1f,%.1f) view(%.1f,%.1f) ui(%.1f,%.1f)\n", action, window.x, window.y, view.x, view.y, ui.x, ui.y ));
	}
#endif
	sceneStack.Top().scene->Tap( action, view, world );
}


void Game::MouseMove( int x, int y )
{
	GLASSERT( Engine::mapMakerMode );
	((BattleScene*)sceneStack.Top().scene)->MouseMove( x, y );
}



void Game::Zoom( int action, int distance )
{
	sceneStack.Top().scene->Zoom( action, distance );
}


void Game::Rotate( int action, float degrees )
{
	sceneStack.Top().scene->Rotate( action, degrees );
}


void Game::CancelInput()
{
	isDragging = false;
}


void Game::HandleHotKeyMask( int mask )
{
	if ( mask & GAME_HK_TOGGLE_DEBUG_TEXT ) {
		debugTextOn = !debugTextOn;
	}
	sceneStack.Top().scene->HandleHotKeyMask( mask );
}


void Game::DeviceLoss()
{
	TextureManager::Instance()->DeviceLoss();
}


void Game::Resize( int width, int height, int rotation ) 
{
	screenport.Resize( width, height, rotation );
}


void Game::RotateSelection( int delta )
{
	((BattleScene*)sceneStack.Top().scene)->RotateSelection( delta );
}

void Game::DeleteAtSelection()
{
	((BattleScene*)sceneStack.Top().scene)->DeleteAtSelection();
}


void Game::DeltaCurrentMapItem( int d )
{
	((BattleScene*)sceneStack.Top().scene)->DeltaCurrentMapItem(d);
}

