#include "dialogscene.h"
#include "game.h"
#include "cgame.h"
#include "../engine/engine.h"
#include "../engine/uirendering.h"
#include "../grinliz/glstringutil.h"

using namespace gamui;

DialogScene::DialogScene( Game* _game, const DialogSceneData* _data ) : Scene( _game ), data( _data )
{
	Engine* engine = GetEngine();
	const Screenport& port = engine->GetScreenport();

	// 248, 228, 8
	//const float INV = 1.f/255.f;
	//uiRenderer.SetTextColor( 248.f*INV, 228.f*INV, 8.f*INV );

	background.Init( &gamui2D, game->GetRenderAtom( Game::ATOM_TACTICAL_BACKGROUND ), false );
	background.SetSize( port.UIWidth(), port.UIHeight() );

	textBox.Init( &gamui2D );
	textBox.SetSize( port.UIWidth()-GAME_GUTTER_X()*2.0f, port.UIHeight()-GAME_GUTTER_Y()*2.0f );
	textBox.SetText( data->text.c_str() );

	const ButtonLook& look = game->GetButtonLook( Game::BLUE_BUTTON );

	button0.Init( &gamui2D, look );
	button0.SetSize( GAME_BUTTON_SIZE_F()*2.0f, GAME_BUTTON_SIZE_F() );

	button1.Init( &gamui2D, look );
	button1.SetSize( GAME_BUTTON_SIZE_F()*2.0f, GAME_BUTTON_SIZE_F() );

	switch ( data->type ) {
	case DialogSceneData::DS_YESNO:
		button0.SetText( "No" );
		button1.SetText( "Yes" );
		break;

	default:
		button0.SetText( "Cancel" );
		button1.SetText( "Okay" );
		break;

	}
}


void DialogScene::Resize()
{
	const Screenport& port = GetEngine()->GetScreenport();
	background.SetSize( port.UIWidth(), port.UIHeight() );
	textBox.SetPos( GAME_GUTTER_X(), GAME_GUTTER_Y() );
	button0.SetPos( GAME_GUTTER_X(), port.UIHeight()-GAME_GUTTER_Y()-GAME_BUTTON_SIZE_F() );
	button1.SetPos( port.UIWidth()-GAME_GUTTER_X()-GAME_BUTTON_SIZE_F()*2.0f, 
					port.UIHeight()-GAME_GUTTER_Y()-GAME_BUTTON_SIZE_F() );
}


void DialogScene::Activate()
{
	GetEngine()->SetMap( 0 );
}


/*
int DialogScene::HandleHotKeyMask( int mask )	
{
	if ( mask == GAME_HK_BACK ) {
	}
}
*/


void DialogScene::Tap( int action, const grinliz::Vector2F& screen, const grinliz::Ray& world )
{
	grinliz::Vector2F ui;
	GetEngine()->GetScreenport().ViewToUI( screen, &ui );

	const UIItem* item = 0;
	if ( action == GAME_TAP_DOWN ) {
		gamui2D.TapDown( ui.x, ui.y );
		return;
	}
	else if ( action == GAME_TAP_CANCEL ) {
		gamui2D.TapCancel();
		return;
	}
	else if ( action == GAME_TAP_UP ) {
		item = gamui2D.TapUp( ui.x, ui.y );
	}

	if ( item == &button0 ) {
		game->PopScene( 0 );
	}
	else if ( item == &button1 ) {
		game->PopScene( 1 );
	}
}
