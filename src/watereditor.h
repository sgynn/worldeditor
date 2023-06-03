#pragma once
#include "editorplugin.h"
#include "water/water.h"
#include <base/gui/gui.h>
#include <base/vec.h>
#include <vector>
#include <map>

namespace gui { class Button; class Listbox; class ListItem; class Textbox; class Combobox; class Spinbox; }
namespace base { class XMLElement; class SceneNode; class DrawableMesh; class Material; }

class WaterEditor : public EditorPlugin {
	using River = WaterSystem::River;
	using Lake = WaterSystem::Lake;
	using Body = WaterSystem::Body;
	public:
	WaterEditor(gui::Root*, FileSystem*, MapGrid*, base::SceneNode*);
	~WaterEditor();
	void load(const base::XMLElement&, const TerrainMap* context) override;
	base::XMLElement save(const TerrainMap* context) const override;
	void update(const Mouse&, const Ray&, base::Camera*, InputState&) override;
	void notifyTileChanged(const Point&, const TerrainMap* tile, const TerrainMap* prev) override;
	void activate() override;
	void clear() override;
	void close() override;

	protected:
	void deselect();
	void addItem(gui::Combobox*, gui::ListItem&);
	void deleteItem(gui::Button*);
	void duplicateItem(gui::Button*);
	void selectItem(gui::Listbox*, gui::ListItem&);

	void updateLines();
	void updateGeometry(WaterSystem* system);
	static base::Material* getMaterial();

	protected:
	MapGrid* m_terrain;
	base::SceneNode* m_node;
	gui::Listbox* m_list;
	vec3 m_centre;

	struct WaterData {
		WaterSystem* system = nullptr;
		base::Mesh* mesh = nullptr;
	};
	std::map<const TerrainMap*, WaterData> m_water;
	std::map<Point, base::SceneNode*> m_nodes;

	enum class Held { None, Node, SplineA, SplineB, SideLeft, SideRight, Split };
	friend bool operator!(Held h) { return h == Held::None; }
	Body* m_active = nullptr;
	vec3 m_offset;
	int m_activeNode = -1;
	Held m_held = Held::None;
	WaterSystem* m_system = nullptr;
};


