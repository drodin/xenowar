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

#include "map.h"
#include "model.h"
#include "loosequadtree.h"
#include "renderqueue.h"
#include "surface.h"
#include "text.h"
#include "vertex.h"
#include "uirendering.h"
#include "engine.h"
#include "settings.h"

#include "../engine/particleeffect.h"
#include "../engine/particle.h"

#include <tinyxml2.h>

#include "../grinliz/glstringutil.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glperformance.h"

using namespace grinliz;
using namespace micropather;
using namespace tinyxml2;


const float DIAGONAL_COST = 1.414f;


const ModelResource* MapItemDef::GetModelResource() const
{
	if ( !resource )
		return 0;

	ModelResourceManager* resman = ModelResourceManager::Instance();
	return resman->GetModelResource( resource );
}


const ModelResource* MapItemDef::GetOpenResource() const 
{
	if ( !resourceOpen )
		return 0;

	ModelResourceManager* resman = ModelResourceManager::Instance();
	return resman->GetModelResource( resourceOpen );
}


const ModelResource* MapItemDef::GetDestroyedResource() const
{
	if ( !resourceDestroyed )
		return 0;

	ModelResourceManager* resman = ModelResourceManager::Instance();
	return resman->GetModelResource( resourceDestroyed );
}



Map::Map( SpaceTree* tree )
	: itemPool( "mapItemPool", sizeof( MapItem ), sizeof( MapItem ) * 200, false )
{
	memset( pyro, 0, SIZE*SIZE*sizeof(U8) );
	memset( obscured, 0, SIZE*SIZE*sizeof(U8) );
	memset( visMap, 0, SIZE*SIZE );
	memset( pathMap, 0, SIZE*SIZE );
	dayTime = true;
	pathBlocker = 0;
	nImageData = 0;

	microPather = new MicroPather(	this,			// graph interface
									SIZE*SIZE,		// max possible states (+1)
									6 );			// max adjacent states

	this->tree = tree;
	width = height = SIZE;
	//walkingVertex.Clear();

	gamui::RenderAtom nullAtom;
	for( int i=0; i<NUM_LAYERS; ++i )
		overlay[i].Init( this, nullAtom, nullAtom, 0 );

	walkingMap[0].Init( &overlay[LAYER_UNDER_LOW] );
	//nWalkingMaps = SettingsManager::Instance()->GetNumWalkingMaps();

	if ( MAX_WALKING_MAPS > 1 ) {
		walkingMap[1].Init( &overlay[LAYER_OVER] );
	}
	lightMapValid = false;

	TextureManager* texman = TextureManager::Instance();
	backgroundTexture = texman->CreateTexture( "MapBackground", EL_MAP_TEXTURE_SIZE, EL_MAP_TEXTURE_SIZE, Surface::RGB16, Texture::PARAM_NONE, this );
	greyTexture = texman->CreateTexture( "MapGrey", EL_MAP_TEXTURE_SIZE/2, EL_MAP_TEXTURE_SIZE/2, Surface::RGB16, Texture::PARAM_NONE, this );

	backgroundSurface.Set( Surface::RGB16, EL_MAP_TEXTURE_SIZE, EL_MAP_TEXTURE_SIZE );
	backgroundSurface.Clear( 0 );
	greySurface.Set( Surface::RGB16, EL_MAP_TEXTURE_SIZE/2, EL_MAP_TEXTURE_SIZE/2 );
	greySurface.Clear( 0 );

	nSeenIndex = nUnseenIndex = nPastSeenIndex = 0;
	static const float INV64 = 1.0f / 64.0f;

	for( int j=0; j<SIZE+1; ++j ) {
		for( int i=0; i<SIZE+1; ++i ) {
			mapVertex[j*(SIZE+1)+i].Set( (float)i*INV64, (float)(SIZE-j)*INV64 );
		}
	}

	pathQueryID = 1;
	visibilityQueryID = 1;

	dayMap.Set( Surface::RGB16, SIZE, SIZE );
	nightMap.Set( Surface::RGB16, SIZE, SIZE );
	dayMap.Clear( 255 );
	nightMap.Clear( 255 );
	lightMap = &dayMap;

	lightFogMap.Set( Surface::RGB16, SIZE, SIZE );

	lightMapTex = texman->CreateTexture( "MapLightMap", SIZE, SIZE, Surface::RGB16, Texture::PARAM_NONE, this );
	lightFogMapTex = texman->CreateTexture( "MapLightFogMap", SIZE, SIZE, Surface::RGB16, Texture::PARAM_NONE, this );

	GLOUTPUT(( "Map created. %dK\n", sizeof( *this )/1024 ));
}


Map::~Map()
{
	TextureManager* texman = TextureManager::Instance();
	texman->DeleteTexture( backgroundTexture );
	texman->DeleteTexture( greyTexture );
	texman->DeleteTexture( lightMapTex );
	texman->DeleteTexture( lightFogMapTex );

	Rectangle2I b( 0, 0, SIZE-1, SIZE-1 );
	MapItem* pItem = quadTree.FindItems( b, 0, 0 );

	while( pItem ) {
		MapItem* temp = pItem;
		pItem = pItem->next;
		if ( temp->model )
			tree->FreeModel( temp->model );
		itemPool.Free( temp );
	}
	quadTree.Clear();

	delete microPather;
}


void Map::SetSize( int w, int h )					
{
	width = w; 
	height = h; 
}


void Map::DrawSeen()
{
	GenerateLightMap();
	if ( nSeenIndex == 0 )
		return;

	CompositingShader shader;

	GPUStream stream;
	stream.stride = sizeof( mapVertex[0] );
	stream.nPos = 2;
	stream.posOffset = 0;
	stream.nTexture0 = 2;
	stream.texture0Offset = 0;
	stream.nTexture1 = 2;
	stream.texture1Offset = 0;

	shader.SetStream( stream, mapVertex, nSeenIndex, seenIndex );
	shader.SetTexture0( backgroundTexture );
	shader.SetTexture1( lightMapTex );

	// the vertices are storred in texture coordinates, to use less space.

	Matrix4 swizzle;
	swizzle.m11 = 64.0f;
	swizzle.m22 = 0.0f;
	swizzle.m32 = -64.0f;	swizzle.m33 = 0.0f;		swizzle.m34 = 64.0f;

	shader.PushMatrix( GPUShader::MODELVIEW_MATRIX );
	shader.MultMatrix( GPUShader::MODELVIEW_MATRIX, swizzle );
	shader.Draw();
	shader.PopMatrix( GPUShader::MODELVIEW_MATRIX );
}


void Map::DrawUnseen()
{
	GenerateLightMap();
	if ( nUnseenIndex == 0 )
		return;

	CompositingShader shader;
	GPUStream stream;
	stream.stride = sizeof(mapVertex[0]);
	stream.nPos = 2;
	stream.posOffset = 0;
	shader.SetStream( stream, mapVertex, nUnseenIndex, unseenIndex );

	shader.SetColor( 0, 0, 0 );

	Matrix4 swizzle;
	swizzle.m11 = 64.0f;
	swizzle.m22 = 0.0f;
	swizzle.m32 = -64.0f;	swizzle.m33 = 0.0f;		swizzle.m34 = 64.0f;

	shader.PushMatrix( GPUShader::MODELVIEW_MATRIX );
	shader.MultMatrix( GPUShader::MODELVIEW_MATRIX, swizzle );
	shader.Draw();
	shader.PopMatrix( GPUShader::MODELVIEW_MATRIX );
}


void Map::DrawPastSeen( const Color4F& color )
{
	GenerateLightMap();
	if ( nPastSeenIndex == 0 )
		return;

	CompositingShader shader;
	GPUStream stream;
	stream.stride = sizeof(mapVertex[0]);
	stream.nPos = 2;
	stream.posOffset = 0;
	stream.nTexture0 = 2;
	stream.texture0Offset = 0;
	shader.SetStream( stream, mapVertex, nPastSeenIndex, pastSeenIndex );
	shader.SetTexture0( greyTexture );

	shader.SetColor( color );

	// the vertices are stored in texture coordinates, to use less space.
	Matrix4 swizzle;
	swizzle.m11 = 64.0f;
	swizzle.m22 = 0.0f;
	swizzle.m32 = -64.0f;	swizzle.m33 = 0.0f;		swizzle.m34 = 64.0f;

	shader.PushMatrix( GPUShader::MODELVIEW_MATRIX );
	shader.MultMatrix( GPUShader::MODELVIEW_MATRIX, swizzle );
	shader.Draw();
	shader.PopMatrix( GPUShader::MODELVIEW_MATRIX );
}



void Map::DrawOverlay( int layer )
{
	GLASSERT( layer >= 0 && layer < NUM_LAYERS );
	overlay[layer].Render();
}


void Map::MapImageToWorld( int x, int y, int w, int h, int tileRotation, Matrix2I* m )
{
	Matrix2I t, r, p;
	t.x = x;
	t.y = y;
	r.SetXZRotation( 90*tileRotation );

	switch ( tileRotation ) {
		case 0:
			p.x = 0;	p.y = 0;
			break;

		case 1:
			p.x = 0;	p.y += h-1;
			break;
		case 2:
			p.x += w-1; p.y += h-1;
			break;
		case 3:
			p.x += w-1; p.y += 0;
			break;
		default:
			GLRELASSERT( 0 );
	}
	*m = t * p * r;
}


void Map::SetTexture( const Surface* s, int x, int y, int tileRotation )
{
	//GRINLIZ_PERFTRACK
	GLRELASSERT( s->Width() == s->Height() );

	Rectangle2I src;
	src.Set( 0, 0, s->Width()-1, s->Height()-1 );

	Rectangle2I target;
	target.Set( x, y, x+s->Width()-1, y+s->Height()-1 );	// always square for this to work, obviously.

	if ( tileRotation == 0 ) {
		backgroundSurface.BlitImg( target.min, s, src );
	}
	else 
	{
		Matrix2I m, inv;
		MapImageToWorld( x, y, s->Width(), s->Height(), tileRotation, &m );
		m.Invert( &inv );
		
		backgroundSurface.BlitImg( target, s, inv );
	}

	Rectangle2I greyTarget;
	greyTarget.Set( target.min.x/2, target.min.y/2, target.max.x/2, target.max.y/2 );
	for( int j=greyTarget.min.y; j<=greyTarget.max.y; ++j ) {
		for( int i=greyTarget.min.x; i<=greyTarget.max.x; ++i ) {
			Color4U8 rgba0 = Surface::CalcRGB16( backgroundSurface.GetImg16( i*2+0, j*2+0 ) );
			Color4U8 rgba1 = Surface::CalcRGB16( backgroundSurface.GetImg16( i*2+1, j*2+0 ) );
			Color4U8 rgba2 = Surface::CalcRGB16( backgroundSurface.GetImg16( i*2+0, j*2+1 ) );
			Color4U8 rgba3 = Surface::CalcRGB16( backgroundSurface.GetImg16( i*2+1, j*2+1 ) );

			int c = (  rgba0.r + rgba0.g + rgba0.g + rgba0.b 
				     + rgba1.r + rgba1.g + rgba1.g + rgba1.b 
				     + rgba2.r + rgba2.g + rgba2.g + rgba2.b 
				     + rgba3.r + rgba3.g + rgba3.g + rgba3.b ) >> 4; 
			
			GLRELASSERT( c >= 0 && c <= 255 );
			Color4U8 grey = { (U8)c, (U8)c, (U8)c, 255 };

			greySurface.SetImg16( i, j, Surface::CalcRGB16( grey ) );
		}
	}


	backgroundTexture->Upload( backgroundSurface );
	greyTexture->Upload( greySurface );
}


void Map::SetLightMap0( int x, int y, float r, float g, float b )
{
	static const float EXP = 255.0f;
	Color4U8 rgba = { (U8)(r*EXP), (U8)(g*EXP), (U8)(b*EXP) };

	if ( dayTime )
		dayMap.SetImg16( x, y, Surface::CalcRGB16( rgba ) );
	else
		nightMap.SetImg16( x, y, Surface::CalcRGB16( rgba ) );

	lightMapValid = false;
}


void Map::SetLightMaps( const Surface* day, const Surface* night, int x, int y, int tileRotation )
{
	//GRINLIZ_PERFTRACK
	GLRELASSERT( day );
	GLRELASSERT( night );
	
	GLRELASSERT( day->BytesPerPixel() == 2 );
	GLRELASSERT( night->BytesPerPixel() == 2 );
	GLRELASSERT(    day->Width() == night->Width() 
		      && day->Height() == night->Height() 
			  && day->Width() == day->Height() );

	Rectangle2I rect;
	rect.Set( 0, 0, day->Width()-1, day->Height()-1 );

	if ( tileRotation == 0 ) {
		Vector2I target = { x, y };
		dayMap.BlitImg( target, day, rect );
		nightMap.BlitImg( target, night, rect );
	}
	else {
		Matrix2I inv, m;
		MapImageToWorld( x, y, day->Width(), day->Height(), tileRotation, &m );
		m.Invert( &inv );
		
		Rectangle2I target;
		target.Set( x, y, x+day->Width()-1, y+day->Height()-1 );

		dayMap.BlitImg( target, day, inv );
		nightMap.BlitImg( target, night, inv );
	}
	lightMapValid = false;
}


void Map::SetDayTime( bool day )
{
	if ( day != dayTime ) {
		dayTime = day;
		lightMap = dayTime ? &dayMap : &nightMap;
		lightMapValid = false;
	}
}


grinliz::BitArray<Map::SIZE, Map::SIZE, 1>* Map::LockFogOfWar()
{
	return &fogOfWar;
}


void Map::ReleaseFogOfWar()
{
	pastSeenFOW.DoUnion( fogOfWar );
}


void Map::GenerateLightMap()
{
	//GRINLIZ_PERFTRACK
	if ( !lightMapValid )
	{
		lightMapTex->Upload( *lightMap );
		lightMapValid = true;
	}
}


void Map::GenerateSeenUnseen()
{
	GRINLIZ_PERFTRACK;
	// Test case: 
	// (continue, back) 8.9 k/f
	// Go to strip creation: 5.9 k/f (and that's total - the code below should get the submit down to 400-500 tris possibly?)

	// Ran at about 4% with out this check. Peaks at 0.5% with. Works surprisingly well.
	if ( fogOfWar == cachedFogOfWar )
		return;

	cachedFogOfWar = fogOfWar;

#define PUSHQUAD( _arr, _index, _x0, _x1, _y )		\
		_arr[ _index++ ] = (_y+0)*(SIZE+1)+(_x0);	\
		_arr[ _index++ ] = (_y+1)*(SIZE+1)+(_x0);	\
		_arr[ _index++ ] = (_y+1)*(SIZE+1)+(_x1);	\
		_arr[ _index++ ] = (_y+0)*(SIZE+1)+(_x0);	\
		_arr[ _index++ ] = (_y+1)*(SIZE+1)+(_x1);	\
		_arr[ _index++ ] = (_y+0)*(SIZE+1)+(_x1);	

	int countArr[3] = { 0, 0, 0 };
	nSeenIndex = nUnseenIndex = nPastSeenIndex = 0;

	for( int j=0; j<height; ++j ) {
		for( int i=0; i<width; i += 32 ) {

			U32 fog = fogOfWar.Access32( i, j, 0 );
			U32 past = pastSeenFOW.Access32( i, j, 0 );

			past = past ^ fog;				// if the fog is set, then we don't draw the past.
			U32 unseen = ~( fog | past );	// everything else unseen.

			if ( i+32 > width ) {
				// mask off the end bits.
				U32 mask = (1<<(width-i))-1;

				fog &= mask;
				past &= mask;
				unseen &= mask;
			}

			const U32 arr[3] = { fog, past, unseen };
			U16* indexArr[3] = { seenIndex, pastSeenIndex, unseenIndex };

			for ( int k=0; k<3; ++k ) {

				int start=0;
				while( start < 32 ) {
					int end = start+1;

					if ( (1<<start) & arr[k] ) {
						while( end < 32 ) {
							if ( ((1<<end) & arr[k] ) == 0 ) {
								break;
							}
							++end;
						}
						PUSHQUAD( indexArr[k], countArr[k], i+start, i+end, j );
					}
					start = end;
				}
			}
		}
	}
	nSeenIndex = countArr[0];
	nPastSeenIndex = countArr[1];
	nUnseenIndex = countArr[2];
#ifdef SHOW_FOW
	memcpy( pastSeenIndex+nPastSeenIndex, unseenIndex, sizeof(U16)*nUnseenIndex );
	nPastSeenIndex += nUnseenIndex;
	nUnseenIndex = 0;
#endif

#undef PUSHQUAD

	for( int j=0; j<height; ++j ) {
		for( int i=0; i<width; ++i ) {
			if ( fogOfWar.IsSet( i, j ) ) 
			{
				U16 c = lightMap->GetImg16( i, j );
				lightFogMap.SetImg16( i, j, c );
			}
			else if (    (i>0      && fogOfWar.IsSet( i-1, j ))
				      || (i<SIZE-1 && fogOfWar.IsSet( i+1, j ))
					  || (j>0      && fogOfWar.IsSet( i, j-1 ))
					  || (j<SIZE-1 && fogOfWar.IsSet( i, j+1 )) ) 
			{
				U16 c = lightMap->GetImg16( i, j );
				lightFogMap.SetImg16( i, j, c );
			}
			else 
			{
				lightFogMap.SetImg16( i, j, 0 /*0x3333*/ );
			}
		}
	}
	lightFogMapTex->Upload( lightFogMap );
	quadTree.MarkVisible( fogOfWar );
}


void Map::DoDamage( int x, int y, const MapDamageDesc& damage, Rectangle2I* dBounds, Vector2I* explodes )
{
	float hp = damage.damage;
	if ( hp <= 0.0f )
		return;

	const MapItem* root = quadTree.FindItems( x, y, 0, 0 );

	for( ; root; root=root->next ) {
		if ( root->model ) {
			GLRELASSERT( root->model->IsFlagSet( Model::MODEL_OWNED_BY_MAP ) );

			DoDamage( root->model, damage, dBounds, explodes );
		}
	}
}


void Map::DoDamage( Model* m, const MapDamageDesc& damageDesc, Rectangle2I* dBounds, Vector2I* explodes )
{
	if ( m->IsFlagSet( Model::MODEL_OWNED_BY_MAP ) ) 
	{
		MapItem* item = quadTree.FindItem( m );
		//GLRELASSERT( ( item->flags & MapItem::MI_IS_LIGHT ) == 0 );

		const MapItemDef& itemDef = *item->def;

		int hp = (int)(damageDesc.damage );
		GLOUTPUT(( "map damage '%s' (%d,%d) dam=%d hp=%d\n", itemDef.Name(), item->XForm().x, item->XForm().y, hp, item->hp ));

		bool destroyed = false;
		if ( itemDef.CanDamage() && item->DoDamage(hp) ) 
		{
			if ( dBounds ) {
				Rectangle2I r = item->MapBounds();
				dBounds->DoUnion( r );
				dBounds->DoIntersection( Bounds() );
			}

			// Destroy the current model. Replace it with "destroyed"
			// model if there is one. This is as simple as saving off
			// the properties, deleting it, and re-adding. A little
			// tricky since we reach into the matrix itself.
			int x, y, rot;
			WorldToXYR( item->XForm(), &x, &y, &rot, true );
			const MapItemDef* def = item->def;
			int flags = item->flags;
			int hp = item->hp;

			if ( itemDef.flags & MapItemDef::EXPLODES ) {
				explodes->Set( x, y );
			}
			if ( itemDef.flags & MapItemDef::ROTATES ) {
				U32 in = x | (y<<16);
				U32 hash = Random::Hash( &in, sizeof(in) );
				Random r( hash, x+y );
				rot = r.Rand( 4 );
			}

			DeleteItem( item );
			AddItem( x, y, rot, def, hp, flags );
			destroyed = true;
		}
		if ( !destroyed && itemDef.CanDamage() && itemDef.flammable > 0 ) {
			if ( damageDesc.incendiary > random.Rand( 256-itemDef.flammable )) {
				Rectangle2I b = item->MapBounds();
				SetPyro( (b.min.x+b.max.x)/2, (b.min.y+b.max.y)/2, 0, 1, 0 );
			}
		}
	}
}


void Map::DoSubTurn( Rectangle2I* change, float fireDamagePerSubTurn )
{
	for( int i=0; i<SIZE*SIZE; ++i ) {
		if ( pyro[i] ) {
			int y = i/SIZE;
			int x = i-y*SIZE;

			if ( PyroSmoke( x, y ) || PyroFlare( x, y ) ) {
				int duration = PyroDuration( x, y );
				if ( duration > 0 )
					duration--;
				SetPyro( x, y, duration, false, PyroFlare( x, y ) != 0 );
			}
			else if ( PyroFire( x, y ) ) {
				// Spread? Reduce to smoke?
				// If there is nothing left, then fire ends. (ouchie.)
				MapItem* root = quadTree.FindItems( x, y, 0, 0 );
				while ( root && ( root->Destroyed() || !root->def->flammable ) )	// skip things that are destroyed or inflammable
					root = root->next;

				if ( !root ) {
					// a few sub-turns of smoke
					SetPyro( x, y, 3+random.Rand(2), false, false );
				}
				else {
					// Will torch a building in no time. (Adjacent fires do multiple damage.)
					MapDamageDesc d = { fireDamagePerSubTurn, 0 };
					Vector2I explodes = { -1, -1 };
					DoDamage( x, y, d, change, &explodes );	// FIXME BUG Burning objects don't blow up. Just needs code, and
															// points out that the damage code probably shouldn't be in the 
															// map class.

					MapDamageDesc f = { 0, fireDamagePerSubTurn };
					if ( x > 0 ) DoDamage( x-1, y, f, change, &explodes );
					if ( x < SIZE-1 ) DoDamage( x+1, y, f, change, &explodes );
					if ( y > 0 ) DoDamage( x, y-1, f, change, &explodes );
					if ( y < SIZE-1 ) DoDamage( x, y+1, f, change, &explodes );
				}
			}
		}
	}
}


void Map::EmitParticles( U32 delta )
{
	ParticleSystem* system = ParticleSystem::Instance();
	for( int i=0; i<SIZE*SIZE; ++i ) {
		if ( pyro[i] ) {
			int y = i/SIZE;
			int x = i-y*SIZE;
			Vector3F pos = { (float)x+0.5f, 0.0f, (float)y+0.5f };
			if ( PyroFire( x, y ) ) {
				system->EmitFlame( delta, pos );
			}
			else if ( PyroSmoke( x, y ) ) {
				system->EmitSmoke( delta, pos );
			}
			else if ( PyroFlare( x, y ) ) {
				if ( random.Rand( 1000 ) < delta ) {
					static const Color4F color		= { 1, 0, 0, 1 };
					static const Color4F colorVec	= { 0, 0, 0, -1.f };
					static const Vector3F velocity	= { 0.0f, 0.5f, 0.0f };

					system->EmitPoint(	5,
										ParticleSystem::PARTICLE_RAY,
										color,
										colorVec,
										pos,
										0.01f,
										velocity,
										0.3f );
				}
			}
		}
	}
}


void Map::AddSmoke( int x, int y, int subTurns )
{
	if ( PyroOn( x, y ) ) {
		// If on fire, ignore. Already smokin'
		if ( !PyroFire( x, y ) ) {
			SetPyro( x, y, subTurns, 0, 0 );
		}
	}
	else {
		SetPyro( x, y, subTurns, 0, 0 );
	}
}


void Map::AddFlare( int x, int y, int subturns )
{
	if ( !PyroFire( x, y ) ) {
		SetPyro( x, y, subturns, false, true );
	}
}


void Map::SetPyro( int x, int y, int duration, bool fire, bool flare )
{
	GLRELASSERT( x >= 0 && x < SIZE );
	GLRELASSERT( y >= 0 && y < SIZE );
	U8 p = 0;

	if ( fire ) {
		p |= 0x80;
	}
	else if ( flare ) {
		p |= 0x40;
	}
	p += Clamp( duration, 0, 0x3f );
	pyro[y*SIZE+x] = p;
}


Model* Map::CreatePreview( int x, int y, const MapItemDef* def, int rotation )
{
	Model* model = 0;
	const ModelResource* resource = def->GetModelResource();
	if ( resource ) {

		Matrix2I m;
		Vector3F modelPos;

		XYRToWorld( x, y, rotation*90, &m );
		WorldToModel( m, false, &modelPos );

		model = tree->AllocModel( resource );
		model->SetPos( modelPos );
		model->SetRotation( 90.0f * rotation );
	}
	return model;
}


void Map::WorldToModel( const Matrix2I& mat, bool billboard, grinliz::Vector3F* model )
{
	// Account for rounding from map (integer based) to model (float based) coordinates.
	// Irritating transformation problem.
	{
		Vector2I a = { 0, 0 };
		Vector2I d = { 1, 1 };

		Vector2I aPrime = mat * a;
		Vector2I dPrime = mat * d;

		int xMin = Min( aPrime.x, dPrime.x ) - mat.x;
		int yMin = Min( aPrime.y, dPrime.y ) - mat.y;

		model->x = (float)(mat.x - xMin);
		model->y = 0.0f;
		model->z = (float)(mat.y - yMin);
	}
}


void Map::XYRToWorld( int x, int y, int rotation, Matrix2I* mat )
{
	GLRELASSERT( rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270 );

	Matrix2I r, t, gf;

	r.SetXZRotation( rotation );
	t.SetTranslation( x, y );
	if ( mat ) {
		*mat = t*r;
	}
}


void Map::WorldToXYR( const Matrix2I& mat, int *x, int *y, int* r, bool useRot0123 )
{
	int rMul = (useRot0123) ? 1 : 90;
	*x = mat.x;
	*y = mat.y;

	// XZ rotation. (So confusing. Not XY.)
	// 1 0		0  1		-1 0		0 -1
	// 0 1		-1  0		 0 -1		1  0
	if      ( mat.a == 1  && mat.b == 0  && mat.c == 0  && mat.d == 1 ) {
		*r = 0*rMul;
	}
	else if ( mat.a == 0  && mat.b == 1 && mat.c == -1  && mat.d == 0 ) {
		*r = 1*rMul;
	}
	else if ( mat.a == -1 && mat.b == 0  && mat.c == 0  && mat.d == -1 ) {
		*r = 2*rMul;
	}
	else if ( mat.a == 0  && mat.b == -1  && mat.c == 1 && mat.d == 0 ) {
		*r = 3*rMul;
	}
	else {
		// Not valid simple rotation
		GLRELASSERT( 0 );
		*r = 0;
	}
}


Map::MapItem* Map::AddItem( int x, int y, int rotation, const MapItemDef* def, int hp, int flags )
{
	GLRELASSERT( x >= 0 && x < width );
	GLRELASSERT( y >= 0 && y < height );
	GLRELASSERT( rotation >=0 && rotation < 4 );
	GLRELASSERT( def );

	if ( !def->resource ) {
		GLOUTPUT(( "No model resource.\n" ));
		GLRELASSERT( 0 );
		return 0;
	}
		
	bool metadata = false;

	Matrix2I xform;
	Vector3F modelPos;
	XYRToWorld( x, y, rotation*90, &xform );
	WorldToModel( xform, false, &modelPos );

	// Check for meta data.
	if (    StrEqual( def->Name(), "guard") 
		 || StrEqual( def->Name(), "scout" )
		 || StrEqual( def->Name(), "pawn" ) )
	{
		metadata = true;
		Vector2I v = { x, y };

		if ( !Engine::mapMakerMode ) {
			// If we aren't in MapMaker mode, push back the guard and scout positions for later use.
			// Don't actually add the model.
			if ( StrEqual( def->Name(), "guard" )) {
				guardPos.Push( v );
			}
			else if ( StrEqual( def->Name(), "scout" )) {
				scoutPos.Push( v );
			}
			else if ( StrEqual( def->Name(), "pawn" )) {
				civPos.Push( v );
			}
			return 0;
		}
	}

	Rectangle2I mapBounds;
	MultMatrix2I( xform, def->Bounds(), &mapBounds );

	// Check bounds on map.
	if ( mapBounds.min.x < 0 || mapBounds.min.y < 0 || mapBounds.max.x >= SIZE || mapBounds.max.y >= SIZE ) {
		GLOUTPUT(( "Out of bounds.\n" ));
		return 0;
	}

	// Check for duplicate. Only check for exact dupe - same origin & rotation.
	// This isn't required, but prevents map creation mistakes.
	MapItem* root = quadTree.FindItems( mapBounds, 0, 0 );
	while( root ) {
		if ( root->def == def && root->XForm() == xform ) {
			GLOUTPUT(( "Duplicate layer.\n" ));
			return 0;
		}
		root = root->next;
	}

	// Finally add!!
	MapItem* item = (MapItem*) itemPool.Alloc();

	item->model = 0;
	if ( hp == -1 )
		hp = def->hp;

	item->SetXForm( xform );
	item->def = def;
	item->open = 0;
	item->amountObscuring = 0;
	item->modelX = modelPos.x;
	item->modelZ = modelPos.z;
	item->modelRotation = rotation;

	item->hp = hp;
	if ( def->IsDoor() )
		flags |= MapItem::MI_DOOR;
	item->flags = flags;

	GLRELASSERT( mapBounds.min.x >= 0 && mapBounds.max.x < 256 );	// using int8
	GLRELASSERT( mapBounds.min.y >= 0 && mapBounds.max.y < 256 );	// using int8
	item->mapBounds8.Set( mapBounds.min.x, mapBounds.min.y, mapBounds.max.x, mapBounds.max.y );
	
	item->next = 0;
	
	quadTree.Add( item );

	const ModelResource* res = 0;
	// Clean up XML errors
	if ( !def->CanDamage() )
		hp = 0xffff;

	if ( def->CanDamage() && hp == 0 ) {
		res = def->GetDestroyedResource();
	}
	else {
		res = def->GetModelResource();
		if ( def->flags & MapItemDef::OBSCURES ) {
			ChangeObscured( mapBounds, 1 );
			item->amountObscuring = 1;
		}
	}
	if ( res ) {
		Model* model = tree->AllocModel( res );

		model->SetFlag( Model::MODEL_OWNED_BY_MAP );
		if ( metadata )
			model->SetFlag( Model::MODEL_METADATA );
		model->SetPos( modelPos );
		model->SetRotation( item->ModelRot() );
		item->model = model;
	}

	// Patch the world states:
	ResetPath();
	ClearVisPathMap( mapBounds );
	CalcVisPathMap( mapBounds );
	return item;
}


void Map::DeleteItem( MapItem* item )
{	
	GLRELASSERT( item );

	// Delete the light first, if it exists:
	quadTree.UnlinkItem( item );
	Rectangle2I mapBounds = item->MapBounds();
	if ( item->amountObscuring ) {
		ChangeObscured( mapBounds, -((int)item->amountObscuring) );
	}
	if ( item->model )
		tree->FreeModel( item->model );

	itemPool.Free( item );
	ResetPath();
	ClearVisPathMap( mapBounds );
	CalcVisPathMap( mapBounds );
}


void Map::Save( XMLPrinter* printer )
{

	printer->OpenElement( "Map" );
	printer->PushAttribute( "sizeX", width );
	printer->PushAttribute( "sizeY", height );

	if ( !Engine::mapMakerMode ) {

		printer->OpenElement( "Seen" );
	
		char buf[BitArray<Map::SIZE, Map::SIZE, 1>::STRING_SIZE];
		pastSeenFOW.ToString( buf );
		printer->PushText( buf );

		printer->CloseElement();	// Seen
	}

	SubSave( printer );

	printer->OpenElement( "Images" );

	for( int i=0; i<nImageData; ++i ) {
		printer->OpenElement( "Image" );

		printer->PushAttribute( "x", imageData[i].x );
		printer->PushAttribute( "y", imageData[i].y );
		printer->PushAttribute( "size", imageData[i].size );
		printer->PushAttribute( "tileRotation", imageData[i].tileRotation );
		printer->PushAttribute( "name", imageData[i].name.c_str() );

		printer->CloseElement();	// Image
	}
	printer->CloseElement();	// Images

	printer->OpenElement( "PyroGroup" );
	for( int i=0; i<SIZE*SIZE; ++i ) {
		if ( pyro[i] ) {
			int y = i / SIZE;
			int x = i - SIZE*y;

			printer->OpenElement( "Pyro" );
			printer->PushAttribute( "x", x );
			printer->PushAttribute( "y", y );
			printer->PushAttribute( "fire", PyroFire( x, y ) ? 1 : 0 );
			printer->PushAttribute( "flare", PyroFlare( x, y ) ? 1 : 0 );
			printer->PushAttribute( "duration", PyroDuration( x, y ) );
			printer->CloseElement();	// Pyro
		}
	}
	printer->CloseElement();	// PyroGroup
	printer->CloseElement();	// Map
}


void Map::Load( const XMLElement* mapElement )
{
	GLASSERT( mapElement );
	if ( strcmp( mapElement->Value(), "Game" ) == 0 ) {
		mapElement = mapElement->FirstChildElement( "Map" );
	}
	GLRELASSERT( mapElement );
	GLASSERT( strcmp( mapElement->Value(), "Map" ) == 0 );
	int sizeX = 64;
	int sizeY = 64;

	mapElement->QueryIntAttribute( "sizeX", &sizeX );
	mapElement->QueryIntAttribute( "sizeY", &sizeY );
	SetSize( sizeX, sizeY );
	nImageData = 0;

	pastSeenFOW.ClearAll();
	const XMLElement* pastSeenElement = mapElement->FirstChildElement( "Seen" );
	if ( pastSeenElement ) {
		const char* p = pastSeenElement->GetText();
		if ( p ) {
			pastSeenFOW.FromString( p );
		}
	}

	const XMLElement* imagesElement = mapElement->FirstChildElement( "Images" );
	if ( imagesElement ) {
		for(	const XMLElement* image = imagesElement->FirstChildElement( "Image" );
				image;
				image = image->NextSiblingElement( "Image" ) )
		{
			int x=0, y=0, size=0;
			char buffer[128];
			int tileRotation = 0;
			image->QueryIntAttribute( "x", &x );
			image->QueryIntAttribute( "y", &y );
			image->QueryIntAttribute( "size", &size );
			image->QueryIntAttribute( "tileRotation", &tileRotation );

			// store it to save later:
			const char* name = image->Attribute( "name" ); 
			GLRELASSERT( nImageData < MAX_IMAGE_DATA );
			imageData[ nImageData ].x = x;
			imageData[ nImageData ].y = y;
			imageData[ nImageData ].size = size;
			imageData[ nImageData ].tileRotation = tileRotation;
			imageData[ nImageData ].name = name;
			++nImageData;
			
			ImageManager* imageManager = ImageManager::Instance();

			Surface background, day, night;

			SNPrintf( buffer, 128, "%s_TEX", name );
			imageManager->LoadImage( buffer, &background );
			SNPrintf( buffer, 128, "%s_DAY", name );
			imageManager->LoadImage( buffer, &day );
			SNPrintf( buffer, 128, "%s_NGT", name );
			imageManager->LoadImage( buffer, &night );

			SetTexture( &background, x*512/64, y*512/64, tileRotation );
			SetLightMaps( &day, &night, x, y, tileRotation );			
		}
	}

	SubLoad( mapElement );

	const XMLElement* pyroGroupElement = mapElement->FirstChildElement( "PyroGroup" );
	if ( pyroGroupElement ) {
		for( const XMLElement* pyroElement = pyroGroupElement->FirstChildElement( "Pyro" );
			 pyroElement;
			 pyroElement = pyroElement->NextSiblingElement( "Pyro" ) )
		{
			int x=0, y=0, fire=0, duration=0, flare=0;
			pyroElement->QueryIntAttribute( "x", &x );
			pyroElement->QueryIntAttribute( "y", &y );
			pyroElement->QueryIntAttribute( "fire", &fire );
			pyroElement->QueryIntAttribute( "flare", &flare );
			pyroElement->QueryIntAttribute( "duration", &duration );
			SetPyro( x, y, duration, fire ? true : false, flare ? true : false );
		}
	}

	// Remove things that can't burn. (We don't want to generate unnecessary particles.)
	for( int y=0; y<SIZE; ++y ) {
		for( int x=0; x<SIZE; ++x ) {
			if ( PyroFire( x, y ) ) {
				MapItem* root = quadTree.FindItems( x, y, 0, 0 );
				while ( root && ( root->Destroyed() || !root->def->flammable ) )	// skip things that are destroyed or inflammable
					root = root->next;

				if ( !root ) {
					// a few sub-turns of smoke
					SetPyro( x, y, 0, false, false );
				}
			}
		}
	}
	QueryAllDoors();
}


void Map::DeleteAt( int x, int y )
{
	GLRELASSERT( x >= 0 && x < width );
	GLRELASSERT( y >= 0 && y < height );

	MapItem* item = quadTree.FindItems( x, y, 0, 0 );
	// Delete the *last* item so it behaves more naturally when editing.
	while( item && item->next )
		item = item->next;

	if ( item ) {
		DeleteItem( item );
	}
}


void Map::QueryAllDoors()
{
	Rectangle2I bounds = Bounds();

	doorArr.Clear();

	MapItem* item = quadTree.FindItems( bounds, MapItem::MI_DOOR, 0 );
	while( item ) {
		doorArr.Push( item );
		item = item->next;
	}
}


bool Map::ProcessDoors( const grinliz::Vector2I* openers, int nOpeners )
{
	//GRINLIZ_PERFTRACK
	bool anyChange = false;

	BitArray< SIZE, SIZE, 1 > map;
	Rectangle2I b;

	for( int i=0; i<nOpeners; ++i ) {
		b.Set( openers[i].x-1, openers[i].y-1, openers[i].x+1, openers[i].y+1 );
		b.DoIntersection( Bounds() );
		map.SetRect( b );
	}
	for( int i=0; i<doorArr.Size(); ++i ) {

		MapItem* item = doorArr[i];

		GLRELASSERT( item->def->IsDoor() );
		GLRELASSERT( item->def->resourceOpen );
		GLRELASSERT( item->def->cx == 1 && item->def->cy == 1 );

		if ( !item->Destroyed() ) {
			GLRELASSERT( item->model );

			bool change = false;

			// Cheat on transform! Doors are 1x1
			int x = item->XForm().x;
			int y = item->XForm().y;
			bool shouldBeOpen = map.IsSet( x, y ) != 0;

			if ( item->open && !shouldBeOpen ) {
				change = true;
			}
			else if ( !item->open && shouldBeOpen ) {
				change = true;
			}

			if ( change ) {
				anyChange = true;
				Vector3F pos = item->model->Pos();
				float rot = item->model->GetRotation();

				const ModelResource* res = 0;
				if ( shouldBeOpen ) {
					item->open = 1;
					res = item->def->GetOpenResource();
				}
				else {
					item->open = 0;
					res = item->def->GetModelResource();
				}

				if ( res ) {
					tree->FreeModel( item->model );

					Model* model = tree->AllocModel( res );
					model->SetFlag( Model::MODEL_OWNED_BY_MAP );
					model->SetPos( pos );
					model->SetRotation( rot );
					item->model = model;

					Rectangle2I mapBounds = item->MapBounds();

					ResetPath();
					ClearVisPathMap( mapBounds );
					CalcVisPathMap( mapBounds );
				}
			}
		}
	}
	return anyChange;
}




void Map::DumpTile( int x, int y )
{
	if ( InRange( x, 0, SIZE-1 ) && InRange( y, 0, SIZE-1 )) 
	{
		int i=0;
		MapItem* root = quadTree.FindItems( x, y, 0, 0 );
		while ( root ) {
			const MapItemDef& itemDef = *root->def;
			GLASSERT( itemDef.Name() );

			int r = root->modelRotation;
			UFOText::Instance()->Draw( 0, 100-12*i, "%s r=%d", itemDef.Name(), r );

			++i;
			root = root->next;
		}
		if ( PyroSmoke( x, y ) )
			UFOText::Instance()->Draw( 0, 100-12*i, "smoke" );
		if ( PyroFire( x, y ) )
			UFOText::Instance()->Draw( 0, 100-12*i, "fire" );
	}
}


void Map::DrawPath( int mode )
{
	CompositingShader shader( true );

	for( int j=0; j<SIZE; ++j ) {
		for( int i=0; i<SIZE; ++i ) {
			float x = (float)i;
			float y = (float)j;

			int path = GetPathMask( mode == 2 ? VISIBILITY_TYPE : PATH_TYPE, i, j );
			if ( path == 0 ) 
				continue;

			Vector3F red[12];
			Vector3F green[12];
			static const uint16_t index[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
			int	nRed = 0, nGreen = 0;	
			const float EPS = 0.2f;

			Vector3F edge[5] = {
				{ x, EPS, y+1.0f },
				{ x+1.0f, EPS, y+1.0f },
				{ x+1.0f, EPS, y },
				{ x, EPS, y },
				{ x, EPS, y+1.0f }
			};
			Vector3F center = { x+0.5f, 0, y+0.5f };

			for( int k=0; k<4; ++k ) {
				int mask = (1<<k);

				if ( path & mask ) {
					red[nRed+0] = edge[k];
					red[nRed+1] = edge[k+1];
					red[nRed+2] = center;
					nRed += 3;
				}
				else {
					green[nGreen+0] = edge[k];
					green[nGreen+1] = edge[k+1];
					green[nGreen+2] = center;
					nGreen += 3;
				}
			}
			GLRELASSERT( nRed <= 12 );
			GLRELASSERT( nGreen <= 12 );
			
			GPUStream stream;
			stream.stride = sizeof(Vector3F);
			stream.nPos = 3;
			stream.posOffset = 0;

			shader.SetColor( 1, 0, 0, 0.5f );
			shader.SetStream( stream, red, nRed, index );
			shader.Draw();

			shader.SetColor( 0, 1, 0, 0.5f );
			shader.SetStream( stream, green, nGreen, index );
			shader.Draw();
		}
	}
}


void Map::ResetPath()
{
	microPather->Reset();
	++pathQueryID;
	++visibilityQueryID;
}


void Map::SetPathBlocks( const grinliz::BitArray<Map::SIZE, Map::SIZE, 1>& block )
{
	if ( block != pathBlock ) {
		ResetPath();
		pathBlock = block;
	}
}


void Map::ClearVisPathMap( grinliz::Rectangle2I& _bounds )
{
	Rectangle2I bounds = _bounds;
	bounds.DoIntersection( Bounds() );

	for( int j=bounds.min.y; j<=bounds.max.y; ++j ) {
		for( int i=bounds.min.x; i<=bounds.max.x; ++i ) {
			visMap[j*SIZE+i] = 0;
			pathMap[j*SIZE+i] = 0;
		}
	}
}


void Map::CalcVisPathMap( grinliz::Rectangle2I& _bounds )
{
	GRINLIZ_PERFTRACK
	Rectangle2I bounds = _bounds;
	bounds.DoIntersection( Bounds() );

	MapItem* item = quadTree.FindItems( bounds, 0, 0 );
	while( item ) {
		if ( !item->Destroyed() ) {

			const MapItemDef& itemDef = *item->def;

			int rot = item->modelRotation;
			GLRELASSERT( rot >= 0 && rot < 4 );

			Matrix2I mat = item->XForm();

			// Walk the object in object space & write to world.
			for( int j=0; j<itemDef.cy; ++j ) {
				for( int i=0; i<itemDef.cx; ++i ) {
					
					Vector2I obj = { i, j };
					Vector2I world = mat * obj;

					GLRELASSERT( world.x >= 0 && world.x < SIZE );
					GLRELASSERT( world.y >= 0 && world.y < SIZE );

					// Account for tile rotation. (Actually a bit rotation too, which is handy.)
					// The OR operation is important. This routine will write outside of the bounds,
					// and should do no damage.
					//
					// Open doors don't impede sight or movement.
					//
					if ( !item->open )
					{
						// Path
						U32 p = ( itemDef.Pather(obj.x, obj.y ) << rot );
						p = p | (p>>4);
						pathMap[ world.y*SIZE + world.x ] |= p;

						// Visibility
						p = ( itemDef.Visibility( obj.x, obj.y ) << rot );
						p = p | (p>>4);
						visMap[ world.y*SIZE + world.x ] |= p;
					}
				}
			}
		}
		item = item->next;
	}
}


int Map::GetPathMask( ConnectionType c, int x, int y )
{
	// fast return: if the c is set, we're done.
	if ( c == PATH_TYPE && pathBlock.IsSet( x, y ) ) {
		return 0xf;
	}
	return ( c==PATH_TYPE) ? pathMap[y*SIZE+x] : visMap[y*SIZE+x];
}


float Map::LeastCostEstimate( void* stateStart, void* stateEnd )
{
	Vector2<S16> start, end;
	StateToVec( stateStart, &start );
	StateToVec( stateEnd, &end );

	float dx = (float)(start.x-end.x);
	float dy = (float)(start.y-end.y);

	return sqrtf( dx*dx + dy*dy );
}


bool Map::Connected4(	ConnectionType c, 
						const grinliz::Vector2<S16>& pos,
						const grinliz::Vector2<S16>& delta )
{

//	GLRELASSERT( dir >= 0 && dir < 4 );

	static const int dirArr[9] = {  0, 2, 0,
									3, 0, 1,
									0, 0, 0 };
	const int index = (delta.x+1) + (delta.y+1)*3;

	// This method only checks in the 4 directions.
	GLASSERT( index == 1 || index == 3 || index == 5 || index == 7 );
	const int dir = dirArr[index];
	const int bit = 1<<dir;

	const Vector2I nextPos = { pos.x + delta.x, pos.y + delta.y };
	const Rectangle2I mapBounds = Bounds();
	GLASSERT( pos.x >= 0 && pos.x <= mapBounds.max.x && pos.y >= 0 && pos.y <= mapBounds.max.y );

	// To be connected, it must be clear from a->b and b->a
	if ( mapBounds.Contains( nextPos ) ) {
		const int mask0 = GetPathMask( c, pos.x, pos.y );
		const int maskN = GetPathMask( c, nextPos.x, nextPos.y );
		const int inv   = InvertPathMask( bit );

		if ( (( mask0 & bit ) == 0 ) && (( maskN & inv ) == 0 ) ) {
			return true;
		}
	}
	return false;
}


bool Map::Connected8(	ConnectionType c, 
						const grinliz::Vector2<S16>& pos,
						const grinliz::Vector2<S16>& delta )
{
	int need8 = delta.x*delta.y;	// if both are set, it is a diagonal
	if ( need8 ) {
		const Rectangle2I mapBounds = Bounds();
		if ( mapBounds.Contains( (int)(pos.x+delta.x), (int)(pos.y+delta.y) )) {
			const grinliz::Vector2<S16> delta0 = { delta.x, 0 };
			const grinliz::Vector2<S16> delta1 = { 0, delta.y };

			// If pathing, both directions have to be open. For sight, only one.
			if ( c == PATH_TYPE ) {
				return    Connected4( c, pos, delta0 )
					   && Connected4( c, pos+delta0, delta1 )
					   && Connected4( c, pos, delta1 )
					   && Connected4( c, pos+delta1, delta0 );
			}
			else {
				return	  ( Connected4( c, pos, delta0 ) && Connected4( c, pos+delta0, delta1 ))
					   || ( Connected4( c, pos, delta1 ) && Connected4( c, pos+delta1, delta0 ));
			}
		}
		return false;
	}
	return Connected4( c, pos, delta );
}

void Map::AdjacentCost( void* state, MP_VECTOR< micropather::StateCost > *adjacent )
{
	Vector2<S16> pos;
	StateToVec( state, &pos );

	const Vector2<S16> next[8] = {
		{ 0, 1 },
		{ 1, 0 },
		{ 0, -1 },
		{ -1, 0 },

		{ 1, 1 },
		{ 1, -1 },
		{ -1, -1 },
		{ -1, 1 },
	};

	adjacent->resize( 0 );
	// N S E W
	for( int i=0; i<4; i++ ) {
		if ( Connected4( PATH_TYPE, pos, next[i] ) ) {
			Vector2<S16> nextPos = pos + next[i];

			micropather::StateCost stateCost;
			stateCost.cost = 1.0f;
			stateCost.state = VecToState( nextPos );
			adjacent->push_back( stateCost );
		}
	}
	// Diagonals. Need to check if all the NSEW connections work. If
	// so, then the diagonal connection works too.
	for( int i=4; i<8; i++ ) {
		if ( Connected8( PATH_TYPE, pos, next[i] ) ) {
			Vector2<S16> nextPos = pos + next[i];

			micropather::StateCost stateCost;
			stateCost.cost = SQRT2;
			stateCost.state = VecToState( nextPos );
			adjacent->push_back( stateCost );
		}
	}
}


void Map::PrintStateInfo( void* state )
{
	Vector2<S16> pos;
	StateToVec( state, &pos );
	GLOUTPUT(( "[%d,%d]", pos.x, pos.y ));
}


int Map::SolvePath( const void* user, const Vector2<S16>& start, const Vector2<S16>& end, float *cost, MP_VECTOR< Vector2<S16> >* path )
{
	GRINLIZ_PERFTRACK
	//GLRELASSERT( sizeof( int ) == sizeof( void* ));			// fix this for 64 bit
	GLRELASSERT( sizeof(Vector2<S16>) == sizeof( void* ) );
	GLRELASSERT( pathBlocker );
	if ( pathBlocker ) {
		pathBlocker->MakePathBlockCurrent( this, user );
	}

	// FIXME: optimization
	// check that dest isn't path blocked
	// check that dest is surrounded by path blocks / blocks
	// check that start isn't surrounded by path blocks / blocks

	int result = microPather->Solve(	VecToState( start ),
										VecToState( end ),
										reinterpret_cast< MP_VECTOR<void*>* >( path ),		// sleazy trick if void* is the same size as V2<S16>
										cost );

#if 0
#ifdef DEBUG
	{
		for( unsigned i=0; i<path->size(); ++i ) {
			GLASSERT( !pathBlock.IsSet( (*path)[i].x, (*path)[i].y ) );
		}
	}
#endif
#endif

	/*
	switch( result ) {
		case MicroPather::SOLVED:			GLOUTPUT(( "Solved nPath=%d\n", *nPath ));			break;
		case MicroPather::NO_SOLUTION:		GLOUTPUT(( "NoSolution nPath=%d\n", *nPath ));		break;
		case MicroPather::START_END_SAME:	GLOUTPUT(( "StartEndSame nPath=%d\n", *nPath ));	break;
		case MicroPather::OUT_OF_MEMORY:	GLOUTPUT(( "OutOfMemory nPath=%d\n", *nPath ));		break;
		default:	GLASSERT( 0 );	break;
	}
	*/

	return result;
}


bool Map::InStateCost( int x, int y ) const
{
	Vector2<S16> v = {x,y};
	void* state = VecToState( v );
	for( unsigned i=0; i<stateCostArr.size(); ++i ) {
		if ( stateCostArr[i].state == state )
			return true;
	}
	return false;
}


bool Map::InStateCostBounds( int x, int y ) const
{
	Rectangle2I r;
	r.SetInvalid();

	for( unsigned i=0; i<stateCostArr.size(); ++i ) {
		Vector2<S16> pos;
		StateToVec( stateCostArr[i].state, &pos );
		r.DoUnion( pos.x, pos.y );
	}
	return r.Contains( x, y );
}


void Map::ShowNearPath(	const grinliz::Vector2I& unitPos,
						const void* user, 
						const grinliz::Vector2<S16>& start, 
					    float maxCost,
					    const grinliz::Vector2F* range,
						const grinliz::Vector2<S16>* dest )
{
	if ( pathBlocker ) {
		pathBlocker->MakePathBlockCurrent( this, user );
	}
	stateCostArr.clear();
	mpVector.clear();

	if ( dest ) {
		float total;
		int result = microPather->Solve( VecToState( start ), VecToState( *dest ), &mpVector, &total );

		GLASSERT( total <= maxCost );
		if ( result != micropather::MicroPather::SOLVED ) {
			mpVector.clear();			
		}
	}
	microPather->SolveForNearStates( VecToState( start ), &stateCostArr, maxCost );


	/*
	GLOUTPUT(( "Near states, result=%d\n", result ));
	for( unsigned m=0; m<stateCostArr.size(); ++m ) {
		Vector2<S16> v;
		StateToVec( stateCostArr[m].state, &v );
		GLOUTPUT(( "  (%d,%d) cost=%.1f\n", v.x, v.y, stateCostArr[m].cost ));
	}
	*/

	int nWalkingMaps = SettingsManager::Instance()->GetNumWalkingMaps();

	Vector2I origin = { (unitPos.x - EL_MAP_MAX_PATH), (unitPos.y - EL_MAP_MAX_PATH) };
	for( int i=0; i<MAX_WALKING_MAPS; ++i ) {
		walkingMap[i].SetVisible( i < nWalkingMaps );
		walkingMap[i].Clear();
		walkingMap[i].SetSize( (float)(2*EL_MAP_MAX_PATH+1), (float)(2*EL_MAP_MAX_PATH+1) );
		walkingMap[i].SetPos( (float)origin.x, (float)origin.y );
	}
	gamui::RenderAtom atom[6];
	InitWalkingMapAtoms( atom, nWalkingMaps );

	for( unsigned i=0; i<stateCostArr.size(); ++i ) {
		const micropather::StateCost& stateCost = stateCostArr[i];
		Vector2<S16> v;
		StateToVec( stateCost.state, &v );

		// don't draw where standing.
		if ( v.x == unitPos.x && v.y == unitPos.y )
			continue;

		// If a destination is set, only draw on the path.
		if ( mpVector.size() ) {
			bool found = false;
			for( unsigned v=0; v<mpVector.size(); ++v ) {
				if ( mpVector[v] == stateCost.state ) {
					found = true;
					break;
				}
			}
			if ( !found )
				continue;
		}

		for( int k=0; k<3; ++k ) {
			if ( stateCost.cost >= range[k].x && stateCost.cost < range[k].y ) {
				for( int n=0; n<nWalkingMaps; ++n ) {
					walkingMap[n].SetTile( v.x-origin.x, v.y-origin.y, atom[k+n*3] );
				}
				break;
			}
		}
	}

}


void Map::ClearNearPath()
{
	for( int i=0; i<MAX_WALKING_MAPS; ++i ) {
		walkingMap[i].SetVisible( false );
	}
}


bool Map::CanSee( const grinliz::Vector2I& p, const grinliz::Vector2I& q, ConnectionType connection )
{
	GLRELASSERT( InRange( q.x-p.x, -1, 1 ) );
	GLRELASSERT( InRange( q.y-p.y, -1, 1 ) );
	if ( p.x < 0 || p.x >= SIZE || p.y < 0 || p.y >= SIZE )
		return false;
	if ( q.x < 0 || q.x >= SIZE || q.y < 0 || q.y >= SIZE )
		return false;

	Vector2<S16> p16 = { p.x, p.y };
	Vector2<S16> d16 = { q.x-p.x, q.y-p.y };
	return Connected8( connection, p16, d16 );
}


void Map::MapBoundsOfModel( const Model* m, grinliz::Rectangle2I* mapBounds )
{
	GLASSERT( m->IsFlagSet( Model::MODEL_OWNED_BY_MAP ) );
	MapItem* item = quadTree.FindItem( m );
	GLRELASSERT( item );
	*mapBounds = item->MapBounds();
	GLRELASSERT( mapBounds->min.x <= mapBounds->max.x );
	GLRELASSERT( mapBounds->min.y <= mapBounds->max.y );
}


void Map::ChangeObscured( const grinliz::Rectangle2I& bounds, int delta )
{
	for( int y=bounds.min.y; y<=bounds.max.y; ++y ) {
		for( int x=bounds.min.x; x<=bounds.max.x; ++x ) {
			obscured[y*SIZE+x] += delta;
		}
	}
}

/*
void Map::SetLanderFlight( float normal )
{
	landerFlight = Clamp( normal, 0.0f, 1.0f );
	if ( lander ) {
		Model* model = lander->model;
		Vector3F pos = model->Pos();
		float h = landerFlight*landerFlight * 10.0f;
		
		const Rectangle3F& bounds = model->AABB();
		Vector3F p[4] = {	{ bounds.min.x, h, bounds.min.z },
							{ bounds.max.x, h, bounds.min.z },
							{ bounds.min.x, h, bounds.max.z },
							{ bounds.max.x, h, bounds.max.z } };

		Color4F color[3] = { { 213.0f/255.0f, 63.0f/255.0f, 63.0f/255.0f, 1.0f },
							 { 248.0f/255.0f, 228.0f/255.0f, 8.0f/255.0f, 1.0f },
							 { 1, 0, 0, 1 } };
		Color4F colorV = { 0, 0, 0, -0.3f };
		Vector3F v = { 0, 0, 0 };

		for( int i=0; i<4; ++i ) {
			for( int k=0; k<3; ++k )
				ParticleSystem::Instance()->EmitPoint( 1, ParticleSystem::PARTICLE_SPHERE, color[k], colorV, p[i], 0.8f, v, 0.4f );
		}

		model->SetPos( pos.x, h, pos.z );
	}
}
*/



Map::QuadTree::QuadTree()
{
	Clear();
	filterModel = 0;

	int base = 0;
	for( int i=0; i<QUAD_DEPTH+1; ++i ) {
		depthBase[i] = base;
		base += (1<<i)*(1<<i);
	}
}


void Map::QuadTree::Clear()
{
	memset( tree, 0, sizeof(MapItem*)*NUM_QUAD_NODES );
	memset( depthUse, 0, sizeof(int)*(QUAD_DEPTH+1) );
}


void Map::QuadTree::Add( MapItem* item )
{
	int d=0;
	int i = CalcNode( item->mapBounds8, &d );
	item->nextQuad = tree[i];
	tree[i] = item;
	depthUse[d] += 1;

//	GLOUTPUT(( "QuadTree::Add %x id=%d (%d,%d)-(%d,%d) at depth=%d\n", item, item->itemDefIndex,
//				item->mapBounds8.min.x, item->mapBounds8.min.y, item->mapBounds8.max.x, item->mapBounds8.max.y,
//				d ));

	//GLOUTPUT(( "Depth: %2d %2d %2d %2d %2d\n", 
	//		   depthUse[0], depthUse[1], depthUse[2], depthUse[3], depthUse[4] ));
}


void Map::QuadTree::UnlinkItem( MapItem* item )
{
	int d=0;
	int index = CalcNode( item->mapBounds8, &d );
	depthUse[d] -= 1;
	GLRELASSERT( tree[index] ); // the item should be in the linked list somewhere.

//	GLOUTPUT(( "QuadTree::UnlinkItem %x id=%d\n", item, item->itemDefIndex ));

	MapItem* prev = 0;
	for( MapItem* p=tree[index]; p; prev=p, p=p->nextQuad ) {
		if ( p == item ) {
			if ( prev )
				prev->nextQuad = p->nextQuad;
			else
				tree[index] = p->nextQuad;
			return;
		}
	}
	GLASSERT( 0 );	// should have found the item.
}


Map::MapItem* Map::QuadTree::FindItems( const Rectangle2I& bounds, int required, int excluded )
{
	//GRINLIZ_PERFTRACK
	// Walk the map and pull out items in bounds.
	MapItem* root = 0;

	GLASSERT( bounds.min.x >= 0 && bounds.max.x < 256 );	// using int8
	GLASSERT( bounds.min.y >= 0 && bounds.max.y < 256 );	// using int8
	GLASSERT( bounds.min.x >= 0 && bounds.max.x < EL_MAP_SIZE );
	GLASSERT( bounds.min.y >= 0 && bounds.max.y < EL_MAP_SIZE );	// using int8
	Rectangle2<U8> bounds8;
	bounds8.Set( bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y );

	for( int depth=0; depth<QUAD_DEPTH; ++depth ) 
	{
		// Translate into coordinates for this node level:
		int x0 = WorldToNode( bounds.min.x, depth );
		int x1 = WorldToNode( bounds.max.x, depth );
		int y0 = WorldToNode( bounds.min.y, depth );
		int y1 = WorldToNode( bounds.max.y, depth );

		// i, j, x0.. all in node coordinates.
		for( int j=y0; j<=y1; ++j ) {
			for( int i=x0; i<=x1; ++i ) {
				MapItem* pItem = *(tree + depthBase[depth] + NodeOffset( i, j, depth ) );

				if ( filterModel ) {
					while( pItem ) {
						if ( pItem->model == filterModel ) {
							pItem->next = 0;
							return pItem;
						}
						pItem = pItem->nextQuad;
					}
				}
				else {
					while( pItem ) { 
						if (    ( ( pItem->flags & required) == required )
							 && ( ( pItem->flags & excluded ) == 0 )
							 && pItem->mapBounds8.Intersect( bounds8 ) )
						{
							pItem->next = root;
							root = pItem;
						}
						pItem = pItem->nextQuad;
					}
				}
			}
		}
	}
	return root;
}


Map::MapItem* Map::QuadTree::FindItem( const Model* model )
{
	GLRELASSERT( model->IsFlagSet( Model::MODEL_OWNED_BY_MAP ) );
	Rectangle2I b;

	// May or may not have 'mapBoundsCache' when called.
//	if ( model->mapBoundsCache.min.x >= 0 ) {
//		b = model->mapBoundsCache;
//	}
//	else {
		// Need a little space around the coordinates to account
		// for object rotation.
		b.min.x = Clamp( (int)model->X()-2, 0, SIZE-1 );
		b.min.y = Clamp( (int)model->Z()-2, 0, SIZE-1 );
		b.max.x = Clamp( (int)model->X()+2, 0, SIZE-1 );
		b.max.y = Clamp( (int)model->Z()+2, 0, SIZE-1 );
//	}
	filterModel = model;
	MapItem* root = FindItems( b, 0, 0 );
	filterModel = 0;
	GLRELASSERT( root && root->next == 0 );
	return root;
}


int Map::QuadTree::CalcNode( const Rectangle2<U8>& bounds, int* d )
{
	int offset = 0;

	for( int depth=QUAD_DEPTH-1; depth>0; --depth ) 
	{
		int x0 = WorldToNode( bounds.min.x, depth );
		int y0 = WorldToNode( bounds.min.y, depth );

		int wSize = SIZE >> depth;

		Rectangle2<U8> wBounds;
		wBounds.Set(	NodeToWorld( x0, depth ),
						NodeToWorld( y0, depth ),
						NodeToWorld( x0, depth ) + wSize - 1,
						NodeToWorld( y0, depth ) + wSize - 1 );

		if ( wBounds.Contains( bounds ) ) {
			offset = depthBase[depth] + NodeOffset( x0, y0, depth );
			if ( d )
				*d = depth;
			break;
		}
	}
	GLRELASSERT( offset >= 0 && offset < NUM_QUAD_NODES );
	return offset;
}


void Map::QuadTree::MarkVisible( const grinliz::BitArray<Map::SIZE, Map::SIZE, 1>& fogOfWar )
{
	Rectangle2I mapBounds;
	mapBounds.Set( 0, 0, SIZE-1, SIZE-1 );
	MapItem* root = FindItems( mapBounds, 0, 0 );

	for( MapItem* item=root; item; item=item->next ) {
		
		Rectangle2I b = item->MapBounds();
		if ( b.min.x > 0 ) b.min.x--;
		if ( b.max.x < SIZE-1 ) b.max.x++;
		if ( b.min.y > 0 ) b.min.y--;
		if ( b.max.y < SIZE-1 ) b.max.y++;

		if ( fogOfWar.IsRectEmpty( b ) ) {
			if ( item->model ) item->model->SetFlag( Model::MODEL_INVISIBLE );
		}
		else {
			if ( item->model ) item->model->ClearFlag( Model::MODEL_INVISIBLE );
		}
	}
}




void Map::CreateTexture( Texture* t )
{
	if ( t == backgroundTexture ) {
		t->Upload( backgroundSurface );
	}
	else if ( t == greyTexture ) {
		t->Upload( greySurface );
	}
	else if ( t == lightMapTex ) {
		t->Upload( *lightMap );
	}
	else if ( t == lightFogMapTex ) {
		t->Upload( lightFogMap );
	}
	else {
		GLRELASSERT( 0 );
	}
}


void Map::BeginRender()
{
	gamuiShader.PushMatrix( GPUShader::MODELVIEW_MATRIX );

	Matrix4 rot;
	rot.SetXRotation( 90 );
	gamuiShader.MultMatrix( GPUShader::MODELVIEW_MATRIX, rot );
}


void Map::EndRender()
{
	gamuiShader.PopMatrix( GPUShader::MODELVIEW_MATRIX );
}


void Map::BeginRenderState( const void* renderState )
{
	const float ALPHA = 0.5f;
	const float ALPHA_1 = 0.30f;
	switch( (intptr_t)renderState ) {
		case UIRenderer::RENDERSTATE_UI_NORMAL_OPAQUE:
		case RENDERSTATE_MAP_OPAQUE:
			gamuiShader.SetColor( 1, 1, 1, 1 );
			gamuiShader.SetBlend( false );
			break;

		case UIRenderer::RENDERSTATE_UI_NORMAL:
		case RENDERSTATE_MAP_NORMAL:
			gamuiShader.SetColor( 1.0f, 1.0f, 1.0f, 0.8f );
			gamuiShader.SetBlend( true );
			break;

		case RENDERSTATE_MAP_TRANSLUCENT:
			gamuiShader.SetColor( 1, 1, 1, ALPHA );
			gamuiShader.SetBlend( true );
			break;
		case RENDERSTATE_MAP_MORE_TRANSLUCENT:
			gamuiShader.SetColor( 1, 1, 1, ALPHA_1 );
			gamuiShader.SetBlend( true );
			break;
		default:
			GLRELASSERT( 0 );
	}
}


void Map::BeginTexture( const void* textureHandle )
{
	gamuiShader.SetTexture0( (Texture*)textureHandle );
}


void Map::Render( const void* renderState, const void* textureHandle, int nIndex, const uint16_t* index, int nVertex, const gamui::Gamui::Vertex* vertex )
{
	GPUStream stream( GPUStream::kGamuiType );
	gamuiShader.SetStream( stream, vertex, nIndex, index );

	gamuiShader.Draw();
}
