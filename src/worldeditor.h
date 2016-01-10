#ifndef _WORLD_
#define _WORLD_

#include <base/scene.h>
#include <base/hashmap.h>
#include "gui/gui.h"
#include "object.h"
#include "resource/library.h"
#include "render/render.h"
#include "terraineditor/editor.h"
#include "toolgroup.h"
#include <list>

class EditableTexture;
class DynamicMaterial;
class MaterialEditor;

namespace gui { class Button; class Combobox; class Scrollbar; class Window; }
namespace base { class INIFile; }

class WorldEditor : public base::SceneState {
	public:
	WorldEditor(const base::INIFile&);
	~WorldEditor();

	void update();
	void drawScene();
	void drawHUD();


	void clear();
	void create(int size, float res, float scale, bool streamed);
	void loadWorld(const char* file);
	void saveWorld(const char* file);

	void messageBox(const char* title, const char* message, ...);
	void setupHeightTools(float resolution, const vec2& offset);
	void addGroup(ToolGroup* group, const char* icon);

	private:
	gui::Root* m_gui;
	void showNewDialog(gui::Button*);
	void showOpenDialog(gui::Button*);
	void showSaveDialog(gui::Button*);
	void showOptionsDialog(gui::Button*);
	void showMaterialList(gui::Button*);
	void showTextureList(gui::Button*);

	void createNewTerrain(gui::Button*);
	void cancelNewTerrain(gui::Button*);
	void changeTerrainMode(gui::Combobox*, int);
	void browseTerrainSource(gui::Button*);
	void setTerrainSource(const char* file);

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

	void setTerrainMaterial(DynamicMaterial*);

	private:
	Render* m_renderer;
	Library* m_library;

	// Terrain properies
	bool m_streaming;
	vec2 m_terrainOffset;
	vec2 m_terrainSize;
	float m_terrainScale;
	gui::String m_terrainFile;
	int m_terrainType;


	TerrainEditor* m_editor;				// Terrain Editor
	HeightmapEditorInterface* m_heightMap;	// Terrain object
	std::vector<ToolGroup*> m_groups;		// Terrain tool groups
	MaterialEditor* m_materials;			// Terrain material editor

	// Object list
	base::HashMap<Object*> m_objects;

	struct {
		float distance;	 	// view distance
		float speed;		// Camera speed
		float detail;		// Terrain lod parameter
		bool  collide;		// Camera collides with terrain
		bool  tabletMode;	// Mouse is not grabbed
	} m_options;
	
};

#endif

