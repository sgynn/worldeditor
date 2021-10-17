#pragma once

#include "editorplugin.h"
#include "gui/gui.h"
#include <base/vec.h>
#include <vector>

namespace gui { class Button; class Listbox; class Textbox; class Spinbox; }
namespace base { class XMLElement; }
namespace scene { class SceneNode; class DrawableMesh; }

class PolygonDrawable;

/// Custom properties
struct ItemProperty {
	gui::String key;
	gui::String value;
};

/// Polygon object
struct Polygon {
	std::vector<vec3> points;
	std::vector<int>  edges;
	std::vector<ItemProperty> properties;
	scene::DrawableMesh*  drawable;
};

/// Editor class
class PolygonEditor : public EditorPlugin {
	public:
	PolygonEditor(gui::Root* gui, FileSystem*, MapGrid* terrain, scene::SceneNode* scene);
	~PolygonEditor();
	void load(const base::XMLElement&, const TerrainMap* context) override;
	base::XMLElement save(const TerrainMap* context) const override;
	void update(const Mouse&, const Ray&, base::Camera*, InputState&) override;
	void clear() override;
	void close() override;

	protected:
	void addPolygon(gui::Button*);
	void removePolygon(gui::Button*);
	void polygonSelected(gui::Listbox*, int);
	void addProperty(gui::Button*);
	void removePropery(gui::Button*);
	void changeProperty(gui::Textbox*, const char*);
	void changeFlags(gui::Spinbox*, int);
	void addPropertyWidget();

	protected:
	void updateDrawable(Polygon*);
	void destroyDrawable(Polygon*);

	protected:
	enum DragMode { NONE, VERTEX, EDGE, ALL, INITIAL };
	MapGrid* m_terrain;
	scene::SceneNode* m_node;
	gui::Listbox* m_list;
	gui::Widget* m_properties;
	gui::Widget* m_propertyTemplate;
	vec3 m_cameraPosition;
	Polygon* m_selected;
	DragMode m_dragging;
	vec3 m_offset;
	int m_vertex;			// Vertex we are dragging

	// Need a system to set edge flags
	// - have a point mode where you select a flag then click on the edge
	// - have a spinbox that changes the last selected edge 				<- simpler
	
	// Custom properties
	// - Key value pairs of strings. Just create a dynamic list of text boxes.
	// xandu needs: target: x y; map: name; mode: mode;

	// Multiple tiles
	// - Have data stored in a map<Terrain*, vector<Polygon>>
	// save and load takes Terrain* parameter.
	// add void activate(Terrain*); to populate listbox
	// Perhaps have a droplist of all the terrains ?
	// also void tileLoaded(Terrain*, vec3 position); // to display in all loaded tiles ?
};
