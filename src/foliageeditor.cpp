#include "foliageeditor.h"
#include "filesystem.h"
#include "heightmap.h"
#include <base/material.h>
#include <base/shader.h>
#include <base/autovariables.h>
#include <base/hardwarebuffer.h>
#include <base/bmloader.h>
#include <base/model.h>
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
"	mat3 m = quatToMat(rot.yzwx);\n"
"	vec3 pos = m * vertex.xyz * loc.w + loc.xyz;\n"
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


base::Material* FoliageEditor::createMaterial(FoliageType type, const char* diffuse) {
	static base::Shader* shaders[2] = {0,0};
	if(!shaders[0]) {
		base::ShaderPart* instancedVS = new base::ShaderPart(base::VERTEX_SHADER, shaderSourceInstVS);
		base::ShaderPart* grassVS     = new base::ShaderPart(base::VERTEX_SHADER, shaderSourceVS);
		base::ShaderPart* sharedFS    = new base::ShaderPart(base::FRAGMENT_SHADER, shaderSourceFS);

		shaders[0] = new base::Shader();
		shaders[1] = new base::Shader();
		shaders[0]->attach( instancedVS );
		shaders[1]->attach( grassVS );
		for(base::Shader* shader: shaders) {
			shader->attach( sharedFS );
			shader->bindAttributeLocation("vertex",  0);
			shader->bindAttributeLocation("normal",  1);
			shader->bindAttributeLocation("texCoord",3);
		}
	}

	base::Material* material = new base::Material;
	base::Pass* pass = material->addPass("default");
	pass->state.cullMode = base::CULL_NONE;
	pass->setShader( shaders[(int)type] );
	pass->getParameters().set("lightDirection", vec3(1,1,1));
	pass->getParameters().setAuto("transform", base::AUTO_MODEL_VIEW_PROJECTION_MATRIX);
	pass->getParameters().setAuto("modelMatrix", base::AUTO_MODEL_MATRIX);
	
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

FoliageEditor:: FoliageEditor(gui::Root* gui, FileSystem* fs, MapGrid* terrain, base::SceneNode* node) : m_foliage(0), m_fileSystem(fs), m_terrain(terrain), m_node(node) {
	setupGui(gui);
}
FoliageEditor::~FoliageEditor() {
	clear();
	delete m_foliage;
	delete m_meshList;
	delete m_spriteList;
}

void FoliageEditor::setupGui(gui::Root* gui) {
	createPanel(gui, "foliage", "foliage.xml");
	createToolButton(gui, "Foliage");

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

void FoliageEditor::close() {
	for(uint i=0; i<m_layerList->getItemCount(); ++i) {
		FoliageLayerEditor* editor = m_layerList->getItemData(i).getValue<FoliageLayerEditor*>(0);
		if(editor) editor->getPanel()->setVisible(false);
	}
}

void FoliageEditor::update(const Mouse&, const Ray& ray, base::Camera*, InputState&) {
	if(m_foliage) m_foliage->update(ray.start);
}

void FoliageEditor::addLayer(gui::Combobox* c, int i) {
	FoliageLayerEditor* editor = addLayer((FoliageType)i);
	showEditor(editor);
}

FoliageLayerEditor* FoliageEditor::addLayer(FoliageType type) {
	if(!m_foliage) {
		m_foliage = new Foliage(m_terrain, 3);
		m_node->addChild(m_foliage);
	}

	FoliageLayer* layer;
	switch(type) {
	case FoliageType::Instanced: layer = new FoliageInstanceLayer(20, 50); break;
	case FoliageType::Grass:     layer = new GrassLayer(5, 20); break;
	}
	
	Widget* widget = m_panel->getRoot()->getWidget<Widget>("foliagelayer");
	widget = widget->clone();
	widget->setName("");
	m_panel->getParent()->add(widget);

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
		showEditor(to);
	}
}

void FoliageEditor::layerSelected(Listbox* list, int i) {
	m_removeButton->setEnabled(i>=0);
	m_cloneButton->setEnabled(i>=0);
	if(i>=0) {
		FoliageLayerEditor* editor = 0;
		list->getItemData(i).read(editor);
		if(editor) {
			showEditor(editor);
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

void FoliageEditor::showEditor(FoliageLayerEditor* layer) {
	layer->getPanel()->setVisible(true);
	layer->getPanel()->raise();
	
	// Don't perfectly overlap them
	for(uint i=0; i<m_layerList->getItemCount(); ++i) {
		FoliageLayerEditor* editor = m_layerList->getItemData(i).getValue<FoliageLayerEditor*>(0);
		if(editor != layer && editor->getPanel()->isVisible() && editor->getPanel()->getPosition() == layer->getPanel()->getPosition()) {
			layer->getPanel()->setPosition(layer->getPanel()->getPosition() + Point(32, 32));
			
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

#define CONNECT(Type, name, event, callback) { Type* t=w->getWidget<Type>(name); if(t) t->event.bind(this, &FoliageLayerEditor::callback); else printf("Missing widget: %s\n", name); }
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

	Combobox* align = 0;
	w->getWidget<Widget>("alignrange")->setVisible(false);

	switch(type) {
	case FoliageType::Instanced:
		CONNECT(Combobox,  "mesh",      eventSelected, setMesh);
		CONNECT(Button,    "browsemesh", eventPressed, loadMesh);
		CONNECT(Combobox,  "alignment",  eventSelected, setAlignment);
		CONNECT(Scrollbar, "minalign",  eventChanged, setMinAngle);
		CONNECT(Scrollbar, "maxalign",  eventChanged, setMaxAngle);
		w->getWidget<Widget>("grass")->setVisible(false);
		w->getWidget<Combobox>("mesh")->shareList(editor->m_meshList);
		m_name = "New Instanced Layer";
		m_densityMax = 0.5;
		m_density = 0.01;

		align = w->getWidget<Combobox>("alignment");
		align->addItem("Vertical");
		align->addItem("Normal");
		align->addItem("Absolute");
		align->addItem("Relative");
		align->selectItem(0);
		break;

	case FoliageType::Grass:
		CONNECT(Combobox,  "sprite",     eventSelected, setSprite);
		CONNECT(Button,    "browsesprite", eventPressed, loadSprite);
		w->getWidget<Widget>("instanced")->setVisible(false);
		w->getWidget<Combobox>("sprite")->shareList(editor->m_spriteList);
		m_name = "New Grass Layer";
		m_densityMax = 16;
		m_density = 1;
		break;
	}

	// Defaults
	m_range = 100;
	m_scale.set(1,1);
	m_slope.set(0,1);
	m_angle.set(0,0);
	m_height.set(0,1000);
	m_align = 0;
	updateSliders();

	// Icon
	Icon* icon = w->getWidget<Icon>("icon");
	icon->setIcon(type==FoliageType::Instanced? "Foliage": "Grass");
}

#define SET_SLIDER(name, value, max) { Scrollbar* t=m_panel->getWidget<Scrollbar>(name); if(t) t->setValue(value*1000/(max)); }
void FoliageLayerEditor::updateSliders() {
	m_panel->getWidget<Textbox>("name")->setText(m_name);
	SET_SLIDER("range", m_range, 1000);
	SET_SLIDER("density", powf(m_density/m_densityMax,0.5), 1);
	SET_SLIDER("minheight", m_height.min, 500);
	SET_SLIDER("maxheight", m_height.max, 500);
	SET_SLIDER("minslope", m_slope.min, 1);
	SET_SLIDER("maxslope", m_slope.max, 1);
	SET_SLIDER("minscale", m_scale.min, 10);
	SET_SLIDER("maxscale", m_scale.max, 10);
	SET_SLIDER("minalign", m_angle.min, 1);
	SET_SLIDER("maxalign", m_angle.max, 1);
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
void FoliageLayerEditor::setDensity(gui::Scrollbar*, int v) { m_density=powf(v*0.001,2)*m_densityMax; refresh(); }
void FoliageLayerEditor::setRange(gui::Scrollbar*, int v)   { m_range=v; m_layer->setViewRange(m_range); }

void FoliageLayerEditor::setMinHeight(gui::Scrollbar*, int v) { m_height.min = v*0.5; refresh(); }
void FoliageLayerEditor::setMaxHeight(gui::Scrollbar*, int v) { m_height.max = v*0.5; refresh(); }
void FoliageLayerEditor::setMinSlope(gui::Scrollbar*, int v)  { m_slope.min = v*0.001; refresh(); }
void FoliageLayerEditor::setMaxSlope(gui::Scrollbar*, int v)  { m_slope.max = v*0.001; refresh(); }

void FoliageLayerEditor::setMinScale(gui::Scrollbar*, int v)  { m_scale.min = v*0.01; refresh(); }
void FoliageLayerEditor::setMaxScale(gui::Scrollbar*, int v)  { m_scale.max = v*0.01; refresh(); }

void FoliageLayerEditor::setMinAngle(gui::Scrollbar*, int v)  { m_angle.min = v*0.01; refresh(); }
void FoliageLayerEditor::setMaxAngle(gui::Scrollbar*, int v)  { m_angle.max = v*0.01; refresh(); }

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
	Combobox* list = m_panel->getWidget<Combobox>("mesh");
	int index = getListIndex(m_editor->m_meshList, name);
	if(index<0) {
		FoliageMesh mesh { file, 0,0,0,0 };
		mesh.material = m_editor->createMaterial(m_type, "./maps/white.png");
		base::Model* model = base::BMLoader::load(file);
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
	m_align = i;
	m_panel->getWidget<Widget>("alignrange")->setVisible(m_align>1);
	refresh();
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
	const char* name = strrchr(file, '/');
	if(name) ++name; else name=file;
	Combobox* list = m_panel->getWidget<Combobox>("sprite");
	int index = getListIndex(m_editor->m_spriteList, name);
	if(index<0) {
		FoliageSprite sprite;
		sprite.file = m_editor->m_fileSystem->getRelative(file);
		sprite.material = m_editor->createMaterial(m_type, m_editor->m_fileSystem->getFile(file));
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
		FoliageInstanceLayer* inst = static_cast<FoliageInstanceLayer*>(m_layer);
		inst->setAlignment((FoliageInstanceLayer::OrientaionMode)m_align, m_angle);
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

XMLElement FoliageEditor::save(const TerrainMap* context) const {
	XMLElement e("foliage");
	for(uint i=0; i<m_layerList->getItemCount(); ++i) {
		const FoliageLayerEditor* layer = m_layerList->getItemData(i).getValue<FoliageLayerEditor*>(0);
		e.add(layer->save());
	}
	return e;
}
XMLElement FoliageLayerEditor::save() const {
	const char* types[] = { "instanced", "grass" };
	XMLElement e("layer");
	e.setAttribute("type", types[(int)m_type]);
	e.setAttribute("name", m_name);
	e.setAttribute("range", m_range);
	e.setAttribute("density", m_density);
	saveRange(e, "height", m_height);
	saveRange(e, "slope", m_slope);
	saveRange(e, "scale", m_scale);

	if(m_type==FoliageType::Grass) {
		Combobox* list = m_panel->getWidget<Combobox>("sprite");
		FoliageSprite* sprite = list->getSelectedData().cast<FoliageSprite>();
		if(sprite) {
			XMLElement& f = e.add("sprite");
			f.setAttribute("file", m_editor->m_fileSystem->getRelative(sprite->file));
		}
	}
	if(m_type==FoliageType::Instanced) {
		Combobox* list = m_panel->getWidget<Combobox>("mesh");
		FoliageMesh* mesh = list->getSelectedData().cast<FoliageMesh>();
		if(mesh) {
			XMLElement& f = e.add("mesh");
			f.setAttribute("file", m_editor->m_fileSystem->getRelative(mesh->file));
		}
		e.add("align").setAttribute("mode", m_align);
		if(m_align > 1) saveRange(e, "angle", m_angle);
	}
	return e;
}

void FoliageEditor::load(const XMLElement& e, const TerrainMap* context) {
	clear();
	const XMLElement& list = e.find("foliage");
	for(const XMLElement& layerData: list) {
		const char* typeName = layerData.attribute("type");
		FoliageType type;
		if(strcmp(typeName, "instanced")==0) type = FoliageType::Instanced;
		else if(strcmp(typeName, "grass")==0) type = FoliageType::Grass;
		else continue;	// Invalid type

		FoliageLayerEditor* editor = addLayer(type);
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

	
	if(m_type==FoliageType::Grass) {
		const XMLElement& f = e.find("sprite");
		const char* file = f.attribute("file");
		if(file[0]) loadSpriteFile(file);
	}
	if(m_type==FoliageType::Instanced) {
		const XMLElement& m = e.find("mesh");
		const char* file = m.attribute("file");
		if(file[0]) loadMeshFile( m_editor->m_fileSystem->getFile(file) );

		const XMLElement& align = e.find("align");
		const char* mode = align.attribute("mode");
		if(mode) m_align = atoi(mode);
		m_panel->getWidget<Combobox>("alignment")->selectItem(m_align);
		m_panel->getWidget<Widget>("alignrange")->setVisible(m_align>1);
		loadRange(e.find("angle"), m_angle);
	}

	updateSliders();
	refresh();
	if(eventRenamed) eventRenamed(this);
}



/*
 * How to handle multiple tiles?
 * Either a FoliageSystem per tile, or a shared one. (shared probably better)
 * 	- FoliageSystem::getActive() needs to skip onloaded parts.
 * 	- ChunkSize must be a multiple of mapSize
 * 	- Need multiple FoliageLayers for each layer if maps are used unless a FoliageLayer can have multiple maps
 */


