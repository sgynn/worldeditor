#include "foliageeditor.h"
#include "filesystem.h"
#include "heightmap.h"
#include "scene/material.h"
#include "scene/shader.h"
#include "scene/autovariables.h"
#include "model/bmloader.h"
#include "model/model.h"
#include "widgets/filedialog.h"
#include <base/xml.h>
#include <base/png.h>
#include <base/texture.h>

using namespace gui;

void Foliage::resolvePosition(const vec3& point, vec3& position, float& height) const {
	height = m_terrain->getHeight(point);
	position.set(point.x, height, point.z);
}
void Foliage::resolveNormal(const vec3& point, vec3& normal) const {
	// not so easy to get
	vec3 a,b,c,d;
	a=b=c=d=point;
	a.x-=0.3;
	b.x+=0.3;
	c.z-=0.3;
	d.z+=0.3;
	a.y = m_terrain->getHeight(a);
	b.y = m_terrain->getHeight(b);
	c.y = m_terrain->getHeight(c);
	d.y = m_terrain->getHeight(d);
	normal = (b-a).cross(c-d);
	normal.normalise();
}

int Foliage::getActive(const vec3& p, float cs, float range, IndexList& out) const {
	Point a( floor((p.x-range)/cs), floor((p.z-range)/cs) );
	Point b( ceil((p.x+range)/cs), ceil((p.z+range)/cs) );
	for(Point c=a; c.x<b.x; ++c.x) for(c.y=a.y; c.y<b.y; ++c.y) {
		vec3 check(c.x*cs+1, 0, c.y*cs+1);
		TerrainMap* map = m_terrain->getMap(check);
		if(map) out.push_back(c);
		// Need to check if this layer applies to this tile
	}
	return out.size();
}
float Foliage::getMapValue(const FoliageMap* map, const BoundingBox2D& bounds, const vec3& position) const {
	// wrap map
	float x = position.x / m_terrain->getTileSize();
	float y = position.y / m_terrain->getTileSize();
	return map->getValue(x-floor(x), y-floor(y));
}

// ======================================================================== //

static const char* shaderSourceVS =
"#version 150\n"
"in vec4 vertex;\nin vec3 normal;\nin vec2 texCoord;\n"
"uniform mat4 transform;\nuniform mat4 modelMatrix;\n"
"out vec2 texcoord;\nout vec3 worldNormal;\nout vec3 worldPos;\n"
"void main() { gl_Position=transform*vertex; texcoord=texCoord; worldNormal=mat3(modelMatrix)*normal; worldPos=(modelMatrix * vertex).xyz; }\n";
static const char* shaderSourceInstVS =
"#version 150\n"
"in vec4 vertex;\nin vec3 normal;\nin vec2 texCoord;\n"
"in vec4 loc; in vec4 rot;\n"
"uniform mat4 transform;\nuniform mat4 modelMatrix;\n"
"out vec2 texcoord;\nout vec3 worldNormal;\nout vec3 worldPos;\n"
"mat3 quatToMat(vec4 q) {\n"
"	vec3 x = vec3( 1-2*(q.y*q.y + q.z*q.z), 2*(q.x*q.y + q.w*q.z), 2*(q.x*q.z - q.w*q.y) );\n"
"	vec3 y = vec3( 2*(q.x*q.y - q.w*q.z), 1-2*(q.x*q.x + q.z*q.z), 2*(q.y*q.z + q.w*q.x) );\n"
"	vec3 z = cross(x, y);\n"
"	return mat3(x, y, z);\n}\n"
"void main() {"
"	mat3 m = quatToMat(rot);\n"
"	vec3 pos = m * vertex.xyz + loc.xyz;\n"
"	gl_Position = transform * vec4(pos,1);\n"
"	texcoord = texCoord;\n"
"	worldNormal = mat3(modelMatrix) * m * normal;\n"
"	worldPos = (modelMatrix * vec4(pos,1)).xyz;\n"
"}\n";
static const char* shaderSourceFS =
"#version 150\n"
"in vec2 texcoord;\nin vec3 worldNormal;\nin vec3 worldPos;\nout vec4 fragment;\n"
"uniform sampler2D diffuse;\nuniform vec3 lightDirection;\n"
"void main() { vec4 diff=texture2D(diffuse, texcoord.st);\n"
"if(diff.a < 0.5) discard;\n"
"float l = dot(normalize(worldNormal), normalize(lightDirection));\n"
"float s = (l+1)/1.3*0.2+0.1;\n"
"fragment = vec4(diff.rgb * max(s,l), 1.0); }";


scene::Material* FoliageEditor::createMaterial(FoliageType type, const char* diffuse) {
	static scene::Shader* shaders[2] = {0,0};
	if(!shaders[0]) {
		scene::ShaderPart* instancedVS = new scene::ShaderPart(scene::VERTEX_SHADER, shaderSourceInstVS);
		scene::ShaderPart* grassVS     = new scene::ShaderPart(scene::VERTEX_SHADER, shaderSourceVS);
		scene::ShaderPart* sharedFS    = new scene::ShaderPart(scene::FRAGMENT_SHADER, shaderSourceFS);

		shaders[0] = new scene::Shader();
		shaders[1] = new scene::Shader();
		shaders[0]->attach( instancedVS );
		shaders[1]->attach( grassVS );
		for(scene::Shader* shader: shaders) {
			shader->attach( sharedFS );
			shader->bindAttributeLocation("vertex",  0);
			shader->bindAttributeLocation("normal",  1);
			shader->bindAttributeLocation("texCoord",3);
		}
	}

	scene::Material* material = new scene::Material;
	scene::Pass* pass = material->addPass("default");
	pass->state.cullMode = scene::CULL_NONE;
	pass->setShader( shaders[(int)type] );
	pass->getParameters().set("lightDirection", vec3(1,1,1));
	pass->getParameters().setAuto("transform", scene::AUTO_MODEL_VIEW_PROJECTION_MATRIX);
	pass->getParameters().setAuto("modelMatrix", scene::AUTO_MODEL_MATRIX);
	
	if(diffuse) {
		base::PNG png = base::PNG::load(diffuse);
		if(png.data) {
			base::Texture tex = base::Texture::create(png.width, png.height, png.bpp/8, png.data);
			pass->setTexture("diffuse", new base::Texture(tex));
		}
	}

	pass->compile();
	char buffer[2048]; buffer[0] = 0;
	pass->getShader()->getLog(buffer, sizeof(buffer));
	printf(buffer);
	if(!pass->getShader()->isCompiled()) printf("Failed\n");
	return material;
}

// ======================================================================== //

FoliageEditor:: FoliageEditor(gui::Root* gui, FileSystem* fs, MapGrid* terrain, scene::Scene* scene) : m_foliage(0), m_fileSystem(fs), m_terrain(terrain), m_scene(scene) {
	setupGui(gui);
}
FoliageEditor::~FoliageEditor() {
	clear();
	delete m_foliage;
	delete m_meshList;
	delete m_spriteList;
}

void FoliageEditor::setupGui(gui::Root* gui) {
	m_window = gui->getWidget<gui::Window>("foliage");
	m_layerList = gui->getWidget<Listbox>("foliagelist");
	m_fileDialog = gui->getWidget<FileDialog>("filedialog");

	m_layerList->eventSelected.bind(this, &FoliageEditor::layerSelected);

	gui->getWidget<Combobox>("addfoliage")->eventSelected.bind(this, &FoliageEditor::addLayer);
	m_removeButton = gui->getWidget<Button>("removefoliage");
	m_removeButton->eventPressed.bind(this, &FoliageEditor::removeLayer);
	m_cloneButton = gui->getWidget<Button>("clonefoliage");
	m_cloneButton->eventPressed.bind(this, &FoliageEditor::duplicateLayer);

	m_meshList = new ItemList;
	m_spriteList = new ItemList;
}

void FoliageEditor::updateFoliage(const vec3& cam) {
	if(m_foliage) m_foliage->update(cam);
}

void FoliageEditor::addLayer(gui::Combobox* c, int i) {
	FoliageLayerEditor* editor = addLayer((FoliageType)i);
	editor->getPanel()->setVisible(true);
}

FoliageLayerEditor* FoliageEditor::addLayer(FoliageType type) {
	if(!m_foliage) {
		m_foliage = new Foliage(m_terrain, 3);
		m_scene->add(m_foliage);
	}

	FoliageLayer* layer;
	switch(type) {
	case FoliageType::Instanced: layer = new FoliageInstanceLayer(20, 50); break;
	case FoliageType::Grass:     layer = new GrassLayer(5, 20); break;
	}
	
	Widget* widget = m_window->getRoot()->getWidget<Widget>("foliagelayer");
	widget = widget->clone();
	widget->setName("");
	m_window->getParent()->add(widget);

	FoliageLayerEditor* editor = new FoliageLayerEditor(this, widget, layer, type);
	m_layerList->addItem(editor->getName(), editor, type==FoliageType::Instanced? 16: 18);
	m_foliage->addLayer(layer);
	editor->eventRenamed.bind(this, &FoliageEditor::layerRenamed);
	return editor;
}

void FoliageEditor::removeLayer(Button*) {
	int index = m_layerList->getSelectedIndex();
	if(index>=0) {
		FoliageLayerEditor* editor = m_layerList->getItemData(index).getValue<FoliageLayerEditor*>(0);
		destroy(editor);
		m_layerList->removeItem(index);
	}
}
void FoliageEditor::duplicateLayer(Button*) {
	int index = m_layerList->getSelectedIndex();
	if(index>=0) {
		FoliageLayerEditor* from = m_layerList->getItemData(index).getValue<FoliageLayerEditor*>(0);
		FoliageLayerEditor* to =  addLayer(from->getType());
		to->load(from->save()); // meh ?
		to->getPanel()->setVisible(true);
	}
}

void FoliageEditor::layerSelected(Listbox* list, int i) {
	m_removeButton->setEnabled(i>=0);
	m_cloneButton->setEnabled(i>=0);
	if(i>=0) {
		FoliageLayerEditor* editor = 0;
		list->getItemData(i).read(editor);
		if(editor) {
			editor->getPanel()->setVisible(true);
			editor->getPanel()->raise();
		}
	}
}

void FoliageEditor::layerRenamed(FoliageLayerEditor* e) {
	for(uint i=0; i<m_layerList->getItemCount(); ++i) {
		if(m_layerList->getItemData(i).getValue<FoliageLayerEditor*>(0) == e) {
			m_layerList->setItemName(i, e->getName());
			break;
		}
	}
}

void FoliageEditor::clear() {
	for(uint i=0; i<m_layerList->getItemCount(); ++i) {
		FoliageLayerEditor* editor = m_layerList->getItemData(i).getValue<FoliageLayerEditor*>(0);
		destroy(editor);
	}
	m_layerList->clearItems();
}

void FoliageEditor::destroy(FoliageLayerEditor* editor) {
	m_foliage->removeLayer( editor->getData() );
	editor->getPanel()->removeFromParent();
	delete editor->getPanel();
	delete editor->getData();
	delete editor;
}


// ======================================================================== //

#define CONNECT(Type, name, event, callback) { Widget* t=w->getNamedWidget(name); if(t&&t->cast<Type>()) t->cast<Type>()->event.bind(this, &FoliageLayerEditor::callback); else printf("Missing widget: %s\n", name); }
FoliageLayerEditor::FoliageLayerEditor(FoliageEditor* editor, Widget* w, FoliageLayer* layer, FoliageType type) : m_editor(editor), m_layer(layer), m_type(type), m_panel(w) {
	CONNECT(Textbox, "name", eventSubmit, renameLayer);

	CONNECT(Combobox, "densitymap", eventSelected, setDensityMap);
	CONNECT(Scrollbar, "density",   eventChanged, setDensity);
	CONNECT(Scrollbar, "range",     eventChanged, setRange);
	CONNECT(Scrollbar, "minheight", eventChanged, setMinHeight);
	CONNECT(Scrollbar, "maxheight", eventChanged, setMaxHeight);
	CONNECT(Scrollbar, "minslope",  eventChanged, setMinSlope);
	CONNECT(Scrollbar, "maxslope",  eventChanged, setMaxSlope);
	CONNECT(Scrollbar, "minscale",  eventChanged, setMinScale);
	CONNECT(Scrollbar, "maxscale",  eventChanged, setMaxScale);

	switch(type) {
	case FoliageType::Instanced:
		CONNECT(Combobox,  "mesh",      eventSelected, setMesh);
		CONNECT(Button,    "browsemesh", eventPressed, loadMesh);
		CONNECT(Combobox,  "alignment",  eventSelected, setAlignment);
		w->getNamedWidget("grass")->setVisible(false);
		w->getNamedWidget("mesh")->cast<Combobox>()->shareList(editor->m_meshList);
		m_name = "New Instanced Layer";
		m_densityMax = 0.5;
		break;

	case FoliageType::Grass:
		CONNECT(Combobox,  "sprite",     eventSelected, setSprite);
		CONNECT(Button,    "browsesprite", eventPressed, loadSprite);
		w->getNamedWidget("instanced")->setVisible(false);
		w->getNamedWidget("sprite")->cast<Combobox>()->shareList(editor->m_spriteList);
		m_name = "New Grass Layer";
		m_densityMax = 8;
		break;
	}

	// Defaults
	m_range = 100;
	m_density = m_densityMax * 0.5;
	m_scale.set(1,1);
	m_slope.set(0,1);
	m_height.set(0,1000);
	updateSliders();

	// Icon
	Icon* icon = w->getWidget<Icon>("icon");
	icon->setIcon(type==FoliageType::Instanced? "Foliage": "Grass");
}

#define SET_SLIDER(name, value, max) { Scrollbar* t=m_panel->getNamedWidget(name)->cast<Scrollbar>(); if(t) t->setValue(value*1000/(max)); }
void FoliageLayerEditor::updateSliders() {
	m_panel->getNamedWidget("name")->cast<Textbox>()->setText(m_name);
	SET_SLIDER("range", m_range, 1000);
	SET_SLIDER("density", m_density, m_densityMax);
	SET_SLIDER("minheight", m_height.min, 500);
	SET_SLIDER("maxheight", m_height.max, 500);
	SET_SLIDER("minslope", m_slope.min, 1);
	SET_SLIDER("maxslope", m_slope.max, 1);
	SET_SLIDER("minscale", m_scale.min, 10);
	SET_SLIDER("maxscale", m_scale.max, 10);
}

inline int getListIndex(ItemList* list, const char* name) {
	for(uint i=0; i<list->getItemCount(); ++i) {
		if(strcmp(list->getItem(i), name)==0) return i;
	}
	return -1;
}

void FoliageLayerEditor::renameLayer(gui::Textbox* t) {
	m_name = t->getText();
	if(eventRenamed) eventRenamed(this);
}

void FoliageLayerEditor::setDensityMap(gui::Combobox*, int) {}
void FoliageLayerEditor::setDensity(gui::Scrollbar*, int v) { m_density=v*0.001*m_densityMax; refresh(); }
void FoliageLayerEditor::setRange(gui::Scrollbar*, int v)   { m_range=v; m_layer->setViewRange(m_range); }

void FoliageLayerEditor::setMinHeight(gui::Scrollbar*, int v) { m_height.min = v*0.5; refresh(); }
void FoliageLayerEditor::setMaxHeight(gui::Scrollbar*, int v) { m_height.max = v*0.5; refresh(); }
void FoliageLayerEditor::setMinSlope(gui::Scrollbar*, int v)  { m_slope.min = v*0.001; refresh(); }
void FoliageLayerEditor::setMaxSlope(gui::Scrollbar*, int v)  { m_slope.max = v*0.001; refresh(); }

void FoliageLayerEditor::setMinScale(gui::Scrollbar*, int v)  { m_scale.min = v*0.01; refresh(); }
void FoliageLayerEditor::setMaxScale(gui::Scrollbar*, int v)  { m_scale.max = v*0.01; refresh(); }

// -------------------------------------------------------------------------------------- //
void FoliageLayerEditor::loadMesh(gui::Button*) {
	m_editor->m_fileDialog->eventConfirm.bind(this, &FoliageLayerEditor::loadMeshFile);
	m_editor->m_fileDialog->setFilter("*.bm");
	m_editor->m_fileDialog->setFileName("");
	m_editor->m_fileDialog->showOpen();
}
void FoliageLayerEditor::loadMeshFile(const char* file) {
	m_file = file;
	const char* name = strrchr(file, '/') + 1;
	Combobox* list = m_panel->getNamedWidget("mesh")->cast<Combobox>();
	int index = getListIndex(m_editor->m_meshList, name);
	if(index<0) {
		FoliageMesh mesh { file, 0,0,0,0 };
		mesh.material = m_editor->createMaterial(m_type, "./maps/white.png");
		base::bmodel::Model* model = base::bmodel::BMLoader::load(file);
		if(model && model->getMeshCount()) {
			mesh.mesh = model->getMesh(0);
			mesh.mesh->getVertexBuffer()->createBuffer();
			mesh.mesh->getIndexBuffer()->createBuffer();
		}
		list->addItem(name, mesh);
		list->setText(name);
		index = list->getItemCount()-1;
	}
	list->selectItem(index);
	setMesh(list, index);
}
void FoliageLayerEditor::setMesh(gui::Combobox* list, int index) {
	FoliageMesh* mesh = list->getItemData(index).cast<FoliageMesh>();
	static_cast<FoliageInstanceLayer*>(m_layer)->setMesh(mesh->mesh);
	m_layer->setMaterial(mesh->material);
	refresh();
}
void FoliageLayerEditor::setAlignment(gui::Combobox*, int i) {
}

// -------------------------------------------------------------------------------------- //
void FoliageLayerEditor::loadSprite(gui::Button*) {
	m_editor->m_fileDialog->eventConfirm.bind(this, &FoliageLayerEditor::loadSpriteFile);
	m_editor->m_fileDialog->setFilter("*.png");
	m_editor->m_fileDialog->setFileName("");
	m_editor->m_fileDialog->showOpen();
}
void FoliageLayerEditor::loadSpriteFile(const char* file) {
	m_file = file;
	const char* name = strrchr(file, '/') + 1;
	Combobox* list = m_panel->getNamedWidget("sprite")->cast<Combobox>();
	int index = getListIndex(m_editor->m_spriteList, name);
	if(index<0) {
		FoliageSprite sprite { file, m_editor->createMaterial(m_type, file) };
		list->addItem(name, sprite);
		list->setText(name);
		index = list->getItemCount()-1;
	}
	list->selectItem(index);
	setSprite(list, index);
}
void FoliageLayerEditor::setSprite(gui::Combobox* list, int index) {
	FoliageSprite* sprite = list->getItemData(index).cast<FoliageSprite>();
	m_layer->setMaterial(sprite->material);
	refresh();
}
void FoliageLayerEditor::setScaleMap(gui::Combobox*, int) {}
// -------------------------------------------------------------------------------------- //

void FoliageLayerEditor::refresh() {
	m_layer->setDensity(m_density);
	m_layer->setSlopeRange(m_slope.min, m_slope.max);
	m_layer->setHeightRange(m_height.min, m_height.max);
	m_layer->setScaleRange(m_scale.min, m_scale.max);
	if(m_type == FoliageType::Grass) {
		// set sprite
	}
	else if(m_type == FoliageType::Instanced) {
		// set mesh
	}
	m_layer->regenerate();
}

// =============================================== //

using base::XMLElement;
inline void saveRange(XMLElement& e, const char* key, const ::Range& range) {
	if(range.max<= range.min) return;
	XMLElement& r = e.add(key);
	r.setAttribute("min", range.min);
	r.setAttribute("max", range.max);
}
inline void loadRange(const XMLElement& e, ::Range& range) {
	range.min = e.attribute("min", range.min);
	range.max = e.attribute("max", range.max);
}

XMLElement FoliageEditor::save() const {
	XMLElement e("foliage");
	for(uint i=0; i<m_layerList->getItemCount(); ++i) {
		const FoliageLayerEditor* layer = m_layerList->getItemData(i).getValue<FoliageLayerEditor*>(0);
		e.add(layer->save());
	}
	return e;
}
XMLElement FoliageLayerEditor::save() const {
	XMLElement e("layer");
	e.setAttribute("type", (int)m_type);
	e.setAttribute("name", m_name);
	e.setAttribute("range", m_range);
	e.setAttribute("density", m_density);
	saveRange(e, "height", m_height);
	saveRange(e, "slope", m_slope);
	saveRange(e, "scale", m_scale);

	if(m_type==FoliageType::Grass) {
		Combobox* list = m_panel->getNamedWidget("sprite")->cast<Combobox>();
		FoliageSprite* sprite = list->getSelectedData().cast<FoliageSprite>();
		if(sprite) {
			XMLElement& f = e.add("sprite");
			f.setAttribute("file", m_editor->m_fileSystem->getRelative(sprite->file));
		}
	}
	return e;
}

void FoliageEditor::load(const XMLElement& e) {
	clear();
	for(const XMLElement& layerData: e) {
		int type = layerData.attribute("type", 0);
		FoliageLayerEditor* editor = addLayer((FoliageType)type);
		editor->load(layerData);
	}
}

void FoliageLayerEditor::load(const XMLElement& e) {
	m_name = e.attribute("name", m_name);
	m_range = e.attribute("range", m_range);
	m_density = e.attribute("density", m_density);
	loadRange(e.find("height"), m_height);
	loadRange(e.find("slope"), m_slope);
	loadRange(e.find("scale"), m_scale);

	updateSliders();
	
	if(m_type==FoliageType::Grass) {
		const XMLElement& f = e.find("sprite");
		const char* file = f.attribute("file");
		if(file[0]) loadSpriteFile( m_editor->m_fileSystem->getFile(file) );
	}

	refresh();
}



/*
 * How to handle multiple tiles?
 * Either a FoliageSystem per tile, or a shared one. (shared probably better)
 * 	- FoliageSystem::getActive() needs to skip onloaded parts.
 * 	- ChunkSize must be a multiple of mapSize
 * 	- Need multiple FoliageLayers for each layer if maps are used unless a FoliageLayer can have multiple maps
 */


