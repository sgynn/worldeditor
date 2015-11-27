#ifndef _RENDERER_
#define _RENDERER_

#include "drawable.h"
#include "base/camera.h"
#include <vector>

/** Renderer 
 *	Uses drawable objects for everything
 *	Objects kept in quadtree
 *	Has quadtree list of all drawables (some need to move)
 *
 *	Call collect() to create render list
 *	Call render() render active list
 *
 *	render() takes draw flags for various passes
 *	flags include: NO_NORMALMAP, NO_TEXTURE
 *	flags may change which materials are used
 *	Want to allow deferred shading and ssao to be added which both require 
 *	additional passes
 * */
class Render {
	public:
	Render();
	~Render();

	/** Add drawable to quadtree */
	void add(Drawable* d);
	/** Remove drawable from quadtree */
	void remove(Drawable* d);

	/** Generate the list of drawables to render */
	void collect(base::Camera* camera);

	/** Render collected drawables */
	void render(int flags=0);

	protected:

	// Flat list for now - will add implicit quadtree woth morton keys later */
	std::vector<Drawable*> m_list;
	std::vector<Drawable*> m_collect;
	base::Camera* m_camera;

};

enum RenderInfoState {
	VERTEX_ARRAY   = 1,
	NORMAL_ARRAY   = 2,
	TEXCOORD_ARRAY = 4,
	COLOUR_ARRAY   = 8,

	DOUBLE_SIDED   = 16,
	INVERT_FACES   = 32,
};

/** Render Info - Passed to drawables to describe OpenGL state and other flags */
class RenderInfo {
	public:
	/** Constructor - has render flags */
	RenderInfo(base::Camera* camera, uint flags=0);
	~RenderInfo();
	/** Get the render flags */
	uint flags() { return m_flags; }
	/** Set the OpenGL client state */
	void state(uint state);
	/** Set the current material */
	void material(Material*);
	/** Get current camera */
	base::Camera* getCamera() const { return m_camera; }

	private:
	uint m_flags;
	uint m_state;
	Material* m_material;
	base::Camera* m_camera;
};

#endif

