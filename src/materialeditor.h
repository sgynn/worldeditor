#pragma once

#include "arraytexture.h"
#include "dynamicmaterial.h"
#include <base/gui/widgets.h>
#include <base/gui/lists.h>

namespace base { class XMLElement; class Texture; }
class FileSystem;
class EditableTexture;

/** Texture data */
struct TerrainTexture {
	int         index;
	gui::String name;
	gui::String diffuse;
	gui::String normal;
	gui::String height;
	float       tiling;
};

class MaterialEditor {
	public:
	MaterialEditor(gui::Root* gui, FileSystem* fs, bool streaming);
	~MaterialEditor();
	void setupGui();
	void selectMaterial(DynamicMaterial*);

	// Material functions
	int              getMaterialCount() const;
	DynamicMaterial* createMaterial(const char* name);
	DynamicMaterial* getMaterial() const;
	DynamicMaterial* getMaterial(int index) const;
	DynamicMaterial* getMaterial(const char* name) const;
	void             destroyMaterial(int index);
	DynamicMaterial* loadMaterial(const base::XMLElement&);
	base::XMLElement serialiseMaterial(int index);

	// Texture functions
	int              getTextureCount() const;
	TerrainTexture*  getTexture(int index) const;
	TerrainTexture*  createTexture(const char* name);
	void             destroyTexture(int index);
	void             loadTexture(const base::XMLElement&, int layer);
	base::XMLElement serialiseTexture(int index);
	void             buildTextures();

	const base::Texture& getDiffuseArray() const;
	const base::Texture& getNormalArray() const;
	const base::Texture& getHeightArray() const;

	// Maps
	struct MapData { String name; int size; int channels; int flags; };
	const MapData& getMap(uint id) const;
	void addMap(uint id, const char* name, int size, int channels, int flags);
	void deleteMap(uint id);
	void setTerrainSize(const vec2& s) { m_terrainSize = s; }
	
	public:
	Delegate<void(DynamicMaterial*)> eventChangeMaterial;
	Delegate<void()>                 eventChangeTextureList;

	public:	// gui callbacks here
	void addTexture(gui::Button*);		// Add a texture
	void browseTexture(gui::Button*);	// Browse to texture
	void setTexture(const char*);		// Open dialog callback
	void removeTexture(gui::Button*);	// remove selected texture
	void reloadTexture(gui::Button*);	// reload selected texture
	void selectTexture(gui::Widget*);	// Click on texture
	void renameTexture(gui::Textbox*);	// Rename texture
	void renameTexture(gui::Widget*);	// Rename texture
	void addTextureGUI(TerrainTexture*);// Add texture gui item
	void changeTiling(gui::Scrollbar*, int);


	void addMaterial(gui::Button*);
	void selectMaterial(gui::Combobox*, gui::ListItem&);
	void renameMaterial(gui::Combobox*);
	void exportMaterial(gui::Button*);
	void changeMode(gui::Combobox*, gui::ListItem&);
	void addLayer(gui::Combobox*, gui::ListItem&);
	void removeLayer(gui::Button*);
	void toggleLayer(gui::Button*);
	void renameLayer(gui::Widget*);
	void moveLayer(int, int);
	void expandLayer(gui::Button*);
	void addLayerGUI(MaterialLayer*);
	void setupLayerWidgets(MaterialLayer*, gui::Widget*);
	void selectLayer(gui::Widget*);

	void changeMap(gui::Combobox*, gui::ListItem&);
	void changeChannel(gui::Combobox*, gui::ListItem&);
	void changeTexture(gui::Combobox*, gui::ListItem&);
	void changeBlendMode(gui::Combobox*, gui::ListItem&);
	void changeProjection(gui::Combobox*, gui::ListItem&);
	void changeOpacity(gui::Scrollbar*, int);
	void changeScaleX(gui::Scrollbar*, int);
	void changeScaleY(gui::Scrollbar*, int);
	void changeHeightParam(gui::Scrollbar*, int);
	void changeSlopeParam (gui::Scrollbar*, int);
	void changeConvexParam(gui::Scrollbar*, int);

	protected:	// Gui objects
	gui::Window* m_materialPanel;
	gui::Window* m_texturePanel;
	gui::Scrollpane* m_textureList;
	gui::Scrollpane* m_layerList;
	gui::Combobox*   m_materialList;
	gui::Combobox*   m_materialModes;
	gui::ItemList*   m_textureSelector;

	int m_selectedLayer;
	int m_selectedTexture;
	int m_selectedMaterial;
	int m_browseTarget;

	static int getListIndex(gui::ItemList*, const char*);
	static int getItemIndex(gui::Widget* w, gui::Widget* list);
	void populateMaps(gui::Combobox* list, int mask, uint sel) const;
	MaterialLayer* getLayer(gui::Widget*) const;
	void updateMaterial(gui::Widget*);
	void rebuildMaterial(bool bind=false);

	void colourPicked(const Colour&);
	void colourFinish(const Colour&);

	int  createTextureIcon(const char* name, const base::DDS& dds);
	void deleteTextureIcon(const char* name);

	bool loadTexture(ArrayTexture* target, int layer, const char* file, bool icon=false);

	protected:	// Data
	ArrayTexture m_diffuseMaps;
	ArrayTexture m_normalMaps;
	ArrayTexture m_heightMaps;
	std::vector<DynamicMaterial*> m_materials;		// Materials
	std::vector<TerrainTexture*>  m_textures;		// Textures
	std::vector<MapData>          m_mapInfo;		// Texture map data
	float          m_textureTiling[256];	// Texture tiling values
	vec2           m_terrainSize;			// Terrain map size
	bool           m_streaming;				// Use streamed textures
	FileSystem*    m_fileSystem;			// For finding files
	gui::Root*     m_gui;					// Gui
	base::Texture  m_textureIconTexture;	// Atlas of texture icons
	gui::IconList* m_textureIcons;		

	MaterialLayer* m_layerForColourPicker;
	gui::Combobox* m_boxForColourPicker;


};

