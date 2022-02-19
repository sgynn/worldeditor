#include "materialeditor.h"
#include "filesystem.h"
#include "terraineditor/editabletexture.h"
#include "widgets/filedialog.h"
#include "widgets/orderableitem.h"
#include "widgets/colourpicker.h"
#include <base/directory.h>
#include <base/xml.h>
#include <base/dds.h>
#include <base/png.h>
#include <base/gui/renderer.h>
#include <base/gui/skin.h>

using namespace base;


MaterialEditor::MaterialEditor(gui::Root* gui, FileSystem* fs, bool stream): m_streaming(stream), m_fileSystem(fs), m_gui(gui) {
	// Selector lists
	m_textureSelector = new gui::ItemList();
	m_textureSelector->addItem("Select colour", gui::Any(), -1);
	//m_textureSelector->addItem("Clipped", gui::Any(), -2); // ?

	// Set up gui callbacks
	setupGui();

	// Create material icon list
	m_textureIcons = new gui::IconList();
	m_textureIconTexture = Texture::create(1024, 1024, Texture::BC1, 0, false);
	int img = m_gui->getRenderer()->addImage("textureIcons", 1024, 1024, m_textureIconTexture.unit());
	m_textureIcons->setImageIndex(img);
	m_gui->addIconList("textureIcons", m_textureIcons);

	// Initial tiling values
	for(float& f: m_textureTiling) f = 1.0f;

	//m_diffuseMaps.createBlankTexture(0xffffffff, 4);	// Dont work properly
	//m_normalMaps.createBlankTexture(0xff8080ff, 4);
}

MaterialEditor::~MaterialEditor() {
	// Clear persistant gui lists
	m_materialList->clearItems();
	while(m_layerList->getWidgetCount()) {
		gui::Widget* w = m_layerList->getWidget( m_layerList->getWidgetCount()-1 );
		m_layerList->remove(w);
		delete w;
	}
	while(m_textureList->getWidgetCount()) {
		gui::Widget* w = m_textureList->getWidget( m_textureList->getWidgetCount()-1 );
		m_textureList->remove(w);
		delete w;
	}

	// Delete materials
	for(size_t i=0; i<m_materials.size(); ++i) delete m_materials[i];
	for(size_t i=0; i<m_textures.size(); ++i) delete m_textures[i];

	// Delete selector lists
	delete m_textureSelector;
}

void MaterialEditor::selectMaterial(DynamicMaterial* m) {
	for(size_t i=0; i<m_materials.size(); ++i) {
		if(m == m_materials[i]) {
			m_materialList->selectItem(i);
			selectMaterial(m_materialList, i);
			return;
		}
	}
}


// ------------------------------------------------------------------------------ //



static const char* layerTypes[] = { "auto", "weight", "colour", "indexed", "gradient" };
static const char* blendModes[] = { "normal", "height", "multiply", "add" };
static const char* projections[] = { "flat", "triplanar", "vertical" };
static const char* channels[]   = { "r", "g", "b", "a", "x" };
inline int enumerate(const char* key, const char** values, int size) {
	for(int i=0; i<size; ++i) {
		if(strcmp(key, values[i])==0) return i;
	}
	return -1;
}

int MaterialEditor::getMaterialCount() const { return m_materials.size(); }
DynamicMaterial* MaterialEditor::getMaterial() const {
	return getMaterial(m_selectedMaterial);
}
DynamicMaterial* MaterialEditor::getMaterial(int index) const {
	if((size_t)index < m_materials.size()) return m_materials[index];
	return 0;
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
	m->setTilingData(m_textureTiling);
	m->setCoordinates(m_terrainSize, vec2());
	m_materials.push_back(m);
	m_materialList->addItem( name );
	return m;
}
void MaterialEditor::destroyMaterial(int index) {
	delete m_materials[index];
	m_materials.erase( m_materials.begin() + index );
}

int readVector(const char* src, vec3& out) {
	int read = 0;
	char* end;
	while(*src && read<3) {
		out[read] = strtod(src, &end);
		if(end==src) break;
		if(*end==',' || *end==';' || *end==' ') ++end;
		src = end;
		++read;
	}
	for(int i=read; i<3; ++i) out[i] = out[read];
	return read;
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
			LayerType mode = (LayerType)enumerate(i->attribute("type", "normal"), layerTypes, 4);
			MaterialLayer* layer = mat->addLayer((LayerType) mode);
			layer->name = i->attribute("name");
			layer->blend = (::BlendMode) enumerate(i->attribute("blend", "normal"), blendModes, 4);
			layer->blendScale = i->attribute("blendscale", 1.f);
			layer->opacity = i->attribute("opacity", 1.f);
			layer->texture = i->attribute("texture", -1);
			layer->colour = i->attribute("colour", 0xffffff);
			layer->projection = (TexProjection)enumerate(i->attribute("projection", "flat"), projections, 3);

			// Scale
			int sr = readVector(i->attribute("scale", "10"), layer->scale);
			if(sr==1) layer->scale.y = layer->scale.z = layer->scale.x;
			else if(sr==2 && layer->projection != PROJECTION_FLAT) {
				layer->scale.z = layer->scale.x;
			}

			layer->mapIndex = i->attribute("map", 0);

			switch(mode) {
			case LAYER_COLOUR:
				layer->mapIndex = i->attribute("map", 0);
				break;
			case LAYER_WEIGHT:
				layer->mapIndex = i->attribute("map", 0);
				layer->mapData = enumerate(i->attribute("channel", "r"), channels, 5); // Which channel
				break;
			case LAYER_INDEXED:
				layer->mapIndex = i->attribute("map", 0);
				layer->mapData = i->attribute("channels", 1);	// Number of blended channels 1-4
				break;
			case LAYER_AUTO:
				readAutoParams( i->find("height"), layer->height );
				readAutoParams( i->find("slope"), layer->slope );
				readAutoParams( i->find("concavity"), layer->concavity );
				break;
			default: break;
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
	if(!r.attributes().empty()) e.add(r);
}

XMLElement MaterialEditor::serialiseMaterial(int index) {
	DynamicMaterial dummy; dummy.addLayer(LAYER_AUTO);
	char buffer[64];

	DynamicMaterial* mat = m_materials[index];
	XMLElement e("material");
	e.setAttribute("name", mat->getName());
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
		if(l->projection)          layer.setAttribute("projection", projections[l->projection]);

		// Scale can have different formats
		if(l->scale.x == l->scale.y && (l->scale.y == l->scale.z || l->projection==PROJECTION_FLAT)) sprintf(buffer, "%g", l->scale.x);
		else if(l->projection == PROJECTION_FLAT) sprintf(buffer, "%g,%g", l->scale.x, l->scale.y);
		else sprintf(buffer, "%g,%g,%g", l->scale.x, l->scale.y, l->scale.z);
		layer.setAttribute("scale", buffer);

		switch(l->type) {
		case LAYER_AUTO:
			writeAutoParams(layer, "height",    l->height, dummy.getLayer(0)->height);
			writeAutoParams(layer, "slope",     l->slope, dummy.getLayer(0)->slope);
			writeAutoParams(layer, "concavity", l->concavity, dummy.getLayer(0)->concavity);
			break;
		case LAYER_WEIGHT:
			layer.setAttribute("map", (int)l->mapIndex);
			layer.setAttribute("channel", channels[l->mapData]);
			break;
		case LAYER_COLOUR:
			layer.setAttribute("map", (int)l->mapIndex);
			break;
		case LAYER_INDEXED:
			layer.setAttribute("map", (int)l->mapIndex);
			layer.setAttribute("channels", (int)l->mapData);
			break;
		case LAYER_GRADIENT:
			printf("TODO: save gradient layer\n");
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
	t->tiling = 1.0;
	m_textures.push_back(t);
	m_textureSelector->addItem(name, gui::Any(), m_textures.size()-1); // ToDo: set icon
	return t;
}

void MaterialEditor::destroyTexture(int index) {
	delete m_textures[index];
	m_textures.erase(m_textures.begin() + index);
	m_diffuseMaps.removeTexture(index);
	m_normalMaps.removeTexture(index);
	m_textureSelector->removeItem(index);
}

void MaterialEditor::loadTexture(const XMLElement& e, int layer) {
	TerrainTexture* tex = createTexture( e.attribute("name") );
	tex->tiling = e.attribute("tiling", 1.0f);
	for(XML::iterator i=e.begin(); i!=e.end(); ++i) {
		if(*i == "diffuse") tex->diffuse = i->attribute("file");
		else if(*i == "normal") tex->normal = i->attribute("file");
		else if(*i == "height") tex->height = i->attribute("file");
	}
	// load files
	loadTexture( &m_diffuseMaps, layer, tex->diffuse, true);
	loadTexture( &m_normalMaps, layer, tex->normal );
	loadTexture( &m_heightMaps, layer, tex->height );

	// Update gui
	addTextureGUI(tex);
}

XMLElement MaterialEditor::serialiseTexture(int index) {
	TerrainTexture* tex = getTexture(index);
	XMLElement e("texture");
	e.setAttribute("name", tex->name);
	e.setAttribute("tiling", tex->tiling);
	if(!tex->diffuse.empty()) e.add("diffuse").setAttribute("file", m_fileSystem->getRelative(tex->diffuse));
	if(!tex->normal.empty()) e.add("normal").setAttribute("file", m_fileSystem->getRelative(tex->normal));
	if(!tex->height.empty()) e.add("height").setAttribute("file", m_fileSystem->getRelative(tex->height));
	return e;
}

void MaterialEditor::buildTextures() {
	int rd = m_diffuseMaps.build();
	int rn = m_normalMaps.build();
	int rh = m_heightMaps.build();
	if(rd) printf("Error: Texture %s is incompatible\n", (const char*)m_textures[rd&0xff]->diffuse);
	if(rn) printf("Error: Texture %s is incompatible\n", (const char*)m_textures[rn&0xff]->normal);
	if(rh) printf("Error: Texture %s is incompatible\n", (const char*)m_textures[rn&0xff]->height);
}

const base::Texture& MaterialEditor::getDiffuseArray() const {
	return m_diffuseMaps.getTexture();
}
const base::Texture& MaterialEditor::getNormalArray() const {
	return m_normalMaps.getTexture();
}
const base::Texture& MaterialEditor::getHeightArray() const {
	return m_heightMaps.getTexture();
}

// ------------------------------------------------------------------------------ //
const MaterialEditor::MapData& MaterialEditor::getMap(uint index) const {
	static MapData nullMap{0,0,0,0};
	if(index>= m_mapInfo.size()) return nullMap;
	return m_mapInfo[index];
}

void MaterialEditor::addMap(uint index, const char* name, int size, int channels, int flags) {
	if(m_mapInfo.size() <= index) m_mapInfo.resize(index+1, MapData{0,0});
	m_mapInfo[index].channels = channels;
	m_mapInfo[index].flags = flags;
	m_mapInfo[index].name = name;
	m_mapInfo[index].size = size;
}

void MaterialEditor::deleteMap(uint index) {
	if(index < m_mapInfo.size()) {
		m_mapInfo[index].name.clear();
		m_mapInfo[index].channels = 0;
		m_mapInfo[index].flags = 0;
		m_mapInfo[index].size = 0;
	}
}

void MaterialEditor::populateMaps(gui::ItemList* list, int mask, uint selected) const {
	list->clearItems();
	for(uint i=0; i<m_mapInfo.size(); ++i) {
		if(m_mapInfo[i].flags & mask) {
			list->addItem(m_mapInfo[i].name, i);
			if(selected == i) list->selectItem(list->getItemCount()-1);
		}
	}
}

// ------------------------------------------------------------------------------ //


void MaterialEditor::setupGui() {
	m_materialPanel = m_gui->getWidget<gui::Window>("materials");
	m_texturePanel = m_gui->getWidget<gui::Window>("textures");
	m_textureList = m_gui->getWidget<gui::Scrollpane>("texturelist");
	m_layerList = m_gui->getWidget<gui::Scrollpane>("layerlist");
	m_materialList = m_gui->getWidget<gui::Combobox>("materiallist");
	m_materialModes = m_gui->getWidget<gui::Combobox>("materialmodes");

	// setup main callbacks
	m_gui->getWidget<gui::Button>("addtexture")->eventPressed.bind(this, &MaterialEditor::addTexture);
	m_gui->getWidget<gui::Button>("removetexture")->eventPressed.bind(this, &MaterialEditor::removeTexture);
	m_gui->getWidget<gui::Button>("reloadtexture")->eventPressed.bind(this, &MaterialEditor::reloadTexture);


	m_gui->getWidget<gui::Button>("exportmaterial")->eventPressed.bind(this, &MaterialEditor::exportMaterial);
	m_gui->getWidget<gui::Button>("newmaterial")->eventPressed.bind(this, &MaterialEditor::addMaterial);
	m_gui->getWidget<gui::Button>("removelayer")->eventPressed.bind(this, &MaterialEditor::removeLayer);
	m_gui->getWidget<gui::Combobox>("addlayer")->eventSelected.bind(this, &MaterialEditor::addLayer);
	m_gui->getWidget<gui::Combobox>("addlayer")->setText("Add Layer");
	m_materialList->eventSelected.bind(this, &MaterialEditor::selectMaterial);
	m_materialList->eventSubmit.bind(this, &MaterialEditor::renameMaterial);
	m_materialModes->eventSelected.bind(this, &MaterialEditor::changeMode);

	m_selectedTexture = -1;
	m_selectedLayer = -1;
	m_selectedMaterial = -1;
}

int MaterialEditor::getItemIndex(gui::Widget* w, gui::Widget* list) {
	for(int i=0; i<list->getWidgetCount(); ++i) {
		gui::Widget* t = list->getWidget(i);
		for(gui::Widget* c = w; c && c!=list; c=c->getParent()) {
			if(t == c) return i;
		}
	}
	return -1;
}

int MaterialEditor::getListIndex(gui::ItemList* list, const char* n) {
	if(!n) return -1;
	for(size_t i=0; i<list->getItemCount(); ++i) {
		if(strcmp(list->getItem(i), n)==0) return i;
	}
	return -1;
}


// ------------------------------------------------------------------------------ //

void MaterialEditor::addTextureGUI(TerrainTexture* tex) {
	// Create gui item
	gui::Widget* w = m_gui->createWidget<gui::Widget>("textureitem");
	int top = m_textureList->getWidgetCount() * w->getSize().y;
	m_textureList->add(w);
	w->setPosition(0, top);
	w->setSize(m_textureList->getClientRect().width, w->getSize().y);
	m_textureList->setPaneSize( m_textureList->getClientRect().width, top + w->getSize().y);
	// Initial values
	w->getTemplateWidget<gui::Textbox>("texturename")->setText(tex->name);
	w->getTemplateWidget<gui::Textbox>("texturename")->eventSubmit.bind(this, &MaterialEditor::renameTexture);
	w->getTemplateWidget<gui::Textbox>("texturename")->eventLostFocus.bind(this, &MaterialEditor::renameTexture);
	w->getTemplateWidget<gui::Button>("browse_diffuse")->eventPressed.bind(this, &MaterialEditor::browseTexture);
	w->getTemplateWidget<gui::Button>("browse_normal")->eventPressed.bind(this, &MaterialEditor::browseTexture);
	w->getTemplateWidget<gui::Textbox>("diffuse")->setText( tex->diffuse );
	w->getTemplateWidget<gui::Textbox>("normal")->setText( tex->normal );
	gui::Scrollbar* tiling = w->getTemplateWidget<gui::Scrollbar>("tiling");
	tiling->eventChanged.bind(this, &MaterialEditor::changeTiling);
	tiling->setValue(tex->tiling * 100);
	w->eventGainedFocus.bind(this, &MaterialEditor::selectTexture);
	for(int i=0; i<w->getWidgetCount(); ++i) {
		w->getWidget(i)->eventGainedFocus.bind(this, &MaterialEditor::selectTexture);
	}
	// Icon
	int layer = m_textureList->getWidgetCount() - 1;
	if(layer < m_textureIcons->size()) w->getTemplateWidget<gui::Icon>("textureicon")->setIcon(m_textureIcons, layer);
	
}

void MaterialEditor::addTexture(gui::Button*) {
	TerrainTexture* tex = createTexture("New Texture");
	addTextureGUI(tex);

	// Selection
	int index = m_textureList->getWidgetCount() - 1;
	if(m_selectedTexture>=0) m_textureList->getWidget(m_selectedTexture)->setSelected(false);
	m_textureList->getWidget( index )->setSelected(true);
	m_selectedTexture = index;
}

void MaterialEditor::browseTexture(gui::Button* b) {
	// Save which one
	selectTexture(b);
	m_browseTarget = m_selectedTexture;
	if(b->getName()[7] == 'n') m_browseTarget |= 0x100;

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
	m_textureSelector->removeItem( m_selectedTexture + 1 );
	m_selectedTexture = -1;
	delete old;

	if(eventChangeTextureList) eventChangeTextureList();
}

// Remove alpha blocks from a DXT3 or DXT5 texture.
unsigned char* removeDXTAlpha(unsigned char* dxt, int w, int h) {
	dxt += 8; // colour is second half of block
	int blocks = (w/4) * (h/4);
	unsigned char* out = new unsigned char[blocks*8];
	for(int i=0; i<blocks; ++i) {
		unsigned char* src = dxt + i * 16;
		unsigned char* dst = out + i * 8;
		memcpy(dst, src, 8);
	}
	return out;
}

int MaterialEditor::createTextureIcon(const char* name, const DDS& dds) {
	int index = 0;
	Rect rect(0,0,64,64);
	while(index<m_textureIcons->size() && m_textureIcons->getIconRect(index).width) ++index;
	if(index == m_textureIcons->size()) {
		rect.x = (index%16)*64;
		rect.y = (index/16)*64;
		index = m_textureIcons->addIcon(name, rect);
	}
	else {
		rect = m_textureIcons->getIconRect(index);
		rect.width = rect.height = 64;
		m_textureIcons->setIconRect(index, rect);
		m_textureIcons->setIconName(index, name);
	}
	// Update image
	if(!dds.data) return index;
	int skip = 0;
	while(dds.width>>skip > 64) ++skip;
	unsigned char* data = dds.data[skip];
	if(dds.format==DDS::BC2 || dds.format==DDS::BC3) data = removeDXTAlpha(data, 64, 64);
	// Copy into texture
	m_textureIconTexture.setPixels(rect.x, rect.y, rect.width, rect.height, Texture::BC1, data);
	return index;
}

void MaterialEditor::deleteTextureIcon(const char* name) {
	int index = m_textureIcons->getIconIndex(name);
	if(index>=0) {
		const Rect& r = m_textureIcons->getIconRect(index);
		m_textureIcons->setIconRect(index, Rect(r.x, r.y,0,0) );
		m_textureIcons->setIconName(index, "");
	}
}

bool MaterialEditor::loadTexture(ArrayTexture* array, int layer, const char* filename, bool icon) {
	if(!filename) return false;
	String file = m_fileSystem->getFile(filename);
	DDS dds = DDS::load( file );
	if(dds.format != DDS::INVALID) {
		// Image icon
		if(icon) {
			char iconName[16];
			sprintf(iconName, "mat%d\n", layer);
			createTextureIcon(iconName, dds);
		}

		// Transfer into texture array
		array->setTexture(layer, dds);
		int r = array->build();
		if(r) printf("Error: Texture %s is incompatible\n", file.str());

		// Update active material
		if(m_selectedMaterial>=0)
			m_materials[m_selectedMaterial]->setTextures(this);

		return true;
	}
	else printf("Error: Failed to load %s\n", file.str());
	return false;
}

void MaterialEditor::setTexture(const char* file) {
	if(m_browseTarget<0) return;
	// Local file?
	String relative = m_fileSystem->getRelative(file);
	int type = m_browseTarget >> 8;
	enum Types { DIFFUSE=0, NORMAL=1, HEIGHT=2 };
	file = relative;

	// Update relevant textbox
	gui::Widget* w = m_textureList->getWidget(m_browseTarget & 0xff );
	const char* name = m_browseTarget&0x100? "normal": "diffuse";
	w->getTemplateWidget<gui::Textbox>(name)->setText(file);

	// Update texture object
	TerrainTexture* tex = m_textures[m_browseTarget & 0xff];
	if(type==NORMAL) tex->normal = file;
	else tex->diffuse = file;

	// Load texture
	int layer = m_browseTarget & 0xff;
	ArrayTexture* array = type==NORMAL? &m_normalMaps: &m_diffuseMaps;
	if(loadTexture(array, layer, file, type==DIFFUSE)) {
		w->getTemplateWidget<gui::Icon>("textureicon")->setIcon(m_textureIcons, layer);
	}

	if(eventChangeTextureList) eventChangeTextureList();
}

void MaterialEditor::reloadTexture(gui::Button*) {
	if(m_selectedTexture<0 || !m_gui->getFocusedWidget()) return;
	// reload selected texture
	const char* file = 0;
	ArrayTexture* array = 0;
//	if(strcmp( m_gui->getFocusedWidget()->getName(), "diffuse") == 0) {
		array = &m_diffuseMaps;
		file = m_textures[m_selectedTexture]->diffuse;
		loadTexture(array, m_selectedTexture, file);
//	}
//	else if(strcmp( m_gui->getFocusedWidget()->getName(), "normal") == 0) {
		array = &m_normalMaps;
		file = m_textures[m_selectedTexture]->normal;
		loadTexture(array, m_selectedTexture, file);
//	}
}

void MaterialEditor::changeTiling(gui::Scrollbar* s, int val) {
	int index = s->getParent()->getIndex();
	float value = val * 0.01f;
	m_textureTiling[index] = value;
}

void MaterialEditor::selectTexture(gui::Widget* w) {
	if(m_selectedTexture>=0) m_textureList->getWidget(m_selectedTexture)->setSelected(false);
	m_selectedTexture = getItemIndex(w, m_textureList);
	if(m_selectedTexture>=0) m_textureList->getWidget(m_selectedTexture)->setSelected(true);
}

void MaterialEditor::renameTexture(gui::Textbox* t) {
	int i = getItemIndex(t, m_textureList);
	m_textures[i]->name = t->getText();
	m_textureSelector->setItemName(i+1, t->getText());
	m_textureList->setFocus();
}
void MaterialEditor::renameTexture(gui::Widget* t) {
	renameTexture(t->cast<gui::Textbox>());
}


// ------------------------------------------------------------------------------ //


void MaterialEditor::addMaterial(gui::Button*) {
	createMaterial("New Material");
	m_materialList->selectItem( m_materials.size()-1 );
	selectMaterial(0, m_materials.size()-1);
}

void MaterialEditor::exportMaterial(gui::Button*) {
	DynamicMaterial* mat = getMaterial();
	if(mat) mat->exportMaterial();
}

void MaterialEditor::selectMaterial(gui::Combobox*, int index) {
	// Clear existing
	while(m_layerList->getWidgetCount()) {
		gui::Widget* w = m_layerList->getWidget( m_layerList->getWidgetCount()-1 );
		m_layerList->remove(w);
		delete w;
	}

	m_materialModes->selectItem((int)m_materials[index]->getMode());

	// Build gui for this material
	DynamicMaterial* m = m_materials[index];
	for(size_t i=0; i<m->size(); ++i) {
		addLayerGUI( m->getLayer(i) );
	}

	m_selectedMaterial = index;
	m_selectedLayer = -1;

	// compile and activate
	if(m->needsCompiling()) m->compile();
	m->setTextures(this);
	eventChangeMaterial(m);
}
void MaterialEditor::renameMaterial(gui::Combobox* c) {
	m_materials[m_selectedMaterial]->setName( c->getText() );
	m_materialList->setItemName( m_selectedMaterial, c->getText() );
	m_layerList->setFocus();
}
void MaterialEditor::changeMode(gui::Combobox*, int mode) {
	m_materials[m_selectedMaterial]->setMode((MaterialMode)mode);
	rebuildMaterial();
}
void MaterialEditor::addLayer(gui::Combobox* c, int i) {
	c->setText("Add");	// Hacking Combobox into a dropdown menu
	MaterialLayer* layer = m_materials[ m_selectedMaterial ]->addLayer( (LayerType)i );
	const char* names[] = { "Procedural layer", "Mapped Layer", "Colour layer", "Indexed Layer" };
	layer->name = names[i];
	addLayerGUI(layer);
	m_layerList->setOffset(0, m_layerList->getPaneSize().y - m_layerList->getSize().y);	// Scroll to bottom
	rebuildMaterial(true);
}
void MaterialEditor::removeLayer(gui::Button*) {
	if(m_selectedMaterial<0 || m_selectedLayer<0) return;
	m_materials[m_selectedMaterial]->removeLayer( m_selectedLayer );
	// Gui
	gui::Widget* w = m_layerList->getWidget(m_selectedLayer);
	for(int i=m_selectedLayer+1; i<m_layerList->getWidgetCount(); ++i) {
		gui::Widget* t = m_layerList->getWidget(i);
		t->setPosition(t->getPosition().x, t->getPosition().y - w->getSize().y);
	}
	m_layerList->remove(w);
	m_selectedLayer = -1;
	rebuildMaterial(true);
	delete w;
}
void MaterialEditor::renameLayer(gui::Widget* w) {
	if(m_selectedMaterial<0) return;
	gui::Textbox* t = w->cast<gui::Textbox>();
	int layerIndex = t->getParent()->getIndex();
	MaterialLayer* layer = m_materials[m_selectedMaterial]->getLayer( layerIndex );
	printf("Rename layer %d: %s -> %s\n", layerIndex, layer->name.str(), t->getText());
	layer->name = t->getText();
	m_layerList->setFocus();
}

void MaterialEditor::selectLayer(gui::Widget* w) {
	if(m_selectedLayer>=0) m_layerList->getWidget(m_selectedLayer)->setSelected(false);
	m_selectedLayer = getItemIndex(w, m_layerList);
	if(m_selectedLayer>=0) m_layerList->getWidget(m_selectedLayer)->setSelected(true);
	printf("Select layer %d\n", m_selectedLayer);
}

void MaterialEditor::expandLayer(gui::Button* b) {
	gui::Widget* w = b->getParent();
	int min = w->getWidget(2)->getSize().y;
	if(w->getSize().y > min) {
		w->setSize(w->getSize().x, min);
		b->setIcon("scroll_down");
	} else {
		gui::Widget* btm = w->getWidget( w->getWidgetCount()-1 );
		w->setSize(w->getSize().x, btm->getPosition().y + btm->getSize().y + 4);
		b->setIcon("scroll_up");
	}
	// Shift the rest
	int y = w->getPosition().y + w->getSize().y;
	int start = getItemIndex(w, m_layerList);
	for(int i=start+1; i<m_layerList->getWidgetCount(); ++i) {
		m_layerList->getWidget(i)->setPosition(0, y);
		y += m_layerList->getWidget(i)->getSize().y;
	}
	// update pane
	m_layerList->setAutosize(true);
}

void MaterialEditor::moveLayer(int from, int to) {
	if(from == to) return;
	m_materials[m_selectedMaterial]->moveLayer( from, to );
	m_selectedLayer = to;
	rebuildMaterial();
}

void MaterialEditor::toggleLayer(gui::Button* b) {
	MaterialLayer* layer = getLayer(b);
	layer->visible = !layer->visible;
	b->setIcon( layer->visible? "eye_open": "eye_closed" );
	updateMaterial(b);
}

template<typename T> T* addLayerWidget(gui::Root* gui, gui::Widget* parent, const char* name, const char* type) {
	gui::Widget* prev = parent->getWidget( parent->getWidgetCount()-1 );
	int y = prev->getPosition().y + prev->getSize().y + 2;
	static const int div = 100;

	if(name) {
		gui::Label* label = gui->createWidget<gui::Label>(Rect(4,y,div-4,20), "default");
		label->setCaption(name);
		parent->add(label);
	}
	if(type) {
		T* w = gui->createWidget<T>(Rect(div,y,parent->getSize().x-div-4,20), type, name);
		parent->add(w);
		w->setAnchor("tlr");
		return w;
	}
	return 0;
}

void MaterialEditor::addLayerGUI(MaterialLayer* layer) {
	gui::Widget* w = m_gui->createWidget<gui::Widget>("materiallayer");
	w->setSize(m_textureList->getClientRect().width, w->getSize().y);

	int count = m_layerList->getWidgetCount();
	gui::Widget* prev = count? m_layerList->getWidget(count-1): 0;
	int top = prev? prev->getPosition().y + prev->getSize().y: 0;
	m_layerList->add(w);
	w->setPosition(0, top);
	w->setSize( m_layerList->getPaneSize().x, 10);


	// Setup
	const char* icons[] = { "layer_a", "layer_w", "layer_c", "layer_i", "layer_g" };
	w->getTemplateWidget<gui::Icon>("typeicon")->setIcon( icons[layer->type] );
	w->getTemplateWidget<gui::Textbox>("layername")->setText( layer->name );
	w->getTemplateWidget<gui::Textbox>("layername")->eventLostFocus.bind(this, &MaterialEditor::renameLayer);
	w->getTemplateWidget<gui::Button>("visibility")->eventPressed.bind(this, &MaterialEditor::toggleLayer);
	w->getTemplateWidget<gui::Button>("visibility")->setIcon(layer->visible? "eye_open": "eye_closed");
	w->getTemplateWidget<gui::Button>("expand")->eventPressed.bind(this, &MaterialEditor::expandLayer);
	w->cast<OrderableItem>()->eventReordered.bind(this, &MaterialEditor::moveLayer);

	// Blend mode
	gui::Combobox* blend = addLayerWidget<gui::Combobox>(m_gui, w, "Blend", "droplist");
	blend->addItem("normal");
	blend->addItem("height");
	blend->addItem("multiply");
	blend->addItem("add");
	blend->selectItem( (int) layer->blend );
	blend->eventSelected.bind(this, &MaterialEditor::changeBlendMode);
	
	// Opacity
	gui::Scrollbar* opacity = addLayerWidget<gui::Scrollbar>(m_gui, w, "Opacity", "slider");
	opacity->setRange(0, 1000);
	opacity->setValue( layer->opacity * 1000 );
	opacity->eventChanged.bind(this, &MaterialEditor::changeOpacity);
	
	if(layer->type == LAYER_AUTO || layer->type == LAYER_WEIGHT) {
		// Texture picker
		gui::Combobox* tex = addLayerWidget<gui::Combobox>(m_gui, w, "Texture", "toolgrouplist");
		tex->eventSelected.bind(this, &MaterialEditor::changeTexture);
		tex->setIconList(m_textureIcons);
		tex->shareList(m_textureSelector);
		tex->selectItem(layer->texture+1);
		tex->setSize( tex->getSize().x, 30 );

		if(layer->texture<0) {
			char hexColour[12];
			sprintf(hexColour, "#%06x", layer->colour);
			tex->setText(hexColour);
		}

		// Triplanar option
		gui::Combobox* triplanar = addLayerWidget<gui::Combobox>(m_gui, w, "Projection", "droplist");
		triplanar->addItem("Flat");
		triplanar->addItem("Triplanar");
		triplanar->addItem("Vertical");
		triplanar->selectItem( (int)layer->projection );
		triplanar->eventSelected.bind(this, &MaterialEditor::changeProjection);
	}

	// Maps
	if(layer->type == LAYER_WEIGHT || layer->type == LAYER_COLOUR) {
		gui::Combobox* map = addLayerWidget<gui::Combobox>(m_gui, w, "Map", "droplist");
		map->eventSelected.bind(this, &MaterialEditor::changeMap);
		populateMaps(map, 0x3, layer->mapIndex);
		if(layer->type == LAYER_WEIGHT) {
			gui::Combobox* channel = addLayerWidget<gui::Combobox>(m_gui, w, "Channel", "droplist");
			channel->addItem("Red");
			channel->addItem("Green");
			channel->addItem("Blue");
			channel->addItem("Alpha");
			channel->addItem("Remainder");
			channel->eventSelected.bind(this, &MaterialEditor::changeChannel);
		}
	}
	else if(layer->type == LAYER_INDEXED) {
		gui::Combobox* map = addLayerWidget<gui::Combobox>(m_gui, w, "Index Map", "droplist");
		map->eventSelected.bind(this, &MaterialEditor::changeMap);
		populateMaps(map, 0x4, layer->mapIndex);
	}

	// Texture tiling
	if(layer->type != LAYER_COLOUR) {
		gui::Scrollbar* scaleX = addLayerWidget<gui::Scrollbar>(m_gui, w, "Tiling", "slider");
		gui::Scrollbar* scaleY = addLayerWidget<gui::Scrollbar>(m_gui, w, 0,        "slider");
		scaleX->setRange(1, 1000);
		scaleY->setRange(1, 1000);
		scaleX->setValue(layer->scale.x*40);
		scaleY->setValue(layer->scale.y*40);
		scaleX->eventChanged.bind(this, &MaterialEditor::changeScaleX);
		scaleY->eventChanged.bind(this, &MaterialEditor::changeScaleY);
	}


	// Procedural Parameters
	if(layer->type == LAYER_AUTO) {
		gui::Scrollbar* slider[3];
		slider[0] = addLayerWidget<gui::Scrollbar>(m_gui, w, "Height Min", "slider");
		slider[1] = addLayerWidget<gui::Scrollbar>(m_gui, w, "Height Max", "slider");
		slider[2] = addLayerWidget<gui::Scrollbar>(m_gui, w, "Height Blend", "slider");
		for(int i=0; i<3; ++i) {
			slider[i]->setRange(0, 1000); // need max terrain height
			slider[i]->setValue((&layer->height.min)[i] * (i==2?10:1));
			slider[i]->eventChanged.bind(this, &MaterialEditor::changeHeightParam);
		}

		slider[0] = addLayerWidget<gui::Scrollbar>(m_gui, w, "Slope Min", "slider");
		slider[1] = addLayerWidget<gui::Scrollbar>(m_gui, w, "Slope Max", "slider");
		slider[2] = addLayerWidget<gui::Scrollbar>(m_gui, w, "Slope Blend", "slider");
		for(int i=0; i<3; ++i) {
			slider[i]->setRange(0, 1000);
			slider[i]->setValue((&layer->slope.min)[i] * 1000);
			slider[i]->eventChanged.bind(this, &MaterialEditor::changeSlopeParam);
		}

		/*
		slider[0] = addLayerWidget<gui::Scrollbar>(m_gui, w, "Concave Min", "slider");
		slider[1] = addLayerWidget<gui::Scrollbar>(m_gui, w, "Concave Max", "slider");
		slider[2] = addLayerWidget<gui::Scrollbar>(m_gui, w, "Concave Blend", "slider");
		for(int i=0; i<3; ++i) {
			slider[i]->setRange(-1000, 1000);
			slider[i]->setValue((&layer->concavity.min)[i] * 1000);
			slider[i]->eventChanged.bind(this, &MaterialEditor::changeConvexParam);
		}
		*/
	}

	// Gradient
	if(layer->type == LAYER_GRADIENT) {
		gui::Combobox* input = addLayerWidget<gui::Combobox>(m_gui, w, "Input", "droplist");
		input->addItem("Height");
		input->addItem("Slope");
		// Need a gradient editor widget
	}

	// Layer selection
	w->eventGainedFocus.bind(this, &MaterialEditor::selectLayer);
	for(int i=0; i<3; ++i) {
		w->getWidget(i)->eventGainedFocus.bind(this, &MaterialEditor::selectLayer);
	}

	// Autosize box
	expandLayer(w->getTemplateWidget<gui::Button>("expand"));
}

// ------------------------------------------------------------------------ //

MaterialLayer* MaterialEditor::getLayer(gui::Widget* w) const {
	int i = getItemIndex(w, m_layerList);
	DynamicMaterial* mat = m_materials[ m_selectedMaterial ];
	return mat->getLayer(i);
}
void MaterialEditor::updateMaterial(gui::Widget* w) {
	int i = getItemIndex(w, m_layerList);
	DynamicMaterial* mat = m_materials[ m_selectedMaterial ];
	mat->update(i);
}
void MaterialEditor::rebuildMaterial(bool bindMaps) {
	DynamicMaterial* mat = m_materials[ m_selectedMaterial ];
	mat->compile();
	if(bindMaps) mat->setTextures(this);
	eventChangeMaterial( getMaterial() );	// Reapply material to all terrain patches
}

void MaterialEditor::changeMap(gui::Combobox* w, int i) {
	int index;
	w->getItemData(i).getValue(index);
	MaterialLayer* layer = getLayer(w);
	layer->mapIndex = index;
	rebuildMaterial(true);
	eventChangeMaterial( getMaterial() );	// Reapply material to all terrain patches
}
void MaterialEditor::changeChannel(gui::Combobox* w, int i) {
	getLayer(w)->mapData = i;
	rebuildMaterial(true);
}
void MaterialEditor::changeTexture(gui::Combobox* w, int i) {
	getLayer(w)->texture = i - 1;
	if(i==0) {
		// Open colour picker
		ColourPicker* picker = m_gui->getWidget<ColourPicker>("picker");
		if(picker) {
			picker->setColour( getLayer(w)->colour );
			picker->setVisible(true);
			picker->raise();

			m_layerForColourPicker = getLayer(w);
			m_boxForColourPicker = w;
			picker->eventChanged.bind(this, &MaterialEditor::colourPicked);
			picker->eventSubmit.bind(this, &MaterialEditor::colourFinish);
			picker->eventCancel.bind(this, &MaterialEditor::colourFinish);
		}
	}
	else rebuildMaterial();
}
void MaterialEditor::colourPicked(const Colour& c) {
	char buf[32];
	sprintf(buf, "Colour: #%06X", c.toRGB());
	m_layerForColourPicker->colour = c;
	m_boxForColourPicker->setText(buf);
	rebuildMaterial();
}
void MaterialEditor::colourFinish(const Colour& c) {
	ColourPicker* picker = m_gui->getWidget<ColourPicker>("picker");
	picker->eventChanged.unbind();
}
void MaterialEditor::changeBlendMode(gui::Combobox* w, int i) {
	getLayer(w)->blend = (BlendMode)i;
	rebuildMaterial();
}
void MaterialEditor::changeOpacity(gui::Scrollbar* w, int v) {
	getLayer(w)->opacity = v/1000.f;
	updateMaterial(w);
}
void MaterialEditor::changeProjection(gui::Combobox* w, int i) {
	getLayer(w)->projection = (TexProjection)i;
	rebuildMaterial();
}
void MaterialEditor::changeScaleX(gui::Scrollbar* w, int v) {
	MaterialLayer* l = getLayer(w);
	if(l->projection == PROJECTION_FLAT) l->scale.x = v/40.0;
	else l->scale.x = l->scale.z = v/40.0;
	if(w->getRoot()->getKeyMask() == gui::KeyMask::Shift) {
		gui::Scrollbar* other = w->getParent()->getWidget(w->getIndex() + 1)->cast<gui::Scrollbar>();
		if(other) other->setValue(v);
		return;
	}
	updateMaterial(w);
}
void MaterialEditor::changeScaleY(gui::Scrollbar* w, int v) {
	getLayer(w)->scale.y = v/40.0;
	updateMaterial(w);
}
void MaterialEditor::changeHeightParam(gui::Scrollbar* w, int v) {
	AutoParams& p = getLayer(w)->height;
	switch(w->getName()[8]) {
	case 'i': p.min = v; break;
	case 'a': p.max = v; break;
	case 'l': p.blend = v/10.f; break;
	}
	updateMaterial(w);
}
void MaterialEditor::changeSlopeParam (gui::Scrollbar* w, int v) {
	AutoParams& p = getLayer(w)->slope;
	switch(w->getName()[7]) {
	case 'i': p.min = v/1000.f; break;
	case 'a': p.max = v/1000.f; break;
	case 'l': p.blend = v/1000.f; break;
	}
	updateMaterial(w);
}
void MaterialEditor::changeConvexParam(gui::Scrollbar* w, int v) {
	AutoParams& p = getLayer(w)->concavity;
	switch(w->getName()[9]) {
	case 'i': p.min = v/1000.f; break;
	case 'a': p.max = v/1000.f; break;
	case 'l': p.blend = v/1000.f; break;
	}
	updateMaterial(w);
}



