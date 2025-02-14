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

#include "geomap.h"
#include "../engine/loosequadtree.h"
#include "../engine/model.h"
#include "gamelimits.h"

GeoMap::GeoMap( SpaceTree* _tree ) : tree( _tree )
{
	scrolling = 0;
	dayNightOffset = 0;

	// Use a size 2 texture. The netbook GPU driver doesn't like size 1 textures.
	dayNightSurface.Set( Surface::RGB16, DAYNIGHT_TEX_SIZE, 2 );
	{
		grinliz::Color4U8 nc = { EL_NIGHT_RED_U8, EL_NIGHT_GREEN_U8, EL_NIGHT_BLUE_U8 };
		U16 night = Surface::CalcRGB16( nc );
		for( int i=0; i<DAYNIGHT_TEX_SIZE; ++i ) {
			dayNightSurface.SetTex16( i, 0, i<DAYNIGHT_TEX_SIZE/2 ? night : 0xffff );
			dayNightSurface.SetTex16( i, 1, i<DAYNIGHT_TEX_SIZE/2 ? night : 0xffff );
		}
	}

	geoModel[0] = geoModel[1] = 0;
	dayNightTex = TextureManager::Instance()->CreateTexture( "GeoDayNight", DAYNIGHT_TEX_SIZE, 2, Surface::RGB16, Texture::PARAM_NONE, this );

}


GeoMap::~GeoMap()
{
	tree->FreeModel( geoModel[0] );
	tree->FreeModel( geoModel[1] );
	TextureManager::Instance()->DeleteTexture( dayNightTex );
}


bool GeoMap::GetDayTime( float x )
{
	float night0 = -dayNightOffset*(float)MAP_X;
	float night1 = night0 + 0.5f*(float)MAP_X;

	if ( grinliz::InRange( x, night0, night1 ) || grinliz::InRange( x, night0+(float)MAP_X, night1+(float)MAP_X ) ) {
		return false;
	}
	return true;
}


void GeoMap::LightFogMapParam( float* w, float* h, float* dx, float* dy )
{
	*w = (float)MAP_X;
	*h = (float)MAP_Y;
	*dx = -scrolling + dayNightOffset;
	*dy = 0;
}


void GeoMap::CreateTexture( Texture* t )
{
	if ( t == dayNightTex ) {
		t->Upload( dayNightSurface );
	}
}


void GeoMap::DoTick( U32 currentTime, U32 deltaTime )
{
	dayNightOffset += (float)deltaTime / (40.0f*1000.0f);

	// Create geoModel as needed. (Deleted when we deactivate.)
	if ( !geoModel[0] ) {
		for( int i=0; i<2; ++i ) {
			geoModel[i] = tree->AllocModel( ModelResourceManager::Instance()->GetModelResource( "geomap" ) );
			geoModel[i]->SetFlag( Model::MODEL_OWNED_BY_MAP );
		}
		geoModel[1]->SetPos( MAP_X, 0, 0 );
		/*
		if ( TVMode() )
			geoModel[1]->SetFlag( Model::MODEL_INVISIBLE );
		else
		*/
			geoModel[1]->ClearFlag( Model::MODEL_INVISIBLE );
	}

	if ( dayNightOffset > 1.0f ) {
		double intpart;
		dayNightOffset = (float)modf( dayNightOffset, &intpart );
	}
}


