#ifndef _OBJECT_
#define _OBJECT_

#include "scene/scene.h"

/** Object base class
 * This is for all game objects
 * An obect may have one or more drawables
 * */
class Object : public scene::SceneNode {
	public:
	Object() {}
	virtual ~Object() {}

	/** Update object (needs to be in update list) */
	virtual void update() {};
	
};

#endif

