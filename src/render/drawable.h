#ifndef _DRAWABLE_
#define _DRAWABLE_

#include <base/math.h>
#include <base/model.h>
#include <base/material.h>

class RenderInfo;
using base::Material;
using base::Texture;

/** Drawable base class */
class Drawable {
	public:
	Drawable(): m_visible(true), m_material(0) {}
	virtual ~Drawable() {}

	virtual void draw( RenderInfo& ) = 0;


	void setMaterial(Material* m) { m_material=m; }
	const BoundingBox& bounds() const { return m_bounds; }

	protected:
	bool m_visible;
	BoundingBox m_bounds;
	Matrix m_transform;
	vec3 m_position;

	// Drawables are sorted by materials for optimisation
	Material* m_material;

};

/** Mesh drawable */
class DMesh : public Drawable, public base::model::Mesh {
	public:
	DMesh() {}
	~DMesh() {}
	virtual void draw( RenderInfo& );
	void updateBounds();
	void setMaterial(Material* m) { Drawable::setMaterial(m); } // Resolve ambiguity
	protected:
};

#endif

