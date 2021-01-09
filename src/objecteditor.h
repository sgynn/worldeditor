#pragma once


#include "editorplugin.h"
#include "gui/gui.h"
#include "object.h"
#include <vector>

namespace scene { class Material; }
namespace gui { class Button; class Listbox; class Textbox; class Spinbox; class TreeView; class TreeNode; }

namespace editor { class Gizmo; }
class BoxSelect;

class ObjectEditor : public EditorPlugin {
	public:
	ObjectEditor(gui::Root*, FileSystem*, MapGrid*, scene::SceneNode*);
	~ObjectEditor();
	void setup(gui::Widget* toolPanel) override;
	void load(const base::XMLElement&, const TerrainMap* context) override;
	base::XMLElement save(const TerrainMap* context) const override;
	void update(const Mouse&, const Ray&, int, base::Camera*) override;
	void clear() override;
	void close() override;

	void setResourcePath(const char* path);
	void selectObject(Object*, bool append=false);
	Object* pick(scene::SceneNode* node, const Ray& ray, float& t) const;

	protected:
	void toggleEditor(gui::Button*);
	void changePath(gui::Textbox*);
	void selectResource(gui::TreeView*, gui::TreeNode*);
	void selectObject(gui::TreeView*, gui::TreeNode*);

	protected:
	scene::Material* createMaterial(const char* name);
	gui::TreeNode* addModel(const char* path, const char* name);
	gui::TreeNode* addFolder(const char* path, const char* name);
	void updateObjectBounds(Object*);
	void cancelPlacement();
	bool isSelected(Object* obj) const;
	void clearSelection();
	void placeObject(Object* object, gui::TreeNode* data);
	void applySelectTransform();
	void selectionChanged();
	

	protected:
	enum PlaceMode { SINGLE, ROTATE, CHAIN };
	scene::SceneNode* m_node;
	FileSystem*    m_fileSystem;
	MapGrid*       m_terrain;
	Object*        m_placement;
	Point          m_pressed;
	float          m_yawOffset;
	vec3           m_altitude;
	vec3           m_chainStep;
	vec3           m_chainStart;
	PlaceMode      m_mode;
	bool           m_started;
	editor::Gizmo* m_gizmo;
	BoxSelect*     m_box;

	std::vector<Object*> m_selected;
	scene::SceneNode* m_selectGroup;

	base::HashMap<scene::Material*> m_materials;

	gui::Widget*   m_panel;
	gui::Button*   m_toolButton;
	gui::TreeView* m_resourceList;
	gui::TreeView* m_sceneTree;
	gui::TreeNode* m_resource;
};

