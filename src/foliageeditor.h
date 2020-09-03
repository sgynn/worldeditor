#ifndef _FOLIAGE_EDITOR_
#define _FOLIAGE_EDITOR_

#include "gui/widgets.h"
#include "gui/lists.h"
#include "foliage/foliage.h"

namespace base { class XMLElement; class Texture; namespace bmodel { class Mesh; } }
class FileSystem;
class FileDialog;
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

struct FoliageSprite {
	gui::String file;
	scene::Material* material;
	bool operator==(const FoliageSprite& other) const { return file==other.file; } // For gui::Any
};
struct FoliageMesh {
	gui::String file;
	gui::String diffuseMap;
	gui::String normalMap;
	base::bmodel::Mesh* mesh;
	scene::Material* material;
	bool operator==(const FoliageMesh& other) const { return file==other.file; } // For gui::Any
};


class FoliageEditor {
	public:
	FoliageEditor(gui::Root* gui, FileSystem* fs, MapGrid* terrain, scene::Scene*);
	~FoliageEditor();
	void setupGui(gui::Root*);

	void load(const base::XMLElement&);
	base::XMLElement save() const;
	void clear();

	void showFoliage(bool);
	void updateFoliage(const vec3& cam);
	scene::Material* createMaterial(FoliageType type, const char* diffuse);

	class FoliageLayerEditor* addLayer(FoliageType type);
	
	
	protected:
	void layerSelected(gui::Listbox*, int);
	void layerRenamed(class FoliageLayerEditor*);
	void addLayer(gui::Combobox*, int);
	void removeLayer(gui::Button*);
	void duplicateLayer(gui::Button*);
	void destroy(FoliageLayerEditor*);


	protected:
	friend class FoliageLayerEditor;
	Foliage*      m_foliage;
	FileSystem*   m_fileSystem;
	MapGrid*      m_terrain;
	scene::Scene* m_scene;
	FileDialog*   m_fileDialog;

	gui::Window*  m_window;
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

	void setMinAngle(gui::Scrollbar*, int);
	void setMaxAngle(gui::Scrollbar*, int);

	void loadMesh(gui::Button*);
	void loadMeshFile(const char*);
	void setMesh(gui::Combobox*, int);
	void setAlignment(gui::Combobox*, int);

	void loadSprite(gui::Button*);
	void loadSpriteFile(const char*);
	void setSprite(gui::Combobox*, int);
	void setScaleMap(gui::Combobox*, int);

	public:
	Delegate<void(FoliageLayerEditor*)> eventRenamed;

	protected:
	float m_range;
	float m_density;
	float m_densityMax;
	Range m_height;
	Range m_slope;
	Range m_scale;
	Range m_angle;
	float m_spriteSize;
	int   m_align;
	gui::String m_file;

	protected:
	FoliageEditor* m_editor;
	FoliageLayer* m_layer;
	FoliageType m_type;
	gui::String m_name;
	gui::Widget* m_panel;
};


#endif

