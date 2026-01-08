#pragma once

#include <base/scene.h>
#include <base/string.h>

class ObjectGroup;

/** Object base class
 * This is for all game objects
 * An object may have one or more drawables
 * */
class Object : public base::SceneNode {
	public:
	Object(ObjectGroup* group = nullptr) : m_group(group) {}
	virtual ~Object() {}

	/** Update object (needs to be in update list) */
	virtual void update() {};

	void updateBounds();
	const BoundingBox& getBounds() const { return m_bounds; }

	ObjectGroup* getGroup() const { return m_group; }

	protected:
	BoundingBox m_bounds;
	ObjectGroup* m_group = nullptr;
	
};

// Template data for creating object groups
struct ObjectGroupData {
	class ObjectGroupEditor* const editor;
	base::String name;
	int instances = 0;
};

/// A group of objects controlled by an editor
class ObjectGroup {
	public:
	ObjectGroup(const ObjectGroupData* d) : m_groupData(d) {}
	~ObjectGroup();
	void setVisible(bool visible);
	const ObjectGroupData* getData() const { return m_groupData; }
	std::vector<Object*> objects;
	private:
	const ObjectGroupData* const m_groupData;
};

