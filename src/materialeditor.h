#ifndef _MATERIAL_EDITOR_
#define _MATERIAL_EDITOR_

#include "arraytexture.h"
#include "dynamicmaterial.h"
#include "gui/widgets.h"
#include "gui/lists.h"

namespace base { class XMLElement; class Texture; }
class Library;
class EditableTexture;

/** Texture data */
struct TerrainTexture {
	int         index;
	gui::String name;
	gui::String diffuse;
	gui::String normal;
};

class MaterialEditor {
	public:
	MaterialEditor(gui::Root* gui, Library* lib, bool streaming);
	~MaterialEditor();
	void setupGui();

	// Material functions
	int              getMaterialCount() const;
	DynamicMaterial* createMaterial(const char* name);
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
	void             loadTexture(const base::XMLElement&);
	bool             loadTexture(ArrayTexture*, const char*) const;
	base::XMLElement serialiseTexture(int index);
	void             buildTextures();

	const base::Texture& getDiffuseArray() const;
	const base::Texture& getNormalArray() const;

	// Maps
	void addMap(const char* name, EditableTexture*);
	void deleteMap(const char* name);
	EditableTexture* getMap(const char* name) const;
	int serialiseMaps(base::XMLElement& e) const;
	
	public:
	DelegateS<void(DynamicMaterial*)> eventChangeMaterial;

	public:	// gui callbacks here
	void addTexture(gui::Button*);		// Add a texture
	void browseTexture(gui::Button*);	// Browse to texture
	void setTexture(const char*);		// Open dialog callback
	void removeTexture(gui::Button*);	// remove selected texture
	void reloadTexture(gui::Button*);	// reload selected texture
	void selectTexture(gui::Widget*);	// Click on texture
	void renameTexture(gui::Textbox*);	// Rename texture
	void addTextureGUI(TerrainTexture*);// Add texture gui item


	void addMaterial(gui::Button*);
	void selectMaterial(gui::Combobox*, int);
	void renameMaterial(gui::Combobox*);
	void addLayer(gui::Combobox*, int);
	void removeLayer(gui::Button*);
	void toggleLayer(gui::Button*);
	void renameLayer(gui::Textbox*);
	void moveLayer(int, int);
	void expandLayer(gui::Button*);
	void addLayerGUI(MaterialLayer*);
	void selectLayer(gui::Widget*);

	void changeMap(gui::Combobox*, int);
	void changeIndexMap(gui::Combobox*, int);
	void changeChannel(gui::Combobox*, int);
	void changeTexture(gui::Combobox*, int);
	void changeBlendMode(gui::Combobox*, int);
	void changeProjection(gui::Combobox*, int);
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
	gui::ItemList*   m_textureSelector;
	gui::ItemList*   m_mapSelector;

	int m_selectedLayer;
	int m_selectedTexture;
	int m_selectedMaterial;
	int m_browseTarget;

	static int getListIndex(gui::ItemList*, const char*);
	static int getItemIndex(gui::Widget* w, gui::Widget* list);
	MaterialLayer* getLayer(gui::Widget*) const;
	void updateMaterial(gui::Widget*);
	void rebuildMaterial(bool bind=false);


	protected:	// Data
	ArrayTexture m_diffuseMaps;
	ArrayTexture m_normalMaps;
	std::vector<DynamicMaterial*> m_materials;		// Materials
	std::vector<TerrainTexture*>  m_textures;		// Textures
	base::HashMap<EditableTexture*> m_imageMaps;	// Image maps
	bool         m_streaming;		// Use streamed textures
	Library*     m_library;			// Library to get data
	gui::Root*   m_gui;				// Gui


};


#endif

