#include "polygoneditor.h"
#include "heightmap.h"
#include "gui/widgets.h"
#include "gui/lists.h"
#include "scene/scene.h"
#include <base/xml.h>
#include <base/collision.h>

using base::XMLElement;
using namespace gui;

extern gui::String appPath;

#define CONNECT(Type, name, event, callback) { Type* t=m_panel->getWidget<Type>(name); if(t) t->event.bind(this, &PolygonEditor::callback); else printf("Missing widget: %s\n", name); }

PolygonEditor::PolygonEditor(gui::Root* gui, FileSystem*, MapGrid* terrain, scene::SceneNode* scene)
	: m_terrain(terrain), m_selected(0), m_dragging(DragMode::NONE), m_vertex(0)
{
	m_panel = gui->getWidget("polygoneditor");
	if(!m_panel) gui->load(appPath + "data/polygons.xml");
	m_panel = gui->getWidget("polygoneditor");
	m_panel->setVisible(false);

	m_node = scene->createChild("Polygons");
	m_list = m_panel->getWidget<Listbox>("polygonlist");
	m_properties = m_panel->getWidget<Widget>("properties");
	m_propertyTemplate = m_properties->getWidget<Widget>("property");
	m_propertyTemplate->removeFromParent();
	polygonSelected(m_list,-1);

	m_toolButton = gui->createWidget<Button>("iconbuttondark");
	m_toolButton->setIcon("Polygon");
	m_toolButton->eventPressed.bind(this, &PolygonEditor::toggleEditor);

	CONNECT(Button,  "add", eventPressed, addPolygon);
	CONNECT(Button,  "remove", eventPressed, removePolygon);
	CONNECT(Button,  "addproperty", eventPressed, addProperty);
	CONNECT(Listbox, "polygonlist", eventSelected, polygonSelected);
	CONNECT(Spinbox, "flags", eventChanged, changeFlags);
}

PolygonEditor::~PolygonEditor() {
	polygonSelected(m_list, -1);
	m_properties->add(m_propertyTemplate);
	m_toolButton->removeFromParent();
	delete m_toolButton;
}

void PolygonEditor::setup(gui::Widget* toolPanel) {
	toolPanel->add(m_toolButton, 1);
}

void PolygonEditor::close() {
	m_toolButton->setSelected(false);
	m_panel->setVisible(false);
}

void PolygonEditor::toggleEditor(gui::Button*) {
	m_panel->setVisible(!m_panel->isVisible());
	m_toolButton->setSelected(m_panel->isVisible());
}
// ------------------------------------------------------------------------------ //

void PolygonEditor::addPolygon(Button*) {
	Polygon* p = new Polygon();
	p->drawable = 0;
	p->properties.push_back( ItemProperty{"name", "Polygon"} );
	p->points.push_back( m_cameraPosition + vec3(-5,0,-5) );
	p->points.push_back( m_cameraPosition + vec3(5,0,-5) );
	p->points.push_back( m_cameraPosition + vec3(5,0,5) );
	p->points.push_back( m_cameraPosition + vec3(-5,0,5) );
	for(vec3& v: p->points) p->edges.push_back(0), v.y = m_terrain->getHeight(v);
	int index = m_list->getItemCount();
	m_list->addItem("Polygon", p);
	m_list->selectItem(index);
	polygonSelected(m_list, index);
	updateDrawable(p);
	m_dragging = INITIAL;
	m_selected = p;
	m_vertex = 0;
	m_offset = vec3(5,0,5);
}

void PolygonEditor::removePolygon(Button*) {
	if(m_selected) {
		destroyDrawable(m_selected);
		delete m_selected;
		m_selected = 0;
		int index = m_list->getSelectedIndex();
		m_list->removeItem(index);
		if(index==(int)m_list->getItemCount()) --index;
		m_list->selectItem(index);
		polygonSelected(m_list, index);
	}
}

void PolygonEditor::polygonSelected(Listbox* list, int index) {
	m_properties->setVisible(index>=0);
	m_panel->getWidget("flags")->setVisible(index>=0);
	if(index>=0) {
		list->getItemData(index).read(m_selected);
		int size = m_selected->properties.size();
		while(m_properties->getWidgetCount() < size+1) addPropertyWidget();
		while(m_properties->getWidgetCount() > size+1) {
			Widget* w = m_properties->getWidget(0);
			w->removeFromParent();
			delete w;
		}
		for(int i=0; i<size; ++i) {
			Textbox* key = m_properties->getWidget(i)->getWidget<Textbox>("key");
			Textbox* value = m_properties->getWidget(i)->getWidget<Textbox>("value");
			key->setText( m_selected->properties[i].key );
			value->setText( m_selected->properties[i].value );
		}
	}
	else {
		m_selected = 0;
	}
}

// ------------------------------------------------------------------------------ //

void PolygonEditor::addPropertyWidget() {
	Widget* prop = m_propertyTemplate->clone();
	prop->getWidget<Button>("remove")->eventPressed.bind(this, &PolygonEditor::removePropery);
	prop->getWidget<Textbox>("value")->eventChanged.bind(this, &PolygonEditor::changeProperty);
	prop->getWidget<Textbox>("key")->eventChanged.bind(this, &PolygonEditor::changeProperty);
	m_properties->add(prop, m_properties->getWidgetCount()-1);
}

void PolygonEditor::addProperty(Button*) {
	m_selected->properties.push_back( ItemProperty() );
	addPropertyWidget();
}
void PolygonEditor::removePropery(Button* b) {
	int index = b->getParent()->getIndex();
	m_selected->properties.erase(m_selected->properties.begin()+index);
	m_properties->remove(b->getParent());
	delete b->getParent();
}
void PolygonEditor::changeProperty(Textbox* text, const char* value) {
	int index = text->getParent()->getIndex();
	ItemProperty& prop = m_selected->properties[index];
	if(text->getIndex()==0) prop.key = value;
	else prop.value = value;
	// Changed name
	if(prop.key=="name" || prop.key == "Name") {
		m_list->setItemName(m_list->getSelectedIndex(), value);
	}
}

void PolygonEditor::changeFlags(gui::Spinbox*, int value) {
	if(m_selected && m_selected->edges[m_vertex] != value) {
		m_selected->edges[m_vertex] = value;
		updateDrawable(m_selected);
	}
}

// ------------------------------------------------------------------------------ //

void PolygonEditor::load(const XMLElement& e, const TerrainMap* context) {
	const XMLElement& list = e.find("polygons");
	for(const XMLElement& i: list) {
		Polygon* p = new Polygon();
		int size = i.attribute("size", 0);
		for(auto a=i.attributesBegin(); a!=i.attributesEnd(); ++a) {
			if(strcmp(a->value,"size")==0) continue;
			p->properties.push_back( ItemProperty{a->key, (const char*)a->value});
		}
		// Parse values - assume well formed TODO
		const char* s = i.child(0).text();
		float x=0, y=0; int flags=0;
		for(int j=0; j<size; ++j) {
			char* e;
			x = strtod(s, &e); s=e;
			y = strtod(s, &e); s=e;
			flags = strtol(s, &e, 10); s=e;


			p->points.push_back(vec3(x, 0, y));
			p->edges.push_back(flags);
		}
		for(vec3& v: p->points) p->edges.push_back(0), v.y = m_terrain->getHeight(v);
		m_list->addItem(i.attribute("name"), p);
		updateDrawable(p);
	}
}

XMLElement PolygonEditor::save(const TerrainMap* context) const {
	if(m_list->getItemCount()==0) return XMLElement();
	XMLElement xml("polygons");
	char buffer[2048];
	for(uint i=0; i<m_list->getItemCount(); ++i) {
		XMLElement& e = xml.add("polygon");
		Polygon* poly = *m_list->getItemData(i).cast<Polygon*>();
		for(const ItemProperty& p: poly->properties) e.setAttribute(p.key, p.value);
		char* c = buffer;
		int size = poly->points.size();
		e.setAttribute("size", size);
		for(int j=0; j<size; ++j) c += sprintf(c, "%g %g %d ", poly->points[j].x, poly->points[j].z, poly->edges[j]);
		if(c>buffer) {
			c[-1]=0;
			XMLElement& pts = e.add("points");
			pts.setText(buffer);
		}
	}
	return xml;
}

void PolygonEditor::clear() {
}

// ------------------------------------------------------------------------------ //

#include <base/game.h>
#include <base/input.h>

void PolygonEditor::update(const Mouse& mouse, const Ray& ray, base::Camera*, InputState& state) {
	m_cameraPosition = ray.start;

	// Detect mouse over
	if(m_dragging==NONE && mouse.pressed==1 && !state.consumedMouseDown) {
		float best = 4;
		vec3 end = ray.point(1000);
		float s, t;
		Polygon* target = 0;
		for(uint k=0; k<m_list->getItemCount(); ++k) {
			Polygon* polygon = *m_list->getItemData(k).cast<Polygon*>();
			int size = polygon->points.size();
			for(int i=size-1, j=0; i<size; i=j++) {
				float r = base::closestPointBetweenLines(ray.start, end, polygon->points[i], polygon->points[j], s, t);
				if(r<best) {
					target = polygon;
					m_vertex = i;
					// Detect mode based on t
					float dist = polygon->points[i].distance(polygon->points[j]);
					if(dist*t < 1 && t < 0.5) m_dragging = VERTEX;
					else if(dist*(1-t)< 1 && t>0.5) m_dragging = VERTEX, m_vertex = j;
					else if(state.keyMask&CTRL_MASK) m_dragging = ALL;
					else m_dragging = EDGE;
					m_offset = ray.point(s * 1000) - polygon->points[m_vertex];
					best = r;
				}
			}
		}
		// Split edge
		if(m_selected && m_dragging==EDGE && !(state.keyMask&SHIFT_MASK)) {
			m_selected->points.insert(m_selected->points.begin()+m_vertex, m_selected->points[m_vertex]);
			m_selected->edges.insert(m_selected->edges.begin()+m_vertex, m_selected->edges[m_vertex]);
			++m_vertex;
			m_selected->points[m_vertex] += m_offset;
			m_offset.set(0,0,0);
			m_dragging = VERTEX;
		}
		// Selection changed
		if(target && target !=m_selected) {
			for(uint i=0; i<m_list->getItemCount(); ++i) {
				if(m_list->getItemData(i) == target) {
					m_list->selectItem(i);
					polygonSelected(m_list, i);
				}
			}
		}
		// Flags
		if(target) m_panel->getWidget<Spinbox>("flags")->setValue(m_selected->edges[m_vertex]);
		if(target) state.consumedMouseDown = true;
	}
	// Move selected vertex
	else if(m_dragging!=NONE && m_selected && (mouse.button==1 || m_dragging==INITIAL)) {
		float t;
		if(!m_terrain->castRay(ray.start, ray.direction, t)) t = -ray.start.y / ray.direction.y; // zero plane
		vec3 p = ray.point(t) - m_offset;
		std::vector<vec3>& poly = m_selected->points;
		if(m_dragging==VERTEX) {
			for(size_t i=0; i<poly.size(); ++i) if((int)i!=m_vertex && poly[i].distance2(p)<1) p = poly[i];
			p.y = m_terrain->getHeight(p);
			poly[m_vertex] = p;
		}
		else if(m_dragging==EDGE) {
			int next = (m_vertex + 1) % poly.size();
			vec3 other = poly[next] - poly[m_vertex];
			poly[m_vertex] = p;
			poly[next] = p + other;
			poly[m_vertex].y = m_terrain->getHeight(poly[m_vertex]);
			poly[next].y = m_terrain->getHeight(poly[next]);
		}
		else if(m_dragging==ALL || m_dragging==INITIAL) {
			vec3 start = poly[m_vertex];
			for(vec3& pt: poly) {
				pt = p + pt - start;
				pt.y = m_terrain->getHeight(pt);
			}
		}
		updateDrawable(m_selected);
	}
	if(mouse.released==1 && m_dragging!=NONE) {
		m_dragging = NONE;
		for(size_t i=1; i<m_selected->points.size(); ++i) {
			if(m_selected->points[i-1] == m_selected->points[i]) {
				m_selected->points.erase(m_selected->points.begin()+i);
				m_selected->edges.erase(m_selected->edges.begin()+i);
				--i;
			}
		}
		if(m_selected->points.size()<2) removePolygon(0);
	}
	if(mouse.pressed&4 && m_dragging == INITIAL) {
		removePolygon(0);
		m_dragging = NONE;
	}
}


#include "model/mesh.h"
#include "model/hardwarebuffer.h"
#include "scene/mesh.h"
#include "scene/material.h"
#include "scene/shader.h"
#include "scene/autovariables.h"

static const char* polygonVS = 
"#version 150\nin vec4 vertex;\nin vec4 colour;\nuniform mat4 transform;\nout vec4 col;\nvoid main() { gl_Position=transform*vertex; col=colour; }";
static const char* polygonFS = 
"#version 150;\nin vec4 col;\nout vec4 fragment;\nvoid main() { fragment=col; }";

void PolygonEditor::updateDrawable(Polygon* poly) {
	// Create drawable
	if(!poly->drawable) {
		base::bmodel::Mesh* mesh = new base::bmodel::Mesh();
		mesh->setPolygonMode(base::bmodel::TRIANGLE_STRIP);
		base::HardwareVertexBuffer* buffer = new base::HardwareVertexBuffer();
		buffer->attributes.add(base::VA_VERTEX, base::VA_FLOAT3);
		buffer->attributes.add(base::VA_COLOUR, base::VA_ARGB);
		buffer->createBuffer();
		mesh->setVertexBuffer(buffer);
		poly->drawable = new scene::DrawableMesh(mesh);
		m_node->attach(poly->drawable);

		// Create material
		static scene::Material* material = 0;
		if(!material) {
			scene::ShaderPart* vs = new scene::ShaderPart(scene::VERTEX_SHADER, polygonVS);
			scene::ShaderPart* fs = new scene::ShaderPart(scene::FRAGMENT_SHADER, polygonFS);
			scene::Shader* shader = new scene::Shader();
			shader->attach(vs);
			shader->attach(fs);
			shader->bindAttributeLocation("vertex", 0);
			shader->bindAttributeLocation("colour", 4);
			material = new scene::Material();
			scene::Pass* pass = material->addPass("default");
			pass->getParameters().setAuto("transform", scene::AUTO_MODEL_VIEW_PROJECTION_MATRIX);
			pass->setShader(shader);
			pass->state.cullMode = scene::CULL_NONE;
			pass->compile();
		}
		poly->drawable->setMaterial(material);
	}
	
	// Update drawable
	float width = 1;
	uint colours[] = { 0xffffff, 0xff0000, 0x00ff00, 0x0000ff };
	struct Vertex { float x,y,z; uint colour; };
	std::vector<Vertex> data;
	for(size_t i=0; i<poly->points.size(); ++i) {
		uint colour = colours[poly->edges[i] % 4];
		const vec3& p = poly->points[i];
		data.push_back( Vertex{p.x, p.y+width, p.z, colour} );
		data.push_back( Vertex{p.x, p.y-width, p.z, colour} );
		int next = (i+1) % poly->points.size();
		if(poly->edges[next]!=poly->edges[i] || next==0) {
			const vec3& p = poly->points[next];
			data.push_back( Vertex{p.x, p.y+width, p.z, colour} );
			data.push_back( Vertex{p.x, p.y-width, p.z, colour} );
		}
	}

	base::HardwareVertexBuffer* buffer = poly->drawable->getMesh()->getVertexBuffer();
	buffer->copyData(&data[0], data.size(), sizeof(Vertex));
}
void PolygonEditor::destroyDrawable(Polygon* poly) {
	if(poly->drawable) {
		m_node->detach(poly->drawable);
		delete poly->drawable->getMesh();
		delete poly->drawable;
		poly->drawable = 0;
	}
}


