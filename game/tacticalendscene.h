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

#ifndef UFO_ATTACK_TACTICAL_END_SCENE_INCLUDED
#define UFO_ATTACK_TACTICAL_END_SCENE_INCLUDED

#include "scene.h"
#include "unit.h"
#include "gamelimits.h"

class UIImage;
class UIButtonBox;
class UIButtonGroup;

struct TacticalEndSceneData
{
	int nTerrans;
	int nTerransAlive;
	int nAliens;
	int nAliensAlive;
};

class TacticalEndScene : public Scene
{
public:
	TacticalEndScene( Game* _game, const TacticalEndSceneData* data );
	virtual ~TacticalEndScene();

	// UI
	virtual void Tap(	int count, 
						const grinliz::Vector2I& screen,
						const grinliz::Ray& world )				{}

	virtual void Drag(	int action,
						const grinliz::Vector2I& view )			{}

	virtual void Zoom( int action, int distance )				{}
	virtual void CancelInput()									{}

	// Rendering
	virtual void DoTick( U32 currentTime, U32 deltaTime )		{}
	virtual void DrawHUD();

private:
	UIImage*		background;
	const TacticalEndSceneData* data;
};


#endif // UFO_ATTACK_TACTICAL_END_SCENE_INCLUDED