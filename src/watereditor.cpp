#include "watereditor.h"
#include "water/water.h"
#include "heightmap.h"
#include <base/gui/lists.h>
#include <base/debuggeometry.h>
#include <base/drawablemesh.h>
#include <base/collision.h>
#include <base/camera.h>
#include <base/scene.h>
#include <base/input.h>

using namespace gui;
using namespace base;

#define CONNECT(Type, name, event, callback) { Type* t=m_panel->getWidget<Type>(name); if(t) t->event.bind(this, &WaterEditor::callback); else printf("Missing widget: %s\n", name); }

static vec3 calculateBezierPoint(const vec3* control, float t) {
	static const float factorial[] = {1,1,2,6,24,120,720}; // Factorial lookup table
	vec3 p;
	for(int k=0; k<4; ++k) {
		float kt = t>0||k>0? powf(t,k): 1; // t^k
		float rt = t<1||k<3? powf(1-t,3-k): 1; // (1-t)^ik
		float ni = factorial[3] / (factorial[k]*factorial[3-k]);
		float bias = ni * kt * rt;
		p += control[k] * bias;
	}
	return p;
}


WaterEditor::WaterEditor(Root* gui, FileSystem*, MapGrid* map, SceneNode* scene) : m_terrain(map) {
	m_waterSystem = new WaterSystem();

	createPanel(gui, "watereditor", "water.xml");
	createToolButton(gui, "Water");
	m_node = scene->createChild("Water");
	
	m_list = m_panel->getWidget<Listbox>("waterlist");
	CONNECT(Button, "removewater", eventPressed, deleteItem);
	CONNECT(Button, "duplicatewater", eventPressed, duplicateItem);
	CONNECT(Combobox, "addwater", eventSelected, addItem);
	CONNECT(Listbox, "waterlist", eventSelected, selectItem);
}

WaterEditor::~WaterEditor() {
	delete m_waterSystem;
}

void WaterEditor::load(const base::XMLElement&, const TerrainMap* context) {
}
XMLElement WaterEditor::save(const TerrainMap* context) const {
	return XMLElement();
}
void WaterEditor::update(const Mouse& mouse, const Ray& ray, base::Camera* camera, InputState& state) {
	if(!isActive()) return;

	float t;
	Ray cameraRay(camera->getPosition(), -camera->getDirection());
	if(m_terrain->castRay(cameraRay.start, cameraRay.direction, t)) m_centre = cameraRay.point(t);
	
	// Detect mouse over
	if(m_held==0 && !state.consumedMouseDown) {
		WaterSystem::River* river = 0;
		WaterSystem::Lake* lake=0;
		int node = -1, over = 0;
		float closest = 4;
		vec3 closestPoint;
		auto overPoint = [&](const vec3& point) {
			vec3 r = base::closestPointOnLine(point, ray.start, ray.point(1000));
			float d = r.distance2(point);
			if(d<closest) {
				closest = d;
				closestPoint = point;
				return true;
			}
			return false;
		};
		auto overSpline = [&](const WaterSystem::SplineNode& a, const WaterSystem::SplineNode& b, float dist, float& t) {
			const vec3 control[4] = { a.point, a.point+a.direction*a.b, b.point-b.direction*b.a, b.point };
			vec3 last = a.point;
			float u,v;
			bool hit = false;
			for(int i=0; i<=16; ++i) {
				vec3 p = calculateBezierPoint(control, i/16.f);
				float d = base::closestPointBetweenLines(ray.start, ray.point(1000), last, p, u,v);
				if(d<dist && v>=0 && v<=1) {
					dist = d;
					closestPoint = lerp(last, p, v);
					t = (i+v) / 16.f;
					hit = true;
				}
				last = p;
			}
			return hit;
		};
		// Over nodes
		for(WaterSystem::Lake* l : m_waterSystem->lakes()) {
			for(size_t i=0; i<l->nodes.size(); ++i) {
				WaterSystem::SplineNode& n = l->nodes[i];
				if(overPoint(n.point)) { node=i; over=1; lake=l; river=0; }
				if(overPoint(n.point - n.direction*n.a)) { node=i; over=2; lake=l; }
				if(overPoint(n.point + n.direction*n.b)) { node=i; over=3; lake=l; }
			}
		}
		for(WaterSystem::River* r: m_waterSystem->rivers()) {
			for(size_t i=0; i<r->nodes.size(); ++i) {
				WaterSystem::RiverNode& n = r->nodes[i];
				if(overPoint(n.point)) { node=i; over=1; lake=0; river=r; }
				if(overPoint(n.point - n.direction*n.a)) { node=i; over=2; lake=0; river=r; }
				if(overPoint(n.point + n.direction*n.b)) { node=i; over=3; lake=0; river=r; }
			}
		}
		// Over Splines
		if(over==0 || closest > 1) {
			for(WaterSystem::Lake* l : m_waterSystem->lakes()) {
				for(size_t i=l->nodes.size()-1, j=0; j<l->nodes.size(); i=j++) {
					if(overSpline(l->nodes[i], l->nodes[j], 1, t)) { node=i; over=4; lake=l; }
				}
			}
			for(WaterSystem::River* r: m_waterSystem->rivers()) {
				for(size_t i=r->nodes.size()-1, j=0; j<r->nodes.size(); i=j++) {
					if(overSpline(r->nodes[i], r->nodes[j], 1, t)) { node=i; over=4; lake=0; river=r; }
				}
			}
		}

		if(over>0) {
			const uint col[] = { 0xff0000, 0xff8000, 0xff8000, 0x00ff00 };
			static DebugGeometry circle(SDG_ALWAYS);
			circle.circle(closestPoint, vec3(0,1,0), 0.1, 16, col[over-1]);
			if(mouse.pressed==1) {
				m_held = over;
				m_activeNode = node;
				m_lake = lake;
				m_river = river;

				// Add node
				if(over == 4) {
					++node;
					if(lake) {
						lake->nodes.insert(lake->nodes.begin() + node, WaterSystem::SplineNode());
						lake->nodes[node].point = closestPoint;
					}
					if(river) {
						river->nodes.insert(river->nodes.begin() + node, WaterSystem::RiverNode());
						river->nodes[node].point = closestPoint;
					}
					m_held = 1;
					m_activeNode = node;
				}
			}
		}
	}

	if(m_held>0) {
		WaterSystem::SplineNode& node = m_river? m_river->nodes[m_activeNode]: m_lake->nodes[m_activeNode];
		if(base::intersectRayPlane(ray.start, ray.direction, vec3(0,1,0), node.point.y, t)) {
			bool shift = state.keyMask&SHIFT_MASK;
			if(m_held==1 && !shift) node.point = ray.point(t);
			else {
				vec3 n = node.point - ray.point(t);
				float d = n.normaliseWithLength();
				if(m_held==2) {
					node.direction = n;
					node.a = d;
				}
				else {
					node.direction = -n;
					node.b = d;
				}
				if(shift) node.a = node.b = d;
			}
			updateLines();
		}
		if(!mouse.button) m_held = 0;
	}

}

void WaterEditor::clear() {
}
void WaterEditor::close() {
	EditorPlugin::close();
}


void WaterEditor::addItem(Combobox*, int type) {
	deselect();
	if(type==0) {
		m_river = m_waterSystem->addRiver();
		m_list->addItem("River", m_river, 38);
		WaterSystem::RiverNode node;
		node.point = m_centre - vec3(10,0,0);
		node.direction.set(1,0,0);
		m_river->nodes.push_back(node);
		node.point = m_centre + vec3(10,0,0);
		m_river->nodes.push_back(node);
	}
	else {
		m_lake = m_waterSystem->addLake(type==1);
		m_list->addItem(type==1?"Lake":"Ocean", m_river, 38);
		WaterSystem::SplineNode node;
		node.a = node.b = 4;
		node.point = m_centre + vec3(5,0,0);
		node.direction.set(0,0,1);
		m_lake->nodes.push_back(node);
		node.point = m_centre + vec3(0,0,5);
		node.direction.set(-1,0,0);
		m_lake->nodes.push_back(node);
		node.point = m_centre + vec3(-5,0,0);
		node.direction.set(0,0,-1);
		m_lake->nodes.push_back(node);
		node.point = m_centre + vec3(0,0,-5);
		node.direction.set(1,0,0);
		m_lake->nodes.push_back(node);
	}
	updateLines();
}

void WaterEditor::deselect() {
	m_list->clearSelection();
}

void WaterEditor::selectItem(Listbox* list, int index) {
	list->getItemData(index).read(m_river);
	list->getItemData(index).read(m_lake);
}

void WaterEditor::deleteItem(Button*) {
	if(m_river) m_waterSystem->destroyRiver(m_river);
	if(m_lake) m_waterSystem->destroyLake(m_lake);
	m_list->removeItem(m_list->getSelectedIndex());
}

void WaterEditor::duplicateItem(Button*) {
	// Use this to create stuff
		
	BoundingBox bounds = m_terrain->getBounds();
	Material* material = DebugGeometryManager::getDefaultMaterial();

	m_node->deleteAttachments();
	m_node->attach(new DrawableMesh(m_waterSystem->buildGeometry(bounds, 4), material));
	updateLines();
}




void WaterEditor::updateLines() {
	static DebugGeometry g(SDG_MANUAL);
	for(WaterSystem::Lake* l : m_waterSystem->lakes()) {
		// Handles
		for(WaterSystem::SplineNode& n: l->nodes) {
			g.line(n.point - n.direction * n.a, n.point + n.direction * n.b, 0x000000);
		}
		// Splines
		for(size_t i=l->nodes.size()-1, j=0; j<l->nodes.size(); i=j++) {
			const WaterSystem::SplineNode& na = l->nodes[i];
			const WaterSystem::SplineNode& nb = l->nodes[j];
			const vec3 control[4] = { na.point, na.point+na.direction*na.b, nb.point-nb.direction*nb.a, nb.point };
			vec3 last = na.point;
			for(int t = 1; t<16; ++t) {
				vec3 p = calculateBezierPoint(control, t/16.f);
				g.line(last, p, 0xffffffff);
				last = p;
			}
			g.line(last, nb.point, 0xffffffff);
		}
	}
	g.flush();
}

