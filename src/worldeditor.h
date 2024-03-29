#ifndef _WORLD_
#define _WORLD_

#include <base/gamestate.h>
#include <base/hashmap.h>
#include <base/gui/gui.h>
#include "object.h"
#include "heightmap.h"
#include <base/scene.h>
#include "terraineditor/editor.h"
#include "editorplugin.h"
#include "toolgroup.h"
#include "filesystem.h"
#include <list>

class EditableTexture;
class DynamicMaterial;
class MaterialEditor;
class MiniMap;

namespace gui { class Button; class Combobox; class Scrollbar; class Window; class Listbox; class Popup; class Textbox; class ListItem; }
namespace base { class INIFile; class XMLElement; }

enum class SaveFormat { RAW, TIF16, PNG16 };

class WorldEditor : public base::GameState {
	public:
	WorldEditor(const base::INIFile&);
	~WorldEditor();

	void update() override;
	void draw() override;

	void resized();
	void clear();
	void createNewTerrain(int size);
	void loadWorld(const char* file);
	void saveWorld(const char* file);
	void updateTitle();

	void messageBox(const char* title, const char* message, ...);
	void setupHeightTools(float resolution);
	void addGroup(ToolGroup* group, const char* icon, bool select=false);

	private:
	gui::Root* m_gui;
	gui::Widget* m_mapMarker;

	void showNewDialog(gui::Button*);
	void showOpenDialog(gui::Button*);
	void showSaveDialog(gui::Button*);
	void showOptionsDialog(gui::Button*);
	void showMaterialList(gui::Button*);
	void showTextureList(gui::Button*);
	void showWorldMap(gui::Button*);
	void showFoliageList(gui::Button*);

	void createNewTerrain(gui::Button*);
	void cancelNewTerrain(gui::Button*);
	void changeTerrainMode(gui::Combobox*, gui::ListItem&);
	void browseTerrainSource(gui::Button*);
	void setTerrainSource(const char* file);

	void validateNewEditor(gui::Combobox*, gui::ListItem&);
	void createNewEditor(gui::Button*);
	void cancelNewEditor(gui::Button* = 0);
	void cancelNewEditor(gui::Window*);
	void textureMapCreated(TerrainMap*);

	void createNewTile(gui::Button*);
	void duplicateTile(gui::Button*);
	void lockTile(gui::Button*);
	void loadTile(gui::Button*);
	void unloadTile(gui::Button*);
	void showTileList(gui::Button*);
	void assignTile(gui::Listbox*, gui::ListItem&);
	void showRenameTile(gui::Button*);
	void renameTile(gui::Button*);
	void renameTile(gui::Textbox*);

	void changeViewDistance(gui::Scrollbar*, int);
	void changeDetail(gui::Scrollbar*, int);
	void changeSpeed(gui::Scrollbar*, int);
	void changeTabletMode(gui::Button*);
	void changeCollision(gui::Button*);
	void saveSettings(gui::Window* =0);

	void selectToolGroup(gui::Combobox*, gui::ListItem&);
	void selectTool(ToolInstance*);
	void changeBrushSlider(gui::Scrollbar*, int);
	void updateBrushSliders();

	void moveWorldMap(gui::Widget*, const Point&, int);
	void setTerrainMaterial(DynamicMaterial*);
	void textureListChanged();

	void editorActivated(EditorPlugin* editor);

	protected:
	void refreshMap();										// rebuild minimap
	TerrainMap* createTile(const char* name);				// Create a new tile using existing settings
	TerrainMap* loadTile(const base::XMLElement&);			// Load tile from xml
	void saveTile(TerrainMap*, base::XMLElement&) const;	// Save tile data to xml
	void assignTile(const Point&, TerrainMap* tile);		// Assign map to tile


	private:
	base::Scene*    m_scene;
	base::Renderer* m_renderer;
	base::Camera*    m_camera;
	FileSystem*      m_fileSystem;
	gui::String      m_root;

	// Terrain properies
	bool  m_streaming;					// Use streaming editor
	int   m_mapSize;					// Map size
	float m_resolution;					// Map horizontal resolution

	gui::String m_terrainFile;
	SaveFormat m_heightFormat;
	Rangef m_heightRange;


	gui::String    m_file;
	std::vector<ToolGroup*> m_groups;		// Terrain tool groups
	MaterialEditor* m_materials;			// Terrain material editor
	MiniMap*        m_minimap;				// Minimap data
	TerrainEditor*  m_editor;				// Terrain editor
	gui::Popup*     m_contextMenu;			// Terrain tile context menu
	ToolGroup*      m_activeGroup;			// Active tool
	Point m_currentTile;					// Tile for context menu

	// List of streams that need flushing on save ?
	std::vector<BufferedStream*> m_streams;


	// Terrain data
	std::vector<TerrainMap*> m_maps;
	MapGrid* m_terrain;


	// Image map data
	enum MapUsage { USAGE_COLOUR, USAGE_WEIGHT, USAGE_INDEX };
	struct ImageMapData {
		EditableTexture* map;
		EditableTexture* map2;
		gui::String file;	// Image file
		gui::String file2;	// Second file if using index modes that need more channels
		gui::String name;	// Image name
		int         layers;	// Index layer count
		MapUsage 	usage;	// usage hint
	};
	std::vector<ImageMapData*> m_imageMaps;

	struct ImageMapData* createMapData(EditableTexture* tex, const char* name, const char* file, MapUsage usage); 
	int createUniqueMapName(const char* pattern, char* out) const;

	// Additional editors
	template<class E> void createEditor();
	std::vector<EditorPlugin*> m_editors;
	int m_activeEditor;

	// Object list
	base::HashMap<Object*> m_objects;

	struct {
		float distance;	 	// view distance
		float speed;		// Camera speed
		float detail;		// Terrain lod parameter
		bool  collide;		// Camera collides with terrain
		bool  tabletMode;	// Mouse is not grabbed
		float fov;			// Camera field of view
		bool  escapeQuits;	// Escape quits program when no windows open
	} m_options;
	
};

#endif

