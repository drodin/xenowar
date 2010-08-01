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

#ifndef RENDERQUEUE_INCLUDED
#define RENDERQUEUE_INCLUDED

#include "../grinliz/gldebug.h"
#include "../grinliz/gltypes.h"
#include "enginelimits.h"
#include "vertex.h"

class Model;
struct ModelAtom;
class Texture;

/* 
	The prevailing wisdom for GPU performance is to group the submission by 1)render state
	and 2) texture. In general I question this for tile based GPUs, but it's better to start
	with a queue architecture rather than have to retrofit later.

	RenderQueue queues up everything to be rendered. Flush() commits, and should be called 
	at the end of the render pass. Flush() may also be called automatically if the queues 
	are full.
*/
class RenderQueue
{
public:
	enum {
		MAX_STATE  = 128,
		MAX_MODELS = 1024,
	};

	RenderQueue();
	~RenderQueue();

	void Add( Model* model, const ModelAtom* atom );		// not const - can change billboard rotation

	enum {
		MODE_IGNORE_TEXTURE				= 0x01,		// Ignore textures on all models. Don't set texture state, sort everything to same bucket.
		MODE_IGNORE_ALPHA				= 0x02,		// Ignore alpha settings on texture.
		MODE_PLANAR_SHADOW				= 0x04,		// Do all the fancy tricks to create planar shadows.
	};
	void Flush( int mode, int required, int excluded, float billboardRotation );
	bool Empty() { return nState == 0 && nModel == 0; }
	void Clear() { nState = 0; nModel = 0; }

private:
	enum { 
		FLAG_ALPHA = 0x01,		// render in blend phase
	};

	struct Item {
		Model*				model;
		const ModelAtom*	atom;
		Item*				next;
	};

	struct State {
		int				flags;
		Texture*		texture;
		Item*			root;
	};

	int Compare( const State& s0, const State& s1 ) 
	{
		if ( s0.flags == s1.flags ) {
			if ( s0.texture == s1.texture )
				return 0;
			return ( s0.texture < s1.texture ) ? -1 : 1;
		}
		else if ( s0.flags < s1.flags ) {
			return -1;
		}
		return 1;
	}

	State* FindState( const State& state );

	int nState;
	int nModel;

	void FlushBuffers();
	int nVertex;
	int nIndex;

	State statePool[MAX_STATE];
	Item modelPool[MAX_MODELS];
};


#endif //  RENDERQUEUE_INCLUDED