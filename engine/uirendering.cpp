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

#include "uirendering.h"
#include "texture.h"
#include "../grinliz/glvector.h"
#include "text.h"

using namespace grinliz;
using namespace gamui;


void UIRenderer::BeginRender()
{
	// Should be completely uneeded, but fixes bugs on a netbook. (With questionable drivers.)
	GPUShader::ResetState();
}


void UIRenderer::EndRender()
{
}


void UIRenderer::BeginRenderState( const void* renderState )
{
	switch ( (int)renderState )
	{
	case RENDERSTATE_UI_NORMAL:
		shader.SetColor( 1, 1, 1, 1 );
		shader.SetBlend( true );
		break;

	case RENDERSTATE_UI_NORMAL_OPAQUE:
		shader.SetColor( 1, 1, 1, 1 );
		shader.SetBlend( false );
		break;

	case RENDERSTATE_UI_DISABLED:
		shader.SetColor( 1, 1, 1, 0.5f );
		shader.SetBlend( true );
		break;

	case RENDERSTATE_UI_TEXT:
		shader.SetColor( textRed, textGreen, textBlue, 1 );
		shader.SetBlend( true );
		break;

	case RENDERSTATE_UI_TEXT_DISABLED:
		shader.SetColor( textRed, textGreen, textBlue, 0.5f );
		shader.SetBlend( true );
		break;

	case RENDERSTATE_UI_DECO:
		shader.SetColor( 1, 1, 1, 0.7f );
		shader.SetBlend( true );
		break;

	case RENDERSTATE_UI_DECO_DISABLED:
		shader.SetColor( 1, 1, 1, 0.2f );
		shader.SetBlend( true );
		break;

	default:
		GLASSERT( 0 );
		break;
	}
}


void UIRenderer::BeginTexture( const void* textureHandle )
{
	Texture* texture = (Texture*)textureHandle;
	//glBindTexture( GL_TEXTURE_2D, texture->GLID() );
	shader.SetTexture0( texture );
}


void UIRenderer::Render( const void* renderState, const void* textureHandle, int nIndex, const uint16_t* index, int nVertex, const Gamui::Vertex* vertex )
{
//	shader.SetVertex( 2, sizeof(Gamui::Vertex), &vertex[0].x );
//	shader.SetTexture0( 2, sizeof(Gamui::Vertex), &vertex[0].tx );
	GPUShader::Stream stream( GPUShader::Stream::kGamuiType );
	shader.SetStream( stream, vertex, nIndex, index );

	shader.Draw();
}


void UIRenderer::SetAtomCoordFromPixel( int x0, int y0, int x1, int y1, int w, int h, RenderAtom* atom )
{
	atom->tx0 = (float)x0 / (float)w;
	atom->tx1 = (float)x1 / (float)w;

	atom->ty0 = (float)(h-y1) / (float)h;
	atom->ty1 = (float)(h-y0) / (float)h;
}


RenderAtom UIRenderer::CalcDecoAtom( int id, bool enabled )
{
	GLASSERT( id >= 0 && id < 32 );
	Texture* texture = TextureManager::Instance()->GetTexture( "iconDeco" );
	int y = id / 8;
	int x = id - y*8;
	float tx0 = (float)x / 8.f;
	float ty0 = (float)y / 4.f;
	float tx1 = tx0 + 1.f/8.f;
	float ty1 = ty0 + 1.f/4.f;

	return RenderAtom( (const void*)(enabled ? RENDERSTATE_UI_DECO : RENDERSTATE_UI_DECO_DISABLED), (const void*)texture, tx0, ty0, tx1, ty1, 64, 64 );
}


RenderAtom UIRenderer::CalcParticleAtom( int id, bool enabled )
{
	GLASSERT( id >= 0 && id < 16 );
	Texture* texture = TextureManager::Instance()->GetTexture( "particleQuad" );
	int y = id / 4;
	int x = id - y*4;
	float tx0 = (float)x / 4.f;
	float ty0 = (float)y / 4.f;
	float tx1 = tx0 + 1.f/4.f;
	float ty1 = ty0 + 1.f/4.f;

	return RenderAtom( (const void*)(enabled ? RENDERSTATE_UI_NORMAL : RENDERSTATE_UI_DISABLED), (const void*)texture, tx0, ty0, tx1, ty1, 64, 64 );
}


RenderAtom UIRenderer::CalcIconAtom( int id, bool enabled )
{
	GLASSERT( id >= 0 && id < 32 );
	Texture* texture = TextureManager::Instance()->GetTexture( "icons" );
	int y = id / 8;
	int x = id - y*8;
	float tx0 = (float)x / 8.f;
	float ty0 = (float)y / 4.f;
	float tx1 = tx0 + 1.f/8.f;
	float ty1 = ty0 + 1.f/4.f;

	return RenderAtom( (const void*)(enabled ? RENDERSTATE_UI_NORMAL : RENDERSTATE_UI_DISABLED), (const void*)texture, tx0, ty0, tx1, ty1, 64, 64 );
}


RenderAtom UIRenderer::CalcPaletteAtom( int c0, int c1, int blend, float w, float h, bool enabled )
{
	Vector2I c = { 0, 0 };
	Texture* texture = TextureManager::Instance()->GetTexture( "palette" );

	static const int offset[5] = { 75, 125, 175, 225, 275 };
	static const int subOffset[3] = { 0, -12, 12 };

	if ( blend == PALETTE_NORM ) {
		if ( c1 > c0 )
			Swap( &c1, &c0 );
		c.x = offset[c0];
		c.y = offset[c1];
	}
	else {
		if ( c0 > c1 )
			Swap( &c0, &c1 );

		if ( c0 == c1 ) {
			// first column special:
			c.x = 25 + subOffset[blend];
			c.y = offset[c1];
		}
		else {
			c.x = offset[c0] + subOffset[blend];;
			c.y = offset[c1];
		}
	}
	RenderAtom atom( (const void*)(enabled ? RENDERSTATE_UI_NORMAL : RENDERSTATE_UI_DISABLED), (const void*)texture, 0, 0, 0, 0, w, h );
	SetAtomCoordFromPixel( c.x, c.y, c.x, c.y, 300, 300, &atom );
	return atom;
}


void UIRenderer::GamuiGlyph( int c, IGamuiText::GlyphMetrics* metric )
{
	int advance=0;
	int width=0;
	Rectangle2I src;
	if ( c >= 32 && c < 128 ) {
		UFOText::Metrics( c-32, &advance, &width, &src );

		static const float CX = (1.f/256.f);
		static const float CY = (1.f/128.f);

		metric->advance = (float)advance;
		metric->width = (float)width;
		metric->height = 16.0f;

		metric->tx0 = (float)src.min.x * CX;
		metric->tx1 = (float)src.max.x * CX;

		metric->ty1 = (float)src.min.y * CY;
		metric->ty0 = (float)src.max.y * CY;
	}
	else {
		metric->advance = 1.0f;
		metric->width = 1.0f;
		metric->height = 16.0f;
		metric->tx0 = metric->ty0 = metric->tx1 = metric->ty1 = 0.0f;
	}
}

