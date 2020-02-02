#include "foliageeditor.h"
#include "heightmap.h"
#include "scene/material.h"
#include "scene/shader.h"
#include "scene/autovariables.h"

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
"	return mat3(x, y, z);\n}"
"void main() { vec3 pos=quatToMat(rot)*vectex.xyz+loc.xyz; gl_Position=transform*vec4(pos,1); texcoord=texCoord; worldNormal=mat3(modelMatrix)*normal; worldPos=(modelMatrix * vec4(pos,1)).xyz; }\n";
static const char* shaderSourceFS =
"#version 150\n"
"in vec2 texcoord;\nin vec3 worldNormal;\nin vec3 worldPos;\nout vec4 fragment;\n"
"uniform sampler2D diffuse;\nuniform vec3 lightDirection;\n"
"void main() { vec4 diff=texture2D(diffuse, texcoord.st);\n"
"if(diff.a < 0.5) discard;\n"
"float l = dot(normalize(worldNormal), normalize(lightDirection));\n"
"float s = (l+1)/1.3*0.2+0.1;\n"
"fragment = vec4(diff.rgb * max(s,l), 1.0); }";


scene::Material* FoliageEditor::getMaterial() {
	static scene::Material* material = 0;
	if(material) return material;

	scene::Shader* shader = new scene::Shader();
	shader->attach( new scene::ShaderPart(scene::VERTEX_SHADER, shaderSourceVS) );
	shader->attach( new scene::ShaderPart(scene::FRAGMENT_SHADER, shaderSourceFS) );
	shader->bindAttributeLocation("vertex",  0);
	shader->bindAttributeLocation("normal",  1);
	shader->bindAttributeLocation("texCoord",2);


	material = new scene::Material;
	scene::Pass* pass = material->addPass("default");
	pass->setShader(shader);
	pass->getParameters().set("lightDirection", vec3(1,1,1));
	pass->getParameters().setAuto("transform", scene::AUTO_MODEL_VIEW_PROJECTION_MATRIX);
	pass->getParameters().setAuto("modelMatrix", scene::AUTO_MODEL_MATRIX);
	pass->compile();
	char buffer[2048]; buffer[0] = 0;
	shader->getLog(buffer, sizeof(buffer));
	printf(buffer);
	if(!shader->isCompiled()) printf("Failed\n");
	return material;
}

// ======================================================================== //

FoliageEditor:: FoliageEditor(gui::Root* gui, FileSystem* fs, MapGrid* terrain, scene::Scene* scene) : m_foliage(0), m_fileSystem(fs), m_terrain(terrain), m_scene(scene) {
	setupGui(gui);
}
FoliageEditor::~FoliageEditor() {
	delete m_foliage;
}

void FoliageEditor::setupGui(gui::Root* gui) {
	m_window = gui->getWidget<gui::Window>("foliage");
	m_layerList = gui->getWidget<Listbox>("foliagelist");

	m_layerList->eventSelected.bind(this, &FoliageEditor::layerSelected);

	gui->getWidget<Combobox>("addfoliage")->eventSelected.bind(this, &FoliageEditor::addLayer);
	m_removeButton = gui->getWidget<Button>("removefoliage");
	m_removeButton->eventPressed.bind(this, &FoliageEditor::removeLayer);
}

void FoliageEditor::updateFoliage(const vec3& cam) {
	if(m_foliage) m_foliage->update(cam);
}

void FoliageEditor::addLayer(gui::Combobox* c, int i) {
	if(!m_foliage) {
		m_foliage = new Foliage(m_terrain, 3);
		m_scene->add(m_foliage);
	}

	FoliageLayer* layer;
	FoliageType type = (FoliageType)i;
	switch(type) {
	case FoliageType::Instanced: layer = new FoliageInstanceLayer(20, 50); break;
	case FoliageType::Grass:     layer = new GrassLayer(5, 20); break;
	}
	
	// Material
	layer->setMaterial( getMaterial() );
	
	Widget* widget = m_window->getRoot()->getWidget<Widget>("foliagelayer");
	widget = widget->clone();
	m_window->getParent()->add(widget);
	widget->setVisible(true);

	FoliageLayerEditor* editor = new FoliageLayerEditor(widget, layer, type, m_fileSystem);
	m_layerList->addItem(editor->getName(), editor, 16);
	m_foliage->addLayer(layer);
	editor->eventRenamed.bind(this, &FoliageEditor::layerRenamed);
}

void FoliageEditor::removeLayer(Button*) {
	int index = m_layerList->getSelectedIndex();
	if(index>=0) {
		FoliageLayerEditor* editor = m_layerList->getItemData(index).getValue<FoliageLayerEditor*>(0);
		m_layerList->removeItem(index);

		m_foliage->removeLayer( editor->getData() );
		delete editor->getData();
		delete editor;
	}
}

void FoliageEditor::layerSelected(Listbox* list, int i) {
	m_removeButton->setEnabled(i>=0);
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

// ======================================================================== //

#define CONNECT(Type, name, event, callback) { Widget* t=w->getNamedWidget(name); if(t&&t->cast<Type>()) t->cast<Type>()->event.bind(this, &FoliageLayerEditor::callback); }
FoliageLayerEditor::FoliageLayerEditor(Widget* w, FoliageLayer* layer, FoliageType type, FileSystem* fs) : m_layer(layer), m_fileSystem(fs), m_panel(w) {
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

	if(type==FoliageType::Instanced) {
		CONNECT(Combobox,  "mesh",      eventSelected, setMesh);
		CONNECT(Button,    "browsemesh", eventPressed, loadMesh);
		CONNECT(Combobox,  "alignment",  eventSelected, setAlignment);
		w->getNamedWidget("grass")->setVisible(false);
		m_name = "New Instanced Layer";
	}

	if(type==FoliageType::Grass) {
		CONNECT(Scrollbar, "spritesize", eventChanged, setSpriteSize);
		CONNECT(Combobox,  "sprite",     eventSelected, setSprite);
		CONNECT(Button,    "browsesprite", eventPressed, loadSprite);
		w->getNamedWidget("instanced")->setVisible(false);
		m_name = "New Grass Layer";
	}

	m_scale.set(1,1);
	w->getNamedWidget("name")->cast<Textbox>()->setText(m_name);
	w->getNamedWidget("density")->cast<Scrollbar>()->setValue(100);
}

void FoliageLayerEditor::renameLayer(gui::Textbox* t) {
	m_name = t->getText();
	if(eventRenamed) eventRenamed(this);
}

void FoliageLayerEditor::setDensityMap(gui::Combobox*, int) {}
void FoliageLayerEditor::setDensity(gui::Scrollbar*, int v) { m_density=v*0.01; refresh(); }
void FoliageLayerEditor::setRange(gui::Scrollbar*, int v)   { m_range=v; m_layer->setViewRange(m_range); }

void FoliageLayerEditor::setMinHeight(gui::Scrollbar*, int v) { m_height.min = v; refresh(); }
void FoliageLayerEditor::setMaxHeight(gui::Scrollbar*, int v) { m_height.max = v; refresh(); }
void FoliageLayerEditor::setMinSlope(gui::Scrollbar*, int v)  { m_slope.min = v*0.01; refresh(); }
void FoliageLayerEditor::setMaxSlope(gui::Scrollbar*, int v)  { m_slope.max = v*0.01; refresh(); }

void FoliageLayerEditor::setMinScale(gui::Scrollbar*, int v)  { m_scale.min = v*0.05; refresh(); }
void FoliageLayerEditor::setMaxScale(gui::Scrollbar*, int v)  { m_scale.max = v*0.05; refresh(); }

void FoliageLayerEditor::loadMesh(gui::Button*) {}
void FoliageLayerEditor::setMesh(gui::Combobox*, int) {}
void FoliageLayerEditor::setAlignment(gui::Combobox*, int i) {}

void FoliageLayerEditor::loadSprite(gui::Button*) {}
void FoliageLayerEditor::setSpriteSize(gui::Scrollbar*, int) {}
void FoliageLayerEditor::setSprite(gui::Combobox*, int) {}
void FoliageLayerEditor::setScaleMap(gui::Combobox*, int) {}


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

/*
 * How to handle multiple tiles?
 * Either a FoliageSystem per tile, or a shared one. (shared probably better)
 * 	- FoliageSystem::getActive() needs to skip onloaded parts.
 * 	- ChunkSize must be a multiple of mapSize
 * 	- Need multiple FoliageLayers for each layer if maps are used unless a FoliageLayer can have multiple maps
 */


