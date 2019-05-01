#ifndef _WORLD_
#define _WORLD_

#include <base/scene.h>
#include <base/hashmap.h>
#include "gui/gui.h"
#include "object.h"
#include "resource/library.h"
#include "scene/scene.h"
#include "terraineditor/editor.h"
#include "toolgroup.h"
#include "filesystem.h"
#include <list>

class EditableTexture;
class DynamicMaterial;
class MaterialEditor;
class MiniMap;

namespace gui { class Button; class Combobox; class Scrollbar; class Window; }
namespace base { class INIFile; }

enum HeightFormat { HEIGHT_UINT8, HEIGHT_UINT16, HEIGHT_FLOAT };

class WorldEditor : public base::SceneState {
	public:
	WorldEditor(const base::INIFile&);
	~WorldEditor();

	void update();
	void drawScene();
	void drawHUD();


	void resized();
	void clear();
	void create(int size, float res, float scale, HeightFormat format, bool streamed);
	void loadWorld(const char* file);
	void saveWorld(const char* file);

	void messageBox(const char* title, const char* message, ...);
	void setupHeightTools(float resolution, const vec2& offset);
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

	void createNewTerrain(gui::Button*);
	void cancelNewTerrain(gui::Button*);
	void changeTerrainMode(gui::Combobox*, int);
	void browseTerrainSource(gui::Button*);
	void setTerrainSource(const char* file);

	void validateNewEditor(gui::Combobox*, int);
	void createNewEditor(gui::Button*);
	void cancelNewEditor(gui::Button* = 0);
	void cancelNewEditor(gui::Window*);
	void browseNewEditor(gui::Button*);
	void setEditorSource(const char* file);

	void changeViewDistance(gui::Scrollbar*, int);
	void changeDetail(gui::Scrollbar*, int);
	void changeSpeed(gui::Scrollbar*, int);
	void changeTabletMode(gui::Button*);
	void changeCollision(gui::Button*);
	void saveSettings(gui::Window* =0);

	void selectToolGroup(gui::Combobox*, int);
	void selectTool(ToolInstance*);
	void changeBrushSlider(gui::Scrollbar*, int);
	void updateBrushSliders();

	void moveWorldMap(gui::Widget*, const Point&, int);
	void setTerrainMaterial(DynamicMaterial*);
	void textureListChanged();

	private:
	Scene*   m_scene;
	FileSystem* m_fileSystem;

	// Terrain properies
	bool m_streaming;
	vec2 m_terrainOffset;
	vec2 m_terrainSize;
	float m_terrainScale;
	gui::String m_terrainFile;
	HeightFormat m_heightFormat;


	gui::String    m_file;
	TerrainEditor* m_editor;				// Terrain Editor
	HeightmapEditorInterface* m_heightMap;	// Terrain object
	std::vector<ToolGroup*> m_groups;		// Terrain tool groups
	MaterialEditor* m_materials;			// Terrain material editor

	MiniMap* m_minimap;						// Minimap data

	// List of streams that need flushing on save ?
	std::vector<BufferedStream*> m_streams;

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
	ImageMapData* createMapData(EditableTexture* tex, const char* name, const char* file, MapUsage usage); 
	int createUniqueMapName(const char* pattern, char* out) const;

	// Object list
	base::HashMap<Object*> m_objects;

	struct {
		float distance;	 	// view distance
		float speed;		// Camera speed
		float detail;		// Terrain lod parameter
		bool  collide;		// Camera collides with terrain
		bool  tabletMode;	// Mouse is not grabbed
		float fov;			// Camera field of view
	} m_options;
	
};

#endif

