#pragma once


#include "editorplugin.h"
#include "gui/gui.h"
#include "object.h"
#include <vector>

namespace scene { class Material; }
namespace base { class Texture; namespace bmodel { class Model; class Mesh; } }
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
	Object* pick(scene::SceneNode* node, const Ray& ray, bool ignoreSelection, float& t) const;
	static bool pickMesh(const Ray& ray, const base::bmodel::Mesh* mesh, const Matrix& transform, float& t);

	protected:
	void toggleEditor(gui::Button*);
	void changePath(gui::Textbox*);
	void selectResource(gui::TreeView*, gui::TreeNode*);
	void selectObject(gui::TreeView*, gui::TreeNode*);

	protected:
	void setupMaterials();
	base::Texture* findTexture(const char* name, const char* suffix, int limit=3);
	scene::Material* getMaterial(const char* name, bool normalMap, bool coloured);
	gui::TreeNode* addModel(const char* path, const char* name);
	gui::TreeNode* addFolder(const char* path, const char* name);
	void cancelPlacement();
	bool isSelected(scene::SceneNode* obj) const;
	void clearSelection();
	Object* createObject(const char* name, base::bmodel::Model*, base::bmodel::Mesh*, const char* material);
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
	bool           m_collideObjects;
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


