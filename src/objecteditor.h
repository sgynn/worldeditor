#pragma once


#include "editorplugin.h"
#include "gui/gui.h"
#include "object.h"
#include <vector>

namespace gui { class Button; class Listbox; class Textbox; class Spinbox; class TreeView; class TreeNode; }

namespace editor { class Gizmo; }

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
	void selectObject(Object*);
	Object* pick(scene::SceneNode* node, const Ray& ray, float& t) const;

	protected:
	void toggleEditor(gui::Button*);
	void changePath(gui::Textbox*);
	void selectResource(gui::TreeView*, gui::TreeNode*);
	void selectObject(gui::TreeView*, gui::TreeNode*);

	protected:
	gui::TreeNode* addModel(const char* path, const char* name);
	gui::TreeNode* addFolder(const char* path, const char* name);
	void updateObjectBounds(Object*);
	void cancelPlacement();
	

	protected:
	scene::SceneNode* m_node;
	MapGrid*       m_terrain;
	Object*        m_selected;
	Object*        m_placement;
	Point          m_pressed;
	float          m_rotateMode;
	editor::Gizmo* m_gizmo;


	gui::Widget*   m_panel;
	gui::Button*   m_toolButton;
	gui::TreeView* m_resourceList;
	gui::TreeView* m_sceneTree;
	gui::TreeNode* m_resource;
};


