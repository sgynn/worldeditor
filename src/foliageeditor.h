#ifndef _FOLIAGE_EDITOR_
#define _FOLIAGE_EDITOR_

#include "gui/widgets.h"
#include "gui/lists.h"
#include "foliage/foliage.h"

namespace base { class XMLElement; class Texture; }
class FileSystem;
class MapGrid;

enum class FoliageType { Instanced, Grass };

class Foliage : public FoliageSystem {
	public:
	Foliage(MapGrid* grid, int threads) : FoliageSystem(threads), m_terrain(grid) {}
	void resolvePosition(const vec3& point, vec3& position, float& height) const override;
	void resolveNormal(const vec3& point, vec3& normal) const override;

	int getActive(const vec3& context, float chunkSize, float range, IndexList& out) const override;
	float getMapValue(const FoliageMap* map, const BoundingBox2D& bounds, const vec3& position) const override;
	protected:
	MapGrid* m_terrain;
};



class FoliageEditor {
	public:
	FoliageEditor(gui::Root* gui, FileSystem* fs, MapGrid* terrain, scene::Scene*);
	~FoliageEditor();
	void setupGui(gui::Root*);

	void load(const base::XMLElement&);
	base::XMLElement serialise() const;

	void showFoliage(bool);
	void updateFoliage(const vec3& cam);
	scene::Material* getMaterial();
	
	protected:
	void layerSelected(gui::Listbox*, int);
	void layerRenamed(class FoliageLayerEditor*);
	void addLayer(gui::Combobox*, int);
	void removeLayer(gui::Button*);


	protected:
	Foliage*      m_foliage;
	FileSystem*   m_fileSystem;
	MapGrid*      m_terrain;
	scene::Scene* m_scene;

	gui::Window*  m_window;
	gui::Listbox* m_layerList;
	gui::Button*  m_removeButton;
};

class FoliageLayerEditor {
	public:
	FoliageLayerEditor(gui::Widget*, FoliageLayer*, FoliageType type, FileSystem*);
	const char* getName() const { return m_name; }
	FoliageLayer* getData() { return m_layer; }
	gui::Widget* getPanel() { return m_panel; }

	void load(const base::XMLElement&);
	base::XMLElement save() const;
	void updateSliders();
	void refresh();

	private:
	void renameLayer(gui::Textbox*);

	void setDensityMap(gui::Combobox*, int);
	void setDensity(gui::Scrollbar*, int);
	void setRange(gui::Scrollbar*, int);

	void setMinHeight(gui::Scrollbar*, int);
	void setMaxHeight(gui::Scrollbar*, int);
	void setMinSlope(gui::Scrollbar*, int);
	void setMaxSlope(gui::Scrollbar*, int);

	void setMinScale(gui::Scrollbar*, int);
	void setMaxScale(gui::Scrollbar*, int);

	void loadMesh(gui::Button*);
	void setMesh(gui::Combobox*, int);
	void setAlignment(gui::Combobox*, int);

	void loadSprite(gui::Button*);
	void setSpriteSize(gui::Scrollbar*, int);
	void setSprite(gui::Combobox*, int);
	void setScaleMap(gui::Combobox*, int);

	public:
	Delegate<void(FoliageLayerEditor*)> eventRenamed;

	protected:
	float m_range;
	float m_density;
	Range m_height;
	Range m_slope;
	Range m_scale;
	gui::String m_file;

	protected:
	FoliageType m_type;
	FoliageLayer* m_layer;
	FileSystem* m_fileSystem;
	gui::String m_name;
	float m_spriteSize;
	gui::Widget* m_panel;
};


#endif

