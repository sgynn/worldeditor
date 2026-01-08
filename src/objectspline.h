#pragma once

#include "objecteditor.h"

struct ObjectSplineData : public ObjectGroupData {
	enum Mode { PROJECTED, DIRECT };
	struct Item { MeshReference mesh; float chance; float length; vec3 offset; };
	std::vector<Item> segments;
	std::vector<Item> nodes;
	float maxSlope = 90;
	float separation = 0;
	Mode mode = PROJECTED;
};

class ObjectSpline : public ObjectGroup {
	public:
	ObjectSpline(const ObjectGroupData* d) : ObjectGroup(d) {}

	public:
	struct Node { vec3 point, direction; float a, b; };
	std::vector<Node> m_nodes;
};

class ObjectSplineEditor : public ObjectGroupEditor {
	public:
	void setup(ObjectEditor*, gui::Widget*) override;
	void createObjects(ObjectGroup*) const override;
	bool updateEditor(ObjectGroup*, const Mouse& mouse, const Ray& ray, base::Camera* cam, InputState& state) override;
	void updateLines(ObjectGroup*, base::DebugGeometry&) override;
	base::XMLElement saveGroup(ObjectGroup*) const override;
	void loadGroup(const base::XMLElement&, ObjectGroup*) const override;
	base::XMLElement saveTemplate(const ObjectGroupData*) const override;
	ObjectGroupData* loadTemplate(const base::XMLElement&) const override;
	ObjectGroupData* createTemplate(std::vector<Object*>& m_selection) override;
	ObjectGroup* createGroup(const ObjectGroupData* data, const vec3& position) const override;
	void moveGroup(ObjectGroup* group, const vec3& position) const override;
	void placeGroup(ObjectGroup* group, const vec3& position) override;
	void showPanel(ObjectGroupData*) override;
	bool dropMesh(gui::Widget* widget, const MeshReference& mesh) override;
	private:
	ObjectSplineData* m_data = nullptr;
	ObjectSpline* m_heldObject = nullptr;
	int m_heldIndex = 0; 
	int m_heldPart = 0; // Node, Left, Right
};



