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

#include "model.h"
#include "texture.h"
#include "loosequadtree.h"
#include "renderQueue.h"

#include "../grinliz/glvector.h"
#include "../grinliz/glstringutil.h"
#include "../grinliz/glperformance.h"

#include <float.h>

using namespace grinliz;


void ModelResource::Free()
{
	for( unsigned i=0; i<header.nGroups; ++i ) {
#ifdef EL_USE_VBO
		atom[i].vertexBuffer.Destroy();
		atom[i].indexBuffer.Destroy();
#endif
		memset( &atom[i], 0, sizeof( ModelAtom ) );
	}
	delete [] allVertex;
	delete [] allIndex;
}



void ModelResource::DeviceLoss()
{
#ifdef EL_USE_VBO
	for( unsigned i=0; i<header.nGroups; ++i ) {
		atom[i].vertexBuffer.Destroy();
		atom[i].indexBuffer.Destroy();
	}
#endif
}


int ModelResource::Intersect(	const grinliz::Vector3F& point,
								const grinliz::Vector3F& dir,
								grinliz::Vector3F* intersect ) const
{
	float t;
	int result = IntersectRayAABB( point, dir, header.bounds, intersect, &t );
	if ( result == grinliz::INTERSECT || result == grinliz::INSIDE ) {
		float close2 = FLT_MAX;
		Vector3F test;

		for( unsigned i=0; i<header.nGroups; ++i ) {
			for( unsigned j=0; j<atom[i].nIndex; j+=3 ) {
				int r = IntersectRayTri( point, dir, 
										 atom[i].vertex[ atom[i].index[j+0] ].pos,
										 atom[i].vertex[ atom[i].index[j+1] ].pos,
										 atom[i].vertex[ atom[i].index[j+2] ].pos,
										 &test );
				if ( r == grinliz::INTERSECT ) {
					float c2 =  (point.x-test.x)*(point.x-test.x) +
								(point.y-test.y)*(point.y-test.y) +
								(point.z-test.z)*(point.z-test.z);
					if ( c2 < close2 ) {
						close2 = c2;
						*intersect = test;
					}
				}
			}
		}
		if ( close2 < FLT_MAX ) {
			return grinliz::INTERSECT;
		}
	}
	return grinliz::REJECT;
}


void ModelLoader::Load( const gamedb::Item* item, ModelResource* res )
{
	res->header.Load( item );

	res->allVertex = new Vertex[ res->header.nTotalVertices ];
	res->allIndex  = new U16[ res->header.nTotalIndices ];

	item->GetData( "vertex", res->allVertex, res->header.nTotalVertices*sizeof(Vertex) );
	item->GetData( "index", res->allIndex, res->header.nTotalIndices*sizeof(U16) );

	// compute the hit testing AABB
	float ave = grinliz::Max( res->header.bounds.SizeX(), res->header.bounds.SizeZ() )*0.5f;
	//float ave = Max( res->header.bounds.SizeX(), res->header.bounds.SizeZ() );
	res->hitBounds.min.Set( -ave, res->header.bounds.min.y, -ave );
	res->hitBounds.max.Set( ave, res->header.bounds.max.y, ave );

	//GLOUTPUT(( "Load Model: %s\n", res->header.name.c_str() ));

	GLASSERT( res->header.nGroups < 10 );
	for( U32 i=0; i<res->header.nGroups; ++i )
	{
		const gamedb::Item* groupItem = item->Child( i );

		ModelGroup group;
		group.Load( groupItem );

		const char* textureName = group.textureName.c_str();
		if ( !textureName[0] ) {
			textureName = "white";
		}

		GLString base, texname, extension;
		StrSplitFilename( GLString( textureName ), &base, &texname, &extension );
		Texture* t = TextureManager::Instance()->GetTexture( texname.c_str() );

		GLASSERT( t );                       
		res->atom[i].texture = t;

		res->atom[i].nVertex = group.nVertex;
		res->atom[i].nIndex = group.nIndex;

		//GLOUTPUT(( "  '%s' vertices=%d tris=%d\n", textureName, (int)res->atom[i].nVertex, (int)(res->atom[i].nIndex/3) ));
	}

	int vOffset = 0;
	int iOffset = 0;
	for( U32 i=0; i<res->header.nGroups; ++i )
	{
		res->atom[i].index  = res->allIndex+iOffset;
		res->atom[i].vertex = res->allVertex+vOffset;

		iOffset += res->atom[i].nIndex;
		vOffset += res->atom[i].nVertex;
	}
}


Model::Model()		
{	
	// WARNING: in the normal case, the constructor isn't called. Models usually come from a pool!
	// new/delete and Init/Free are both valid uses.
	Init( 0, 0 ); 
}

Model::~Model()	
{	
	// WARNING: in the normal case, the destructor isn't called. Models usually come from a pool!
	// new/delete and Init/Free are both valid uses.
	Free();
}


void Model::Init( const ModelResource* resource, SpaceTree* tree )
{
	this->resource = resource; 
	this->tree = tree;
	this->textureAtoms = 0;

	pos.Set( 0, 0, 0 );
	rot[0] = rot[1] = rot[2] = 0.0f;
	texMatSet = false;
	setTexture = 0;
	Modify();

	if ( tree ) {
		tree->Update( this );
	}

	flags = 0;
	if ( resource && (resource->header.flags & ModelHeader::RESOURCE_NO_SHADOW ) ) {
		flags |= MODEL_NO_SHADOW;
	}
}


void Model::Free()
{
	if ( textureAtoms )	{	// superfluous 'if', used for debugging
		delete [] textureAtoms; 
		textureAtoms = 0;
	}
}


void Model::SetPos( const grinliz::Vector3F& pos )
{ 
	if ( pos != this->pos ) {
		Modify();
		this->pos = pos;	
		tree->Update( this ); 
	}
}


void Model::SetRotation( float r, int axis )
{
	GLASSERT( axis >= 0 && axis < 3 );
	while( r < 0 )			{ r += 360.0f; }
	while( r >= 360 )		{ r -= 360.0f; }

	if ( r != this->rot[axis] ) {
		Modify();
		this->rot[axis] = r;		
		tree->Update( this );	// call because bound computation changes with rotation
	}
}



void Model::SetSkin( int armor, int skin, int hair )
{
	texMatSet = false;
	if ( armor > 0 || skin > 0 || hair > 0 ) {
	
		float textureOffsetX = 0.0f;

		armor = Clamp( armor, 0, 3 );
		skin = Clamp( skin, 0, 3 );
		hair = Clamp( hair, 0, 3 );

		textureOffsetX += float( armor ) / float( 4 );
		textureOffsetX += float( skin ) / float( 16 );
		textureOffsetX += float( hair ) / float( 64 );

		texMat.SetIdentity();
		texMat.m14 = textureOffsetX;
		texMatSet = (textureOffsetX != 0) ? true : false;
	}
}


void Model::SetTexXForm( float a, float d, float x, float y )
{
	texMat.SetIdentity();
	texMatSet = false;
	if ( a!=1.0f || d!=1.0f || x!=0.0f || y!=0.0f ){
		texMat.m11 = a;
		texMat.m22 = d;
		texMat.m14 = x;
		texMat.m24 = y;
		texMatSet = true;
	}
}


void Model::CalcHitAABB( Rectangle3F* aabb ) const
{
	// This is already an approximation - ignore rotation.
	aabb->min = pos + resource->hitBounds.min;
	aabb->max = pos + resource->hitBounds.max;
}


void Model::CalcTrigger( grinliz::Vector3F* trigger ) const
{
	const Matrix4& xform = XForm();
	*trigger = xform * resource->header.trigger;
}


void Model::CalcTarget( grinliz::Vector3F* target ) const
{
	const Matrix4& xform = XForm();
	Vector3F t = { 0, resource->header.target, 0 };
	*target = xform * t;
}


void Model::SetTexture( Texture* t )
{
	delete [] textureAtoms;
	textureAtoms = 0;

	if ( t ) {
		textureAtoms = new ModelAtom[resource->header.nGroups];
		for( int i=0; i<resource->header.nGroups; ++i ) {
			textureAtoms[i] = resource->atom[i];
			textureAtoms[i].texture = t;		// the whole point: assign to a different texture.
		}
	}
}


void Model::Queue( RenderQueue* queue, GPUShader* opaque, GPUShader* transparent )
{
	if ( flags & MODEL_INVISIBLE )
		return;

	for( U32 i=0; i<resource->header.nGroups; ++i ) 
	{
		if ( textureAtoms )
			queue->Add( this, &textureAtoms[i], textureAtoms[i].texture->Alpha() ? transparent : opaque );
		else
			queue->Add( this, &resource->atom[i], resource->atom[i].texture->Alpha() ? transparent : opaque );
	}
}


void ModelAtom::LowerBind( GPUShader* shader, const GPUShader::Stream& stream ) const
{
#ifdef EL_USE_VBO
	if ( GPUShader::SupportsVBOs() && !vertexBuffer.IsValid() ) {
		GLRELASSERT( !indexBuffer.IsValid() );

		vertexBuffer = GPUVertexBuffer::Create( vertex, nVertex );
		indexBuffer  = GPUIndexBuffer::Create(  index,  nIndex );
	}
#endif

	if ( vertexBuffer.IsValid() && indexBuffer.IsValid() ) 
		shader->SetStream( stream, vertexBuffer, nIndex, indexBuffer );
	else
		shader->SetStream( stream, vertex, nIndex, index );
}


void ModelAtom::Bind( GPUShader* shader ) const
{
	GPUShader::Stream stream( vertex );
	LowerBind( shader, stream );
}


void ModelAtom::BindPlanarShadow( GPUShader* shader ) const
{
	GPUShader::Stream stream;
	stream.stride = sizeof( Vertex );
	stream.nPos = 3;
	stream.posOffset = Vertex::POS_OFFSET;
	stream.nTexture0 = 3;
	stream.texture0Offset = Vertex::POS_OFFSET;

	LowerBind( shader, stream );
}


/*
void ModelAtom::Draw() const
{
	GRINLIZ_PERFTRACK
	// mode, count, type, indices
#ifdef EL_USE_VBO
	glDrawElements( GL_TRIANGLES, 
					nIndex,
					GL_UNSIGNED_SHORT, 
					0 );
#else
	glDrawElements( GL_TRIANGLES, nIndex, GL_UNSIGNED_SHORT, index );
#endif
	CHECK_GL_ERROR;
	trianglesRendered += nIndex / 3;
	drawCalls++;
}
*/

/*
void Model::PushMatrix( bool textureToo ) const
{
	glPushMatrix();
	const Matrix4& xform = XForm();
	glMultMatrixf( xform.x );

	if ( textureToo && texMatSet ) {
		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		
		float m[16] = {
			texMat.a, 0,	0,	0,		// column 1
			0,	texMat.d, 0, 0,
			0, 0, 0, 0,
			texMat.x, texMat.y, 0, 1
		};
		glMultMatrixf( m );
	}
	CHECK_GL_ERROR;
}


void Model::PopMatrix( bool textureToo ) const
{
	if ( textureToo && texMatSet ) {
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
	}
	glPopMatrix();
	CHECK_GL_ERROR;
}
*/


const grinliz::Rectangle3F& Model::AABB() const
{
	XForm();	// just makes sure the cache is good.
	return aabb;
}


const grinliz::Matrix4& Model::XForm() const
{
	if ( !xformValid ) {
		Matrix4 t;
		t.SetTranslation( pos );

		Matrix4 r;
		if ( rot[1] != 0.0f ) 
			r.ConcatRotation( rot[1], 1 );
		if ( rot[2] != 0.0f )
			r.ConcatRotation( rot[2], 2 );
		if ( rot[0] != 0.0f )
			r.ConcatRotation( rot[0], 0 );

		_xform = t*r;

		// compute the AABB.
		MultMatrix4( _xform, resource->header.bounds, &aabb );
		xformValid = true;
	}
	return _xform;
}


const grinliz::Matrix4& Model::InvXForm() const
{
	if ( !invValid ) {
		const Matrix4& xform = XForm();

		const Vector3F& u = *((const Vector3F*)&xform.x[0]);
		const Vector3F& v = *((const Vector3F*)&xform.x[4]);
		const Vector3F& w = *((const Vector3F*)&xform.x[8]);

		_invXForm.m11 = u.x;	_invXForm.m12 = u.y;	_invXForm.m13 = u.z;	_invXForm.m14 = -DotProduct( u, pos );
		_invXForm.m21 = v.x;	_invXForm.m22 = v.y;	_invXForm.m23 = v.z;	_invXForm.m24 = -DotProduct( v, pos );
		_invXForm.m31 = w.x;	_invXForm.m32 = w.y;	_invXForm.m33 = w.z;	_invXForm.m34 = -DotProduct( w, pos );
		_invXForm.m41 = 0;		_invXForm.m42 = 0;		_invXForm.m43 = 0;		_invXForm.m44 = 1;

		invValid = true;
	}
	return _invXForm;
}


// TestHitTest(PC)
// 1.4 fps (eek!)
// sphere test: 4.4
// tweaking: 4.5
// AABB testing in model: 8.0
// cache the xform: 8.4
// goal: 30. Better but ouchie.

int Model::IntersectRay(	const Vector3F& _origin, 
							const Vector3F& _dir,
							Vector3F* intersect ) const
{
	Vector4F origin = { _origin.x, _origin.y, _origin.z, 1.0f };
	Vector4F dir    = { _dir.x, _dir.y, _dir.z, 0.0f };
	int result = grinliz::REJECT;

	//Sphere sphere;
	//CalcBoundSphere( &sphere );
	//int initTest = IntersectRaySphere( sphere, _origin, _dir );
	Vector3F dv;
	float dt;
	int initTest = IntersectRayAABB( _origin, _dir, AABB(), &dv, &dt );

	if ( initTest == grinliz::INTERSECT || initTest == grinliz::INSIDE )
	{
		const Matrix4& inv = InvXForm();

		Vector4F objOrigin4 = inv * origin;
		Vector4F objDir4    = inv * dir;

		Vector3F objOrigin = { objOrigin4.x, objOrigin4.y, objOrigin4.z };
		Vector3F objDir    = { objDir4.x, objDir4.y, objDir4.z };
		Vector3F objIntersect;

		result = resource->Intersect( objOrigin, objDir, &objIntersect );
		if ( result == grinliz::INTERSECT ) {
			// Back to this coordinate system. What a pain.
			const Matrix4& xform = XForm();

			Vector4F objIntersect4 = { objIntersect.x, objIntersect.y, objIntersect.z, 1.0f };
			Vector4F intersect4 = xform*objIntersect4;
			intersect->Set( intersect4.x, intersect4.y, intersect4.z );
		}
	}
	return result;
}


void ModelResourceManager::AddModelResource( ModelResource* res )
{
	modelResArr.Push( res );
	map.Add( res->header.name.c_str(), res );
}


const ModelResource* ModelResourceManager::GetModelResource( const char* name, bool errorIfNotFound )
{
	ModelResource* res = 0;
	bool found = map.Query( name, &res );

	if ( found ) 
		return res;
	if ( errorIfNotFound ) {
		GLASSERT( 0 );
	}
	return 0;
}


///*static*/ int ModelResourceManager::Compare( const void * elem1, const void * elem2 )
//{
//	const ModelResource* t1 = *((const ModelResource**)elem1);
//	const ModelResource* t2 = *((const ModelResource**)elem2);
//	return strcmp( t1->header.name, t2->header.name );
//}


/*static*/ void ModelResourceManager::Create()
{
	GLASSERT( instance == 0 );
	instance = new ModelResourceManager();
}

/*static*/ void ModelResourceManager::Destroy()
{
	GLASSERT( instance );
	delete instance;
	instance = 0;
}

ModelResourceManager* ModelResourceManager::instance = 0;

ModelResourceManager::ModelResourceManager()
{
}
	

ModelResourceManager::~ModelResourceManager()
{
	for( unsigned i=0; i<modelResArr.Size(); ++i )
		delete modelResArr[i];
}


void ModelResourceManager::DeviceLoss()
{
	for( unsigned i=0; i<modelResArr.Size(); ++i )
		modelResArr[i]->DeviceLoss();

}