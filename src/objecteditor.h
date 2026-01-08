#pragma once


#include "editorplugin.h"
#include <base/gui/gui.h>
#include "object.h"
#include <vector>

namespace base { class Material; class Texture;  class Model; class Mesh; class DebugGeometry; }
namespace gui { class Button; class Listbox; class Combobox; class Textbox; class Spinbox; class SpinboxFloat; class TreeView; class TreeNode; class Listbox; class ListItem; }

namespace editor { class Gizmo; }
class BoxSelect;
class ObjectEditor;

struct MeshReference {
	base::String model;
	base::String mesh;
	const char* getName() const;
	bool operator==(const MeshReference& o) const { return model==o.model && mesh==o.mesh; }
};

class ObjectGroupEditor {
	public:
	virtual void createObjects(ObjectGroup*) const = 0;
	virtual void moveGroup(ObjectGroup* group, const vec3& position) const = 0;
	virtual void placeGroup(ObjectGroup*, const vec3& position) {}
	virtual bool updateEditor(ObjectGroup*, const Mouse& mouse, const Ray& ray, base::Camera*, InputState& state) = 0;
	virtual void updateLines(ObjectGroup*, base::DebugGeometry&) {}
	virtual void loadGroup(const base::XMLElement&, ObjectGroup*) const = 0;
	virtual ObjectGroupData* loadTemplate(const base::XMLElement&) const = 0;
	virtual ObjectGroupData* createTemplate(std::vector<Object*>& m_selection) = 0;
	virtual ObjectGroup* createGroup(const ObjectGroupData* data, const vec3& position) const = 0;
	virtual base::XMLElement saveGroup(ObjectGroup*) const = 0;
	virtual base::XMLElement saveTemplate(const ObjectGroupData*) const = 0;
	virtual void showPanel(ObjectGroupData*) {}
	virtual void setup(ObjectEditor* parent, gui::Widget* panel) { m_parent = parent; m_panel = panel; }
	virtual bool dropMesh(gui::Widget* widget, const MeshReference& mesh) { return false; }
	gui::Widget* getPanel() const { return m_panel; }
	protected:
	ObjectEditor* m_parent = nullptr;
	gui::Widget* m_panel = nullptr;
};


class ObjectEditor : public EditorPlugin {
	public:
	ObjectEditor(gui::Root*, FileSystem*, MapGrid*, base::SceneNode*);
	~ObjectEditor();
	void load(const base::XMLElement&, const TerrainMap* context) override;
	base::XMLElement save(const TerrainMap* context) const override;
	void update(const Mouse&, const Ray&, base::Camera*, InputState&) override;
	void clear() override;
	void close() override;

	void setResourcePath(const char* path);
	void selectObject(Object*, bool append=false);
	Object* pick(base::SceneNode* node, const Ray& ray, bool ignoreSelection, float& t) const;
	static bool pickMesh(const Ray& ray, const base::Mesh* mesh, const Matrix& transform, float& t);
	bool trace(const Ray& ray, float& t) const;

	Object* createObject(const MeshReference&, ObjectGroup* group=nullptr);
	float getTerrainHeight(const vec3& p) const;
	void notifyTemplateChanged(ObjectGroupData*);
	void notifyTemplateRenamed(ObjectGroupData*);

	protected:
	void changePath(gui::Textbox*);
	void selectResource(gui::TreeView*, gui::TreeNode*);
	void selectObject(gui::Listbox*, gui::ListItem&);
	void changeVisible(gui::Listbox*, gui::ListItem&, gui::Widget*);

	protected:
	void setupMaterials();
	base::Texture* findTexture(const char* name, const char* suffix, int limit=3);
	base::Material* getMaterial(const char* name, bool normalMap, bool coloured);
	gui::TreeNode* addModel(const char* path, const char* name);
	gui::TreeNode* addFolder(const char* path, const char* name);
	void cancelPlacement();
	bool isSelected(base::SceneNode* obj) const;
	void clearSelection();
	Object* createObject(const char* name, base::Model* model, const char* mesh=0, ObjectGroup* group = nullptr);
	void placeObject(Object* object, gui::TreeNode* data);
	void applySelectTransform();
	void selectionChanged();
	void setProperty(gui::Widget*);
	void setName(gui::Widget*);
	void setTransform(gui::SpinboxFloat*, float);
	void refreshTransform(Object*);
	void setGizmoMode(int mode);
	void setGizmoSpaceLocal(bool local);
	void createTemplate(gui::Combobox*, gui::ListItem&);
	void templateSelected(gui::Listbox*, gui::ListItem&);
	void editTemplate(gui::Button*);
	void addGroupEditor(const char* name, ObjectGroupEditor*, const char* panel=nullptr);

	protected:
	enum PlaceMode { SINGLE, ROTATE, CHAIN };
	base::SceneNode* m_node;
	FileSystem*    m_fileSystem;
	MapGrid*       m_terrain;
	Object*        m_placement;
	ObjectGroup*   m_placementGroup = nullptr;
	Point          m_pressed;
	float          m_yawOffset;
	vec3           m_altitude;
	vec3           m_chainStep;
	vec3           m_chainStart;
	PlaceMode      m_mode;
	bool           m_started;
	bool           m_collideObjects;
	Object*        m_activeObject = nullptr;
	ObjectGroup*   m_activeGroup = nullptr;
	editor::Gizmo* m_gizmo;
	BoxSelect*     m_box;

	std::vector<Object*> m_chain;
	base::SceneNode* m_selectGroup;

	base::HashMap<base::Material*> m_materials;

	gui::TreeView* m_resourceList;
	gui::TreeNode* m_resource;
	gui::Listbox* m_objectList;
	gui::Combobox* m_templateEditors;
	base::HashMap<base::Model*> m_models;
};


