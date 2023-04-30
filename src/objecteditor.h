#pragma once


#include "editorplugin.h"
#include <base/gui/gui.h>
#include "object.h"
#include <vector>

namespace base { class Material; }
namespace base { class Texture;  class Model; class Mesh;  }
namespace gui { class Button; class Listbox; class Textbox; class Spinbox; class TreeView; class TreeNode; }

namespace editor { class Gizmo; }
class BoxSelect;

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

	protected:
	void changePath(gui::Textbox*);
	void selectResource(gui::TreeView*, gui::TreeNode*);
	void selectObject(gui::TreeView*, gui::TreeNode*);
	void changeVisible(gui::TreeView*, gui::TreeNode*, gui::Widget*);

	protected:
	void setupMaterials();
	base::Texture* findTexture(const char* name, const char* suffix, int limit=3);
	base::Material* getMaterial(const char* name, bool normalMap, bool coloured);
	gui::TreeNode* addModel(const char* path, const char* name);
	gui::TreeNode* addFolder(const char* path, const char* name);
	void cancelPlacement();
	bool isSelected(base::SceneNode* obj) const;
	void clearSelection();
	Object* createObject(const char* name, base::Model*, base::Mesh*, const char* material);
	void placeObject(Object* object, gui::TreeNode* data);
	void applySelectTransform();
	void selectionChanged();
	void setProperty(gui::Widget*);
	

	protected:
	enum PlaceMode { SINGLE, ROTATE, CHAIN };
	base::SceneNode* m_node;
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
	base::SceneNode* m_selectGroup;

	base::HashMap<base::Material*> m_materials;

	gui::TreeView* m_resourceList;
	gui::TreeView* m_sceneTree;
	gui::TreeNode* m_resource;
};


