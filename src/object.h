#ifndef _OBJECT_
#define _OBJECT_

class Render;

/** Object base class
 * This is for all game objects
 * An obect may have one or more drawables
 * */
class Object {
	public:
	Object() {}
	virtual ~Object() {}

	/** Add contained drawables to the scene */
	virtual void addToScene(Render* r) {}
	/** Renove from scene */
	virtual void removeFromScene(Render* r) {}

	/** Update object (needs to be in update list) */
	virtual void update() {};
	
};

#endif

