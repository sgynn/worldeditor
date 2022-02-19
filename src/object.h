#ifndef _OBJECT_
#define _OBJECT_

#include <base/scene.h>

/** Object base class
 * This is for all game objects
 * An object may have one or more drawables
 * */
class Object : public base::SceneNode {
	public:
	Object() {}
	virtual ~Object() {}

	/** Update object (needs to be in update list) */
	virtual void update() {};

	void updateBounds();
	const BoundingBox& getBounds() const { return m_bounds; }

	protected:
	BoundingBox m_bounds;
	
};

#endif

