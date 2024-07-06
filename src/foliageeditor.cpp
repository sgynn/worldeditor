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
using namespace base;

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

static const char* shaderSourceVS = R"-(#version 330
in vec4 vertex;
in vec3 normal;
in vec2 texCoord;
uniform mat4 transform;
uniform mat4 modelMatrix;
out vec2 texcoord;
out vec3 worldNormal;
out vec3 worldPos;
void main() {
	gl_Position = transform*vertex;
	texcoord = texCoord;
	worldNormal = mat3(modelMatrix) * normal;
	worldPos = (modelMatrix * vertex).xyz;
})-";

static const char* shaderSourceInstVS = R"-(#version 330
in vec4 vertex;
in vec3 normal;
in vec2 texCoord;
in vec4 loc;
in vec4 rot;
uniform mat4 transform;
uniform mat4 modelMatrix;
out vec2 texcoord;
out vec3 worldNormal;
out vec3 worldPos;
mat3 quatToMat(vec4 q) {
	vec3 x = vec3( 1-2*(q.y*q.y + q.z*q.z), 2*(q.x*q.y + q.w*q.z), 2*(q.x*q.z - q.w*q.y) );
	vec3 y = vec3( 2*(q.x*q.y - q.w*q.z), 1-2*(q.x*q.x + q.z*q.z), 2*(q.y*q.z + q.w*q.x) );
	vec3 z = cross(x, y);
	return mat3(x, y, z);
}
void main() {
	mat3 m = quatToMat(rot);
	vec3 pos = m * vertex.xyz * loc.w + loc.xyz;
	gl_Position = transform * vec4(pos, 1.0);
	texcoord = texCoord;
	worldNormal = mat3(modelMatrix) * m * normal;
	worldPos = (modelMatrix * vec4(pos, 1.0)).xyz;
})-";

static const char* shaderSourceFS = R"-(#version 330
in vec2 texcoord;
in vec3 worldNormal;
in vec3 worldPos;
out vec4 fragment;
uniform sampler2D diffuse;
uniform vec3 lightDirection;
void main() {
	vec4 diff = texture(diffuse, texcoord.st);
	if(diff.a < 0.5) discard;
	float l = dot(normalize(worldNormal), normalize(lightDirection));
	float s = (l+1.0) / 1.3 * 0.2 + 0.1;
	fragment = vec4(diff.rgb * max(s,l), 1.0);
})-";


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
		base::Image img = base::PNG::load(diffuse);
		if(img) {
			base::Texture tex = base::Texture::create(img.getWidth(), img.getHeight(), img.getFormat(), img.getData());
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

FoliageEditor:: FoliageEditor(Root* gui, FileSystem* fs, MapGrid* terrain, base::SceneNode* node) : m_foliage(0), m_fileSystem(fs), m_terrain(terrain), m_node(node) {
	setupGui(gui);
}
FoliageEditor::~FoliageEditor() {
	clear();
	delete m_foliage;
	delete m_meshList;
	delete m_spriteList;
}

void FoliageEditor::setupGui(Root* gui) {
	createPanel(gui, "foliage", "foliage.xml");
	createToolButton(gui, "Foliage");

	m_layerList = gui->getWidget<Listbox>("foliagelist");
	m_fileDialog = gui->getWidget<FileDialog>("filedialog");

	m_layerList->eventSelected.bind(this, &FoliageEditor::layerSelected);
	m_layerList->eventCustom.bind(this, &FoliageEditor::changeVisible);

	gui->getWidget<Combobox>("addfoliage")->eventSelected.bind(this, &FoliageEditor::addLayer);
	m_removeButton = gui->getWidget<Button>("removefoliage");
	m_removeButton->eventPressed.bind(this, &FoliageEditor::removeLayer);
	m_cloneButton = gui->getWidget<Button>("clonefoliage");
	m_cloneButton->eventPressed.bind(this, &FoliageEditor::duplicateLayer);

	m_meshList = new ItemList;
	m_spriteList = new ItemList;
}

void FoliageEditor::close() {
	for(const ListItem& item: m_layerList->items()) {
		FoliageEditor* editor = item.findValue<FoliageEditor*>();
		if(editor) editor->getPanel()->setVisible(false);
	}
}

void FoliageEditor::update(const Mouse&, const Ray& ray, base::Camera*, InputState&) {
	if(m_foliage) m_foliage->update(ray.start);
}

void FoliageEditor::addLayer(Combobox* c, ListItem& type) {
	FoliageLayerEditor* editor = addLayer((FoliageType)type.getIndex());
	showEditor(editor);
	c->clearSelection();
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
	m_layerList->addItem(editor->getName(), editor, type==FoliageType::Instanced? 16: 18, true);
	m_foliage->addLayer(layer);
	editor->eventRenamed.bind(this, &FoliageEditor::layerRenamed);
	return editor;
}

void FoliageEditor::removeLayer(Button*) {
	if(const ListItem* item = m_layerList->getSelectedItem()) {
		FoliageLayerEditor* editor = item->findValue<FoliageLayerEditor*>();
		m_layerList->removeItem(item->getIndex());
		destroy(editor);
	}
}
void FoliageEditor::duplicateLayer(Button*) {
	if(const ListItem* item = m_layerList->getSelectedItem()) {
		FoliageLayerEditor* from = item->findValue<FoliageLayerEditor*>();
		FoliageLayerEditor* to = addLayer(from->getType());
		to->load(from->save()); // meh ?
		showEditor(to);
	}
}

void FoliageEditor::layerSelected(Listbox* list, ListItem& item) {
	m_removeButton->setEnabled(item.isValid());
	m_cloneButton->setEnabled(item.isValid());
	if(FoliageLayerEditor* editor = item.findValue<FoliageLayerEditor*>()) {
		showEditor(editor);
	}
}

void FoliageEditor::layerRenamed(FoliageLayerEditor* e) {
	for(ListItem& item: m_layerList->items()) {
		if(item.findValue<FoliageLayerEditor*>() == e) {
			item.setValue(e->getName());
			break;
		}
	}
}

void FoliageEditor::changeVisible(Listbox*, ListItem& item, Widget*) {
	if(FoliageLayerEditor* editor = item.findValue<FoliageLayerEditor*>()) {
		editor->setVisible(item.getValue(3, true));
	}
}

void FoliageEditor::showEditor(FoliageLayerEditor* layer) {
	layer->getPanel()->setVisible(true);
	layer->getPanel()->raise();
	
	// Don't perfectly overlap them
	for(ListItem& item: m_layerList->items()) {
		FoliageLayerEditor* editor = item.findValue<FoliageLayerEditor*>();
		if(editor != layer && editor->getPanel()->isVisible() && editor->getPanel()->getPosition() == layer->getPanel()->getPosition()) {
			layer->getPanel()->setPosition(layer->getPanel()->getPosition() + Point(32, 32));
		}
	}
}

void FoliageEditor::clear() {
	for(ListItem& item: m_layerList->items()) {
		destroy(item.findValue<FoliageLayerEditor*>());
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
#define CONNECTL(Type, name, event, callback) { Type* t=w->getWidget<Type>(name); if(t) t->event.bind(callback); else printf("Missing widget: %s\n", name); }
FoliageLayerEditor::FoliageLayerEditor(FoliageEditor* editor, Widget* w, FoliageLayer* layer, FoliageType type) : m_editor(editor), m_layer(layer), m_type(type), m_panel(w) {
	CONNECT(Textbox, "name", eventSubmit, renameLayer);
	CONNECT(Combobox, "densitymap", eventSelected, setDensityMap);
	CONNECTL(Combobox, "distribution", eventSelected, [this](Combobox*, ListItem& i) {
		m_distribution = i.getIndex();
		m_panel->getWidget("cluster")->setVisible(m_distribution);
		int h = 0;
		for(Widget* w: m_panel) if(w->isVisible()) h = std::max(h, w->getRect().bottom());
		h += m_panel->getSize().y - m_panel->getClientRect().height;
		m_panel->setSize(m_panel->getSize().x, h);
		refresh();
	});

	auto setupSlider = [this](float& value, const char* name, float max, bool regen=true) {
		Slider slider { m_panel->getWidget<Scrollbar>(name), value, max };
		if(slider.slider) {
			slider.slider->setRange(0, 1000);
			slider.slider->eventChanged.bind([this, &value, max, regen](Scrollbar*, int v) {
				value = (float)v * max / 1000.f;
				refresh(regen);
			});
			m_sliders.push_back(slider);
		}
		else printf("Error: Missing widget %s\n", name);
	};

	setupSlider(m_range,      "range",    1000, false);
	setupSlider(m_density,    "density", type==FoliageType::Instanced? 0.5: 16);
	setupSlider(m_height.min, "minheight", 500);
	setupSlider(m_height.max, "maxheight", 500);
	setupSlider(m_slope.min,  "minslope", 1);
	setupSlider(m_slope.max,  "maxslope", 1);
	setupSlider(m_scale.min,  "minscale", 10);
	setupSlider(m_scale.max,  "maxscale", 10);
	setupSlider(m_angle.min,  "minalign", 1);
	setupSlider(m_angle.max,  "maxalign", 1);


	setupSlider(m_clusterDensity, "clusterdensity", 0.05);
	setupSlider(m_clusterRadius.min, "clustermin", 100);
	setupSlider(m_clusterRadius.max, "clustermax", 100);
	setupSlider(m_clusterShapeScale, "clustershapescale", 1);
	setupSlider(m_clusterFalloff,    "clusterfalloff", 1);

	CONNECTL(Scrollbar, "clustershape", eventChanged, [this](Scrollbar*, int v) { m_clusterShapePoints=v; refresh(); });
	m_panel->getWidget<Scrollbar>("clustershape")->setRange(0, 10);

	Combobox* align = 0;
	w->getWidget<Widget>("alignrange")->setVisible(false);

	switch(type) {
	case FoliageType::Instanced:
		CONNECT(Combobox,  "mesh",      eventSelected, setMesh);
		CONNECT(Button,    "browsemesh", eventPressed, loadMesh);
		CONNECT(Combobox,  "alignment",  eventSelected, setAlignment);
		w->getWidget<Widget>("grass")->setVisible(false);
		w->getWidget<Combobox>("mesh")->shareList(editor->m_meshList);
		m_name = "New Instanced Layer";
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
		m_density = 1;
		break;
	}

	w->getWidget<Combobox>("distribution")->selectItem(0, true);

	// Defaults
	m_range = 100;
	m_scale.set(1,1);
	m_slope.set(0,1);
	m_angle.set(0,0);
	m_height.set(0,1000);
	m_align = 0;
	updateSliders();

	// Icon
	gui::Image* icon = w->getWidget<gui::Image>("icon");
	icon->setImage(type==FoliageType::Instanced? "Foliage": "Grass");
}

void FoliageLayerEditor::updateSliders() {
	m_panel->getWidget<Textbox>("name")->setText(m_name);
	for(Slider& s: m_sliders) {
		s.slider->setValue(s.value * 1000 / s.max);
	}
}

void FoliageLayerEditor::setVisible(bool vis) {
	((base::SceneNode*)m_layer)->setVisible(vis);
}

void FoliageLayerEditor::renameLayer(Textbox* t) {
	m_name = t->getText();
	if(eventRenamed) eventRenamed(this);
}

void FoliageLayerEditor::setDensityMap(Combobox*, ListItem&) {}

// -------------------------------------------------------------------------------------- //
void FoliageLayerEditor::loadMesh(Button*) {
	m_editor->m_fileDialog->eventConfirm.bind(this, &FoliageLayerEditor::loadMeshFile);
	m_editor->m_fileDialog->setFilter("*.bm");
	m_editor->m_fileDialog->setFileName("");
	m_editor->m_fileDialog->showOpen();
}
void FoliageLayerEditor::loadMeshFile(const char* file) {
	m_file = file;
	const char* name = strrchr(file, '/') + 1;
	Combobox* list = m_panel->getWidget<Combobox>("mesh");
	ListItem* existing = list->findItem([name](ListItem& i){return strcmp(i.getText(), name)==0; });
	if(existing) list->selectItem(existing->getIndex(), true);
	else {
		FoliageMesh mesh { file, 0,0,0,0 };
		mesh.material = m_editor->createMaterial(m_type, "./maps/white.png");
		base::Model* model = base::BMLoader::load(file);
		if(model && model->getMeshCount()) {
			mesh.mesh = model->getMesh(0);
			mesh.mesh->getVertexBuffer()->createBuffer();
			mesh.mesh->getIndexBuffer()->createBuffer();
		}
		list->addItem(name, mesh);
		list->selectItem(list->getItemCount()-1, true);
	}
}
void FoliageLayerEditor::setMesh(Combobox* list, ListItem& item) {
	const FoliageMesh& mesh = item.getValue<FoliageMesh>(1, FoliageMesh());
	static_cast<FoliageInstanceLayer*>(m_layer)->setMesh(mesh.mesh);
	m_layer->setMaterial(mesh.material);
	refresh();
}
void FoliageLayerEditor::setAlignment(Combobox*, ListItem& value) {
	m_align = value.getIndex();
	m_panel->getWidget<Widget>("alignrange")->setVisible(m_align>1);
	refresh();
}

// -------------------------------------------------------------------------------------- //
void FoliageLayerEditor::loadSprite(Button*) {
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
	ListItem* existing = list->findItem([name](ListItem& i){return strcmp(i.getText(), name)==0; });
	if(existing) list->selectItem(existing->getIndex());
	else {
		FoliageSprite sprite;
		sprite.file = m_editor->m_fileSystem->getRelative(file);
		sprite.material = m_editor->createMaterial(m_type, m_editor->m_fileSystem->getFile(file));
		list->addItem(name, sprite);
		list->selectItem(list->getItemCount()-1, true);
	}
}
void FoliageLayerEditor::setSprite(Combobox* list, ListItem& item) {
	const FoliageSprite& sprite = item.getValue<FoliageSprite>(1, FoliageSprite());
	m_layer->setMaterial(sprite.material);
	refresh();
}
void FoliageLayerEditor::setScaleMap(Combobox*, ListItem&) {}
// -------------------------------------------------------------------------------------- //

void FoliageLayerEditor::refresh(bool regen) {
	m_layer->setViewRange(m_range);
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
	if(m_distribution == 0) m_layer->setCluster(0, 0, 0);
	else {
		m_layer->setCluster(0, fmax(1e-6,m_clusterDensity), {m_clusterRadius.min, m_clusterRadius.max}, m_clusterFalloff);
		m_layer->setClusterGap({m_clusterGap.min, m_clusterGap.max});
		m_layer->setClusterShape(m_clusterShapePoints, m_clusterShapeScale);
	}

	if(regen) m_layer->regenerate();
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
	if(context || m_layerList->getItemCount()==0) return XMLElement();
	XMLElement e("foliage");
	for(const ListItem& item: m_layerList->items()) {
		const FoliageLayerEditor* layer = item.getValue<FoliageLayerEditor*>(1, nullptr);
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

	if(m_distribution == 1) {
		e.setAttribute("clusters", m_clusterDensity);
		e.setAttribute("clusterfalloff", m_clusterFalloff);
		saveRange(e, "radius", m_clusterRadius);
		if(m_clusterGap.max>0) saveRange(e, "gap", m_clusterGap);
		if(m_clusterShapePoints) e.setAttribute("shape", m_clusterShapePoints);
		if(m_clusterShapeScale) e.setAttribute("shapescale", m_clusterShapeScale);
	}


	if(!((base::SceneNode*)m_layer)->isVisible()) e.setAttribute("enabled", false);

	if(m_type==FoliageType::Grass) {
		Combobox* list = m_panel->getWidget<Combobox>("sprite");
		FoliageSprite* sprite = list->getSelectedItem()->getData(1).cast<FoliageSprite>();
		if(sprite) {
			XMLElement& f = e.add("sprite");
			f.setAttribute("file", m_editor->m_fileSystem->getRelative(sprite->file));
		}
	}
	if(m_type==FoliageType::Instanced) {
		Combobox* list = m_panel->getWidget<Combobox>("mesh");
		FoliageMesh* mesh = list->getSelectedItem()->getData(1).cast<FoliageMesh>();
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
	if(context) return;
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

		// Initial being enabled
		if(!layerData.attribute("enabled", true)) {
			m_layerList->getItem(m_layerList->getItemCount()-1).setValue(3, false);
		}
	}
	m_layerList->refresh();
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

	if(!e.attribute("enabled", true)) setVisible(false);

	if(e.hasAttribute("clusters")) m_distribution = 1;
	if(m_distribution) {
		m_clusterDensity = e.attribute("clusters", m_clusterDensity);
		m_clusterFalloff = e.attribute("clusterfalloff", m_clusterFalloff);
		loadRange(e.find("radius"), m_clusterRadius);
		loadRange(e.find("gap"), m_clusterGap);
		m_clusterShapePoints = e.attribute("shape", m_clusterShapePoints);
		m_clusterShapeScale = e.attribute("shapescale", m_clusterShapeScale);
	}

	m_panel->getWidget<Combobox>("distribution")->selectItem(m_distribution);
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
 * 	- Option for global or local. Local uses tile clip bounds
 */


