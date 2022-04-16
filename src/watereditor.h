#pragma once
#include "editorplugin.h"
#include "water/water.h"
#include <base/gui/gui.h>
#include <base/vec.h>
#include <vector>

namespace gui { class Button; class Listbox; class Textbox; class Combobox; class Spinbox; }
namespace base { class XMLElement; }
namespace base { class SceneNode; class DrawableMesh; }

class WaterEditor : public EditorPlugin {
	public:
	WaterEditor(gui::Root*, FileSystem*, MapGrid*, base::SceneNode*);
	~WaterEditor();
	void load(const base::XMLElement&, const TerrainMap* context) override;
	base::XMLElement save(const TerrainMap* context) const override;
	void update(const Mouse&, const Ray&, base::Camera*, InputState&) override;
	void clear() override;
	void close() override;

	protected:
	void deselect();
	void addItem(gui::Combobox*, int);
	void deleteItem(gui::Button*);
	void duplicateItem(gui::Button*);
	void selectItem(gui::Listbox*, int);

	void updateLines();

	protected:
	MapGrid* m_terrain;
	WaterSystem* m_waterSystem;
	base::SceneNode* m_node;
	gui::Listbox* m_list;
	vec3 m_centre;

	WaterSystem::River* m_river = 0;
	WaterSystem::Lake* m_lake = 0;
	int m_activeNode = -1;
	int m_held = 0;
};


