#pragma once

#include "editorplugin.h"
#include <base/gui/widgets.h>
#include <base/gui/lists.h>
#include "foliage/foliage.h"

namespace base { class XMLElement; class Texture;  class Mesh;  }
class FileSystem;
class FileDialog;
class MapGrid;
class FoliageLayerEditor;

enum class FoliageType { Instanced, Grass };
using base::FoliageLayer;
using base::FoliageMap;

class Foliage : public base::FoliageSystem {
	public:
	Foliage(MapGrid* grid, int threads) : FoliageSystem(threads), m_terrain(grid) {}
	void resolvePosition(const vec3& point, vec3& position, float& height) const override;
	void resolveNormal(const vec3& point, vec3& normal) const override;

	int getActive(const vec3& context, float chunkSize, float range, IndexList& out) const override;
	float getMapValue(const FoliageMap* map, const BoundingBox2D& bounds, const vec3& position) const override;
	protected:
	MapGrid* m_terrain;
};

struct FoliageSprite {
	gui::String file;
	base::Material* material;
	bool operator==(const FoliageSprite& other) const { return file==other.file; } // For gui::Any
};
struct FoliageMesh {
	gui::String file;
	gui::String diffuseMap;
	gui::String normalMap;
	base::Mesh* mesh;
	base::Material* material;
	bool operator==(const FoliageMesh& other) const { return file==other.file; } // For gui::Any
};


class FoliageEditor : public EditorPlugin {
	public:
	FoliageEditor(gui::Root* gui, FileSystem* fs, MapGrid* terrain, base::SceneNode*);
	~FoliageEditor();
	void setupGui(gui::Root*);

	void load(const base::XMLElement&, const TerrainMap* context) override;
	base::XMLElement save(const TerrainMap* context) const override;
	void update(const Mouse&, const Ray&, base::Camera*, InputState&) override;
	void clear() override;
	void close() override;

	void showFoliage(bool);
	base::Material* createMaterial(FoliageType type, const char* diffuse);
	FoliageLayerEditor* addLayer(FoliageType type);
	void showEditor(FoliageLayerEditor* layer);
	
	protected:
	void layerSelected(gui::Listbox*, gui::ListItem&);
	void layerRenamed(class FoliageLayerEditor*);
	void addLayer(gui::Combobox*, gui::ListItem&);
	void removeLayer(gui::Button*);
	void duplicateLayer(gui::Button*);
	void destroy(FoliageLayerEditor*);
	void changeVisible(gui::Listbox*, gui::ListItem&, gui::Widget*);

	protected:
	friend class FoliageLayerEditor;
	Foliage*      m_foliage;
	FileSystem*   m_fileSystem;
	MapGrid*      m_terrain;
	base::SceneNode* m_node;
	FileDialog*   m_fileDialog;

	gui::Listbox* m_layerList;
	gui::Button*  m_removeButton;
	gui::Button*  m_cloneButton;

	gui::ItemList* m_spriteList;
	gui::ItemList* m_meshList;
};

class FoliageLayerEditor {
	public:
	FoliageLayerEditor(FoliageEditor*, gui::Widget*, FoliageLayer*, FoliageType type);
	const char* getName() const { return m_name; }
	FoliageType getType() const { return m_type; }
	FoliageLayer* getData() { return m_layer; }
	gui::Widget* getPanel() { return m_panel; }

	void load(const base::XMLElement&);
	base::XMLElement save() const;
	void setVisible(bool);
	void updateSliders();
	void refresh(bool regen=true);

	private:
	void renameLayer(gui::Textbox*);

	void setDensityMap(gui::Combobox*, gui::ListItem&);
	void loadMesh(gui::Button*);
	void loadMeshFile(const char*);
	void setMesh(gui::Combobox*, gui::ListItem&);
	void setAlignment(gui::Combobox*, gui::ListItem&);

	void loadSprite(gui::Button*);
	void loadSpriteFile(const char*);
	void setSprite(gui::Combobox*, gui::ListItem&);
	void setScaleMap(gui::Combobox*, gui::ListItem&);

	public:
	Delegate<void(FoliageLayerEditor*)> eventRenamed;

	protected:
	int   m_distribution = 0;
	float m_range;
	float m_density;
	Range m_height;
	Range m_slope;
	Range m_scale;
	Range m_angle;
	float m_spriteSize;
	int   m_align;

	float m_clusterDensity = 0.01;
	Range m_clusterRadius = 10;
	Range m_clusterGap = 0;
	float m_clusterFalloff = 1;
	float m_clusterShapeScale = 0;
	int   m_clusterShapePoints = 0;
	

	gui::String m_file;

	struct Slider { gui::Scrollbar* slider; float& value; float max; float power=1; };
	std::vector<Slider> m_sliders;

	protected:
	FoliageEditor* m_editor;
	FoliageLayer* m_layer;
	FoliageType m_type;
	gui::String m_name;
	gui::Widget* m_panel;
};

