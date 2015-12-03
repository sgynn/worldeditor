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

namespace gui { class Button; class Combobox; }

class WorldEditor : public base::SceneState {
	public:
	WorldEditor();
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
	void selectToolGroup(gui::Combobox*, int);
	void selectTool(ToolInstance*);

	private:
	Render* m_renderer;
	Library* m_library;


	bool m_streaming;
	vec2 m_terrainOffset;
	TerrainEditor* m_editor;
	HeightmapEditorInterface* m_heightMap;	// Cached
	std::vector<ToolGroup*> m_groups;		// Terrain tool groups

	// Terrain textures
	base::HashMap<EditableTexture*> m_textures;
	

	// Object list
	base::HashMap<Object*> m_objects;

	struct {
		float speed;	// Camera speed
		float lod;		// Terrain lod parameter
		bool  collide;	// Camera collides with terrain
	} m_options;
	
};

#endif

