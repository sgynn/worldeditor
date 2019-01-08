#ifndef _OBJECT_
#define _OBJECT_

class Scene;

/** Object base class
 * This is for all game objects
 * An obect may have one or more drawables
 * */
class Object {
	public:
	Object() {}
	virtual ~Object() {}

	/** Add contained drawables to the scene */
	virtual void addToScene(Scene* r) {}
	/** Renove from scene */
	virtual void removeFromScene(Scene* r) {}

	/** Update object (needs to be in update list) */
	virtual void update() {};
	
};

#endif

