#include "materialeditor.h"
#include "terraineditor/editabletexture.h"
#include "resource/library.h"
#include "widgets/filedialog.h"
#include <base/directory.h>
#include <base/xml.h>
#include <base/dds.h>
#include <base/png.h>

using namespace base;


MaterialEditor::MaterialEditor(gui::Root* gui, Library* lib, bool stream): m_streaming(stream), m_library(lib), m_gui(gui) {
	// Set up gui callbacks
	setupGui();
	m_diffuseMaps.createBlankTexture(0xffffffff, 4);
	m_normalMaps.createBlankTexture(0xff8080ff, 4);
}
MaterialEditor::~MaterialEditor() {
	// Delete materials
	for(size_t i=0; i<m_materials.size(); ++i) delete m_materials[i];
	for(size_t i=0; i<m_textures.size(); ++i) delete m_textures[i];

	// Delete maps
	for(HashMap<EditableTexture*>::iterator i=m_imageMaps.begin(); i!=m_imageMaps.end(); ++i) delete *i;
	m_imageMaps.clear();
}



// ------------------------------------------------------------------------------ //



static const char* layerTypes[] = { "auto", "weight", "colour", "indexed" };
static const char* blendModes[] = { "normal", "height", "multiply", "add" };
static const char* channels[]   = { "r", "g", "b", "a", "x" };
inline int enumerate(const char* key, const char** values, int size) {
	for(int i=0; i<size; ++i) {
		if(strcmp(key, values[i])==0) return i;
	}
	return -1;
}

int MaterialEditor::getMaterialCount() const { return m_materials.size(); }
DynamicMaterial* MaterialEditor::getMaterial(int index) const {
	return m_materials[index];
}
DynamicMaterial* MaterialEditor::getMaterial(const char* name) const {
	for(size_t i=0; i<m_materials.size(); ++i) {
		if(m_materials[i]->getName() == name) return m_materials[i];
	}
	return 0;
}
DynamicMaterial* MaterialEditor::createMaterial(const char* name) {
	DynamicMaterial* m = new DynamicMaterial(m_streaming);
	m->setName(name);
	m_materials.push_back(m);
	return m;
}
void MaterialEditor::destroyMaterial(int index) {
	delete m_materials[index];
	m_materials.erase( m_materials.begin() + index );
}

void readAutoParams(const XMLElement& e, AutoParams& p) {
	p.min   = e.attribute("min", p.min);
	p.max   = e.attribute("max", p.max);
	p.blend = e.attribute("blend", p.blend);
	p.noise = e.attribute("noise", p.noise);
}
DynamicMaterial* MaterialEditor::loadMaterial(const XMLElement& e) {
	DynamicMaterial* mat = createMaterial( e.attribute("name") );

	// read layers
	for(XML::iterator i=e.begin(); i!=e.end(); ++i) {
		if(*i=="layer") {
			int mode = enumerate(i->attribute("type", "normal"), layerTypes, 4);
			MaterialLayer* layer = mat->addLayer((LayerType) mode);
			layer->name = i->attribute("name");
			layer->blend = (::BlendMode) enumerate(i->attribute("blend", "normal"), blendModes, 4);
			layer->blendScale = i->attribute("blendscale", 1.f);
			layer->opacity = i->attribute("opacity", 1.f);
			layer->texture = i->attribute("texture", -1);
			layer->colour = i->attribute("colour", 0xffffff);
			layer->triplanar = strcmp(i->attribute("mode"), "triplanar")==0;
			layer->scale = vec3(1,1,1) * i->attribute("scale", 10.f);

			if(mode<3) {
				layer->map = i->attribute("map");
				layer->mapData = enumerate(i->attribute("channel", "r"), channels, 5);
			} else {
				layer->map = i->attribute("weightmap");
				layer->map2 = i->attribute("indexmap");
				layer->mapData = i->attribute("offset", 0);
			}

			if(mode==0) {
				readAutoParams( i->find("height"), layer->height );
				readAutoParams( i->find("slope"), layer->slope );
				readAutoParams( i->find("concavity"), layer->concavity );
			}
		}
	}
	return mat;
}
void writeAutoParams(XMLElement& e, const char* name, AutoParams& p, AutoParams& d) {
	XMLElement r(name);
	if(p.min != d.min) r.setAttribute("min", p.min);
	if(p.max != d.max) r.setAttribute("max", p.max);
	if(p.blend != d.blend) r.setAttribute("blend", p.blend);
	if(p.noise != d.noise) r.setAttribute("noise", p.noise);
	if(r.attributesBegin() != r.attributesEnd()) e.add(r);
}

XMLElement MaterialEditor::serialiseMaterial(int index) {
	DynamicMaterial dummy; dummy.addLayer(LAYER_AUTO);
	char buffer[64];

	DynamicMaterial* mat = m_materials[index];
	XMLElement e("material");
	for(size_t i=0; i<mat->size(); ++i) {
		MaterialLayer* l = mat->getLayer(i);
		XMLElement& layer = e.add("layer");
		layer.setAttribute("name", l->name);
		layer.setAttribute("type", layerTypes[ l->type ]);
		layer.setAttribute("blend", blendModes[ l->blend ]);
		if(l->blend==BLEND_HEIGHT) layer.setAttribute("blendscale", l->blendScale);
		if(l->opacity<1)           layer.setAttribute("opacity", l->opacity);
		if(l->texture>=0)          layer.setAttribute("texture", l->texture);
		else                       layer.setAttribute("colour", l->colour, true);
		if(l->triplanar)           layer.setAttribute("mode", "triplanar");

		// Scale can have different formats
		int same = (l->scale.x==l->scale.y? 1: 0) + (l->scale.x==l->scale.z? 2: 0);
		if(same==3 || (same==1 && l->triplanar)) layer.setAttribute("scale", l->scale.x);
		else {
			if(!l->triplanar) sprintf(buffer, "%g,%g", l->scale.x, l->scale.y);
			else if(same==1) sprintf(buffer, "%g,%g", l->scale.x, l->scale.z);
			else sprintf(buffer, "%g,%g,%g", l->scale.x, l->scale.y, l->scale.z);
			layer.setAttribute("scale", buffer);
		}

		switch(l->type) {
		case LAYER_AUTO:
			writeAutoParams(layer, "height",    l->height, dummy.getLayer(0)->height);
			writeAutoParams(layer, "slope",     l->slope, dummy.getLayer(0)->slope);
			writeAutoParams(layer, "concavity", l->concavity, dummy.getLayer(0)->concavity);
			break;
		case LAYER_WEIGHT:
			layer.setAttribute("map", l->map);
			layer.setAttribute("channel", l->mapData);
			break;
		case LAYER_COLOUR:
			layer.setAttribute("map", l->map);
			break;
		case LAYER_INDEXED:
			layer.setAttribute("weightmap", l->map);
			layer.setAttribute("heightmap", l->map2);
			break;
		}

	}
	return e;
}


// ------------------------------------------------------------------------------ //


int MaterialEditor::getTextureCount() const { return m_textures.size(); }
TerrainTexture* MaterialEditor::getTexture(int index) const {
	return m_textures[index];
}
TerrainTexture* MaterialEditor::createTexture(const char* name) {
	TerrainTexture* t = new TerrainTexture();
	t->name = name;
	m_textures.push_back(t);
	return t;
}

void MaterialEditor::destroyTexture(int index) {
	delete m_textures[index];
	m_textures.erase(m_textures.begin() + index);
	m_diffuseMaps.removeTexture(index);
	m_normalMaps.removeTexture(index);
}

bool MaterialEditor::loadTexture(ArrayTexture* array, const char* file) const {
	static char path[512];
	if(file[0]) {
		if(m_library->findFile(file, path)) {
			DDS dds = DDS::load(path);
			if(dds.format != DDS::INVALID) {
				array->addTexture(dds);
				return true;
			} else printf("Error: Failed to load %s\n", path);
		} else printf("Error: File not found: %s\n", file);
	}
	array->addBlankTexture();
	return false;
}

void MaterialEditor::loadTexture(const XMLElement& e) {
	TerrainTexture* tex = createTexture( e.attribute("name") );
	for(XML::iterator i=e.begin(); i!=e.end(); ++i) {
		if(*i == "diffuse") tex->diffuse = i->attribute("file");
		else if(*i == "normal") tex->normal = i->attribute("file");
	}
	// load files
	loadTexture( &m_diffuseMaps, tex->diffuse );
	loadTexture( &m_normalMaps, tex->normal );
	// Update gui
	addTexture(0);
	gui::Widget* w = m_textureList->getWidget(m_selectedTexture);
	w->getWidget<gui::Textbox>("texturename")->setText( tex->name );
	w->getWidget<gui::Textbox>("diffuse")->setText( tex->diffuse );
	w->getWidget<gui::Textbox>("normal")->setText( tex->normal );
}

XMLElement MaterialEditor::serialiseTexture(int index) {
	TerrainTexture* tex = getTexture(index);
	XMLElement e("texture");
	e.setAttribute("name", tex->name);
	if(!tex->diffuse.empty()) e.add("diffuse").setAttribute("file", tex->diffuse);
	if(!tex->normal.empty()) e.add("normal").setAttribute("file", tex->normal);
	return e;
}

void MaterialEditor::buildTextures() {
	m_diffuseMaps.build();
	m_normalMaps.build();
}

const base::Texture& MaterialEditor::getDiffuseArray() const {
	return m_diffuseMaps.getTexture();
}
const base::Texture& MaterialEditor::getNormalArray() const {
	return m_normalMaps.getTexture();
}

// ------------------------------------------------------------------------------ //


void MaterialEditor::addMap(const char* name, EditableTexture* map) {
	m_imageMaps[name] = map;
}

void MaterialEditor::deleteMap(const char* name) {
	if(!m_imageMaps.contains(name)) return;
	delete m_imageMaps[name];
	m_imageMaps.erase(name);
}

EditableTexture* MaterialEditor::getMap(const char* name) const {
	if(!m_imageMaps.contains(name)) return 0;
	return m_imageMaps[name];
}


// ------------------------------------------------------------------------------ //


void MaterialEditor::setupGui() {
	m_materialPanel = m_gui->getWidget<gui::Window>("materials");
	m_texturePanel = m_gui->getWidget<gui::Window>("textures");
	m_textureList = m_gui->getWidget<gui::Scrollpane>("texturelist");
	m_layerList = m_gui->getWidget<gui::Scrollpane>("layerlist");
	m_materialList = m_gui->getWidget<gui::Combobox>("materiallist");

	// setup main callbacks
	m_gui->getWidget<gui::Button>("addtexture")->eventPressed.bind(this, &MaterialEditor::addTexture);
	m_gui->getWidget<gui::Button>("removetexture")->eventPressed.bind(this, &MaterialEditor::removeTexture);
	m_gui->getWidget<gui::Button>("reloadtexture")->eventPressed.bind(this, &MaterialEditor::reloadTexture);


//	m_gui->getWidget<gui::Button>("newmaterial")->eventPressed.bind(this, &MaterialEditor::addMaterial);
//	m_gui->getWidget<gui::Button>("removelayer")->eventPressed.bind(this, &MaterialEditor::removeLayer);
//	m_gui->getWidget<gui::Combobox>("addlayer")->eventSelected.bind(this, &MaterialEditor::addLayer);
//	m_materialList->eventSelected.bind(this, &MaterialEditor::selectMaterial);
//	m_materialList->eventSubmit.bind(this, &MaterialEditor::renameMaterial);

	m_selectedTexture = -1;
	m_selectedLayer = -1;
	m_materialIndex = 0;
}

// ------------------------------------------------------------------------------ //

int MaterialEditor::getTextureIndex(gui::Widget* w) {
	for(int i=0; i<m_textureList->getWidgetCount(); ++i) {
		gui::Widget* t = m_textureList->getWidget(i);
		if(t == w || t == w->getParent() || t == w->getParent()->getParent()) return i;
	}
	return -1;
}

void MaterialEditor::addTexture(gui::Button*) {
	// Create gui item
	gui::Widget* w = m_gui->createWidget<gui::Widget>("textureitem");
	int count = m_textureList->getWidgetCount();
	int top = count? m_textureList->getWidget(count-1)->getPosition().y + w->getSize().y: 0;
	m_textureList->add(w);
	w->setSize(m_textureList->getClientRect().width, w->getSize().y);
	w->setPosition(0, top);
	m_textureList->setPaneSize( m_textureList->getClientRect().width, top + w->getSize().y);
	// Initial values
	w->getWidget<gui::Textbox>("texturename")->setText("New Texture");
	w->getWidget<gui::Button>("browse_diffuse")->eventPressed.bind(this, &MaterialEditor::browseTexture);
	w->getWidget<gui::Button>("browse_normal")->eventPressed.bind(this, &MaterialEditor::browseTexture);
	w->eventGainedFocus.bind(this, &MaterialEditor::selectTexture);
	for(int i=0; i<w->getWidgetCount(); ++i) {
		w->getWidget(i)->eventGainedFocus.bind(this, &MaterialEditor::selectTexture);
	}
	// Add material to list
	TerrainTexture* tex = new TerrainTexture;
	tex->index = count;
	tex->name = "New Texture";
	m_textures.push_back(tex);
	// Selection
	if(m_selectedTexture>=0) m_textureList->getWidget(m_selectedTexture)->setSelected(false);
	w->setSelected(true);
	m_selectedTexture = count;
}

void MaterialEditor::browseTexture(gui::Button* b) {
	// Save which one
	selectTexture(b);
	m_browseTarget = m_selectedTexture;
	if(*b->getName() == 'n') m_browseTarget |= 0x100;

	// open file dialog
	FileDialog* d = m_gui->getWidget<FileDialog>("filedialog");
	d->eventConfirm.bind(this, &MaterialEditor::setTexture);
	d->setFilter("*.dds,*.png");
	d->setFileName("");
	d->showOpen();
}

void MaterialEditor::removeTexture(gui::Button*) {
	if(m_selectedTexture<0) return;

	// delete selected texture
	delete m_textures[m_selectedTexture];
	m_textures.erase( m_textures.begin() + m_selectedTexture );
	m_diffuseMaps.removeTexture(m_selectedTexture);
	m_normalMaps.removeTexture(m_selectedTexture);
	buildTextures();

	// Change indices of all material layers that use this or layer texture index
	
	// Update GUI
	gui::Widget* old = m_textureList->getWidget( m_selectedTexture );
	for(int i=m_selectedTexture+1; i<m_textureList->getWidgetCount(); ++i) {
		gui::Widget* w = m_textureList->getWidget(i);
		w->setPosition( w->getPosition().x, w->getPosition().y - old->getSize().y);
	}
	m_textureList->remove( old );
	m_selectedTexture = -1;
	delete old;
}

void MaterialEditor::setTexture(const char* file) {
	if(m_browseTarget<0) return;
	// Local file?
	char buffer[1024];
	base::Directory::getRelativePath(file, buffer, 1024);

	// Update relevant textbox
	gui::Widget* w = m_textureList->getWidget(m_browseTarget & 0xff );
	const char* name = m_browseTarget&0x100? "normal": "diffuse";
	w->getWidget<gui::Textbox>(name)->setText(buffer);

	// Load texture
	ArrayTexture* array = m_browseTarget&0x100? &m_normalMaps: &m_diffuseMaps;
	DDS dds = DDS::load(file);
	if(dds.format != DDS::INVALID) {
		array->setTexture(m_browseTarget&0xff, dds);
		array->build();
	} else printf("Error: Failed to load %s\n", buffer);
}

void MaterialEditor::reloadTexture(gui::Button*) {
	if(m_selectedTexture<0 || !m_gui->getFocusedWidget()) return;
	// reload selected texture
	const char* file = 0;
	ArrayTexture* array = 0;
	if(strcmp( m_gui->getFocusedWidget()->getName(), "diffuse") == 0) {
		array = &m_diffuseMaps;
		file = m_textures[m_selectedTexture]->diffuse;
	}
	else if(strcmp( m_gui->getFocusedWidget()->getName(), "normal") == 0) {
		array = &m_normalMaps;
		file = m_textures[m_selectedTexture]->normal;
	}
	// Load it
	if(file) {
		DDS dds = DDS::load(file);
		if(dds.format != DDS::INVALID) {
			array->setTexture(m_selectedTexture, dds);
			array->build();
		} else printf("Error: Failed to load %s\n", file);
	}
}

void MaterialEditor::selectTexture(gui::Widget* w) {
	if(m_selectedTexture>=0) m_textureList->getWidget(m_selectedTexture)->setSelected(false);
	m_selectedTexture = getTextureIndex(w);
	if(m_selectedTexture>=0) m_textureList->getWidget(m_selectedTexture)->setSelected(true);
}

void MaterialEditor::renameTexture(gui::Textbox* t) {
	int i = getTextureIndex(t);
	m_textures[i]->name = t->getText();
}


// ------------------------------------------------------------------------------ //
