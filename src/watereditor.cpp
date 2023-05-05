#include "watereditor.h"
#include "water/water.h"
#include "heightmap.h"
#include <base/gui/lists.h>
#include <base/drawablemesh.h>
#include <base/debuggeometry.h>
#include <base/autovariables.h>
#include <base/material.h>
#include <base/texture.h>
#include <base/shader.h>
#include <base/collision.h>
#include <base/camera.h>
#include <base/scene.h>
#include <base/input.h>
#include <base/file.h>

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
	m_node->deleteAttachments();
	m_list->clearItems();
	delete m_waterSystem;
	delete m_node;
}

void WaterEditor::load(const base::XMLElement& root, const TerrainMap* context) {
	if(const XMLElement& water = root.find("water")) {
		for(const XMLElement& e: water) {
			if(e == "river") {
				WaterSystem::River* river = m_waterSystem->addRiver();
				for(const XMLElement& node: e) {
					WaterSystem::RiverNode n;
					n.left = n.right = node.attribute("width", 1.f);
					n.speed = node.attribute("speed", 1.f);
					n.a = n.b = node.attribute("handle", 1.f);
					sscanf(node.attribute("point"), "%g %g %g", &n.point.x, &n.point.y, &n.point.z);
					sscanf(node.attribute("tangent"), "%g %g %g", &n.direction.x, &n.direction.y, &n.direction.z);
					sscanf(node.attribute("handle"), "%g %g", &n.a, &n.b);
					sscanf(node.attribute("width"), "%g %g", &n.left, &n.right);
					river->nodes.push_back(n);
				}
				m_list->addItem("River", river, 38);
			}
			else if(e == "lake" || e=="ocean") {
				bool inside = e == "lake";
				WaterSystem::Lake* lake = m_waterSystem->addLake(inside);
				float height = e.attribute("height", 0.f);
				for(const XMLElement& node: e) {
					WaterSystem::SplineNode n;
					n.point.y = height;
					n.a = n.b = node.attribute("handle", 1.f);
					sscanf(node.attribute("point"), "%g %g", &n.point.x, &n.point.z);
					sscanf(node.attribute("tangent"), "%g %g", &n.direction.x, &n.direction.z);
					sscanf(node.attribute("handle"), "%g %g", &n.a, &n.b);
					lake->nodes.push_back(n);
				}
				m_list->addItem(inside? "Lake": "Ocean", lake, 38);
			}
		}
		updateGeometry();
	}
}

XMLElement WaterEditor::save(const TerrainMap* context) const {
	if(m_waterSystem->lakes().empty()) return XMLElement();
	char buffer[128];
	auto printElements = [&buffer](std::initializer_list<float> values) {
		char* c = buffer;
		for(float v: values) c+=sprintf(c, "%g ", v);
		c[-1] = 0;
		return buffer;
	};

	XMLElement water("water");
	for(const WaterSystem::Lake* lake : m_waterSystem->lakes()) {
		XMLElement& out = water.add(lake->inside? "lake": "ocean");
		out.setAttribute("height", lake->nodes[0].point.y);
		for(const WaterSystem::SplineNode& n: lake->nodes) {
			XMLElement& e = out.add("node");
			e.setAttribute("point", printElements({n.point.x, n.point.z}));
			e.setAttribute("tangent", printElements({n.direction.x, n.direction.z}));
			e.setAttribute("handle", printElements({n.a, n.b}));
		}
	}
	
	for(const WaterSystem::River* lake : m_waterSystem->rivers()) {
		XMLElement& out = water.add("river");
		for(const WaterSystem::RiverNode& n: lake->nodes) {
			XMLElement& e = out.add("node");
			e.setAttribute("point", printElements({n.point.x, n.point.y, n.point.z}));
			e.setAttribute("tangent", printElements({n.direction.x, n.direction.y, n.direction.z}));
			e.setAttribute("handle", printElements({n.a, n.b}));
			e.setAttribute("width", printElements({n.left, n.right}));
			e.setAttribute("speed", n.speed);
		}
	}

	return water;
}
void WaterEditor::update(const Mouse& mouse, const Ray& ray, base::Camera* camera, InputState& state) {
	if(!isActive()) return;
	static const vec3 up(0,1,0);

	float t;
	Ray cameraRay(camera->getPosition(), -camera->getDirection());
	if(m_terrain->trace(cameraRay, t)) m_centre = cameraRay.point(t);


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
				if(!over) {
					if(overPoint(n.point - n.direction*n.a)) { node=i; over=2; lake=l; }
					if(overPoint(n.point + n.direction*n.b)) { node=i; over=3; lake=l; }
				}
			}
		}
		for(WaterSystem::River* r: m_waterSystem->rivers()) {
			for(size_t i=0; i<r->nodes.size(); ++i) {
				WaterSystem::RiverNode& n = r->nodes[i];
				vec3 side = n.direction.cross(up).normalise();
				if(overPoint(n.point)) { node=i; over=1; lake=0; river=r; }
				if(!over) {
					if(overPoint(n.point - n.direction*n.a)) { node=i; over=2; lake=0; river=r; }
					if(overPoint(n.point + n.direction*n.b)) { node=i; over=3; lake=0; river=r; }
					if(overPoint(n.point - side*n.left))  { node=i; over=4; lake=0; river=r; }
					if(overPoint(n.point + side*n.right)) { node=i; over=5; lake=0; river=r; }
				}
			}
		}
		// Over Splines
		if((over==0 || closest > 1) && !(state.keyMask&CTRL_MASK)) {
			for(WaterSystem::Lake* l : m_waterSystem->lakes()) {
				for(size_t i=l->nodes.size()-1, j=0; j<l->nodes.size(); i=j++) {
					if(overSpline(l->nodes[i], l->nodes[j], 1, t)) { node=i; over=6; lake=l; }
				}
			}
			for(WaterSystem::River* r: m_waterSystem->rivers()) {
				for(size_t i=1; i<r->nodes.size(); ++i) {
					if(overSpline(r->nodes[i-1], r->nodes[i], 1, t)) { node=i-1; over=6; lake=0; river=r; }
				}
			}
		}

		if(over>0) {
			const uint col[] = { 0xff0000, 0xff8000, 0xff8000, 0x80ff, 0x80ff, 0x00ff00 };
			static DebugGeometry circle(SDG_ALWAYS);
			circle.circle(closestPoint, up, 0.1, 16, col[over-1]);
			if(mouse.pressed==1) {
				state.consumedMouseDown = true;
				m_held = over;
				m_activeNode = node;
				m_lake = lake;
				m_river = river;
				
				// Select in list
				ListItem* item = m_list->findItem([lake, river](ListItem& i) { return i.getData(1)==lake || i.getData(1)==river; });
				if(item) m_list->selectItem(item->getIndex(), true);


				// Add node
				if(over == 6) {
					++node;
					if(lake) {
						lake->nodes.insert(lake->nodes.begin() + node, lake->nodes[node-1]);
						lake->nodes[node].point = closestPoint;
					}
					if(river) {
						river->nodes.insert(river->nodes.begin() + node, river->nodes[node-1]);
						river->nodes[node].point = closestPoint;
					}
					m_held = 1;
					m_activeNode = node;
				}
			}

			// MouseWheel to change velocity
			if(river && over==1 && !state.consumedMouseWheel && mouse.wheel) {
				state.consumedMouseWheel = true;
				float& speed = river->nodes[node].speed;
				speed = speed * (1 + mouse.wheel * 0.1);
				updateGeometry();
				printf("Speed: %g\n", speed);
			}
		}
	}

	// delete node
	if(m_held && mouse.pressed==4) {
		if(m_lake && m_lake->nodes.size() > 2) {
			m_lake->nodes.erase(m_lake->nodes.begin() + m_activeNode);
			m_held = 0;
		}
		else if(m_river && m_river->nodes.size() > 2) {
			m_river->nodes.erase(m_river->nodes.begin() + m_activeNode);
			m_held = 0;
		}
		if(!m_held) {
			updateLines();
			updateGeometry();
			state.consumedMouseDown = true;
		}
	}


	// Move node
	if(m_held>0) {
		WaterSystem::SplineNode& node = m_river? m_river->nodes[m_activeNode]: m_lake->nodes[m_activeNode];

		// Moving lake handles needs to use node plane.
		// Moving lake nodes needs terrain trace, clamp to bounds, move others vertically.
		// Moving river nodes needs terrain trace.
		// need something to project outwards.
		// river handles could potentially not be flat.
		// Want snapping river start/end nodes, including rivers from other tiles
		// river nodes inside lakes need to be at lake height

		
		float tp=0, tt=0;
		m_terrain->trace(ray, tt);
		base::intersectRayPlane(ray.start, ray.direction, up, node.point.y, tp);
		if(tp>0 || tt>0) {
			bool shift = state.keyMask&SHIFT_MASK;
			WaterSystem::RiverNode* rnode = m_river? (WaterSystem::RiverNode*)&node: 0;
			if(m_held==1 && !shift) {
				
				// Vertical
				if(state.keyMask&ALT_MASK) {
					vec3 low = node.point, high = node.point;
					low.y = -100;
					high.y = 1000;
					float s, t;
					base::closestPointBetweenLines(ray.start, ray.point(1000), low, high, s, t);
					node.point = lerp(low, high, t);
					if(m_lake) {
						for(WaterSystem::SplineNode& n: m_lake->nodes) n.point.y = node.point.y;
					}
				}
				else {
					node.point = ray.point(m_lake? tp: tt-0.01);
					
					if(m_river) {
						vec3 side = node.direction.cross(up);
						vec3 a = node.point + side * rnode->left;
						vec3 b = node.point - side * rnode->right;
						a.y = m_terrain->getHeight(a);
						b.y = m_terrain->getHeight(b);
						node.point.y = fmin(a.y, b.y);
					}
				}


			}
			else {
				vec3 n = node.point - ray.point(tp);
				if(m_held <= 3 && m_river) {
					if(state.keyMask&ALT_MASK) { // Adjust pitch
						float s, t;
						vec3 pos = node.point + node.direction * (m_held<3? -node.a: node.b);
						base::closestPointBetweenLines(ray.start, ray.point(1000), pos-up*100, pos+up*100, s, t);
						n = node.point - (pos + up * (t * 200 - 100));
					}
					else if(node.direction.y != 0) {
						n.y = node.direction.y * n.length(); // Maintain pitch
						if(m_held==3) n.y = -n.y;
					}
				}

				float d = n.normaliseWithLength();
				switch(m_held) {
				case 1: case 2:	// Forward tangent
					node.direction = n;
					if(shift) node.a = node.b = d;
					else node.a = d;
					break;
				case 3:	// Reverse tangent
					node.direction = -n;
					if(shift) node.a = node.b = d;
					else node.b = d;
					break;
				case 4:	// River left side
					node.direction = -n.cross(up);
					if(shift) rnode->left = rnode->right = d;
					else rnode->left = d;
					break;
				case 5: // river right side
					node.direction = n.cross(up);
					if(shift) rnode->left = rnode->right = d;
					else rnode->right = d;
					break;
				}
			}
			updateLines();
		}
		if(!mouse.button) {
			updateGeometry();
			m_held = 0;
		}
	}

}

void WaterEditor::clear() {
	m_list->clearItems();
	delete m_waterSystem;
	m_waterSystem = new WaterSystem();
}

void WaterEditor::close() {
	EditorPlugin::close();
	updateLines();
}

void WaterEditor::activate() {
	updateLines();
}


void WaterEditor::addItem(Combobox* c, ListItem& item) {
	deselect();
	c->selectItem(-1);
	int type = item.getIndex();
	if(type == 0) {
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
		m_lake = m_waterSystem->addLake(type == 1);
		m_list->addItem(type==1?"Lake":"Ocean", m_lake, 38);
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
	updateGeometry();
}

void WaterEditor::deselect() {
	m_list->clearSelection();
	m_panel->getWidget("duplicatewater")->setEnabled(false);
	m_panel->getWidget("removewater")->setEnabled(false);
	m_river = nullptr;
	m_lake = nullptr;
}

void WaterEditor::selectItem(Listbox* list, ListItem& item) {
	m_river = item.findValue<WaterSystem::River*>();
	m_lake = item.findValue<WaterSystem::Lake*>();
	m_panel->getWidget("duplicatewater")->setEnabled(m_river || m_lake);
	m_panel->getWidget("removewater")->setEnabled(m_river || m_lake);
}

void WaterEditor::deleteItem(Button*) {
	if(m_river) m_waterSystem->destroyRiver(m_river);
	if(m_lake) m_waterSystem->destroyLake(m_lake);
	m_list->removeItem(m_list->getSelectedIndex());
	updateGeometry();
	updateLines();
}


void WaterEditor::duplicateItem(Button*) {
}

extern String appPath;
void WaterEditor::updateGeometry() {
	static Material* material = 0;
	if(!material) {
		constexpr uint c0 = 0xffff8040;
		constexpr uint c1 = 0xffffa050;
		const uint img[] = { c0, c1, c1, c0 };
		Texture tex = Texture::create(2,2,Texture::RGBA8, img);
		File source(appPath + "data/water.glsl");
		material = new Material();
		Pass* pass = material->addPass();
		base::Shader* shader = new base::Shader();
		shader->attach(new ShaderPart(VERTEX_SHADER, source.data()));
		shader->attach(new ShaderPart(FRAGMENT_SHADER, source.data()));
		shader->bindAttributeLocation("vertex",  0);
		shader->bindAttributeLocation("normal",  1);
		pass->setShader(shader);
		pass->getParameters().setAuto("transform", AUTO_MODEL_VIEW_PROJECTION_MATRIX);
		pass->getParameters().setAuto("modelMatrix", AUTO_MODEL_MATRIX);
		pass->getParameters().setAuto("time", AUTO_TIME);
		pass->getParameters().set("lightDirection", vec3(1,1,1));
		pass->setTexture("water", new Texture(tex));
		pass->compile();

		if(!shader->isCompiled()) {
			char buf[2048];
			shader->getLog(buf, sizeof(buf));
			puts(buf);
		}

	}

	BoundingBox bounds = m_terrain->getBounds();
	m_node->deleteAttachments();
	m_node->attach(new DrawableMesh(m_waterSystem->buildGeometry(bounds, 4), material));
}




void WaterEditor::updateLines() {
	static DebugGeometry g(SDG_MANUAL);
	if(!isActive()) { g.flush(); return; }
	auto drawSpline = [](const vec3& a, const vec3& da, const vec3& b, const vec3& db, uint colour) {
		vec3 last = a;
		const vec3 control[4] = { a, a+da, b-db, b };
		for(int t = 1; t<16; ++t) {
			vec3 p = calculateBezierPoint(control, t/16.f);
			g.line(last, p, colour|0xff000000);
			last = p;
		}
		g.line(last, b, colour|0xff000000);
	};


	for(WaterSystem::Lake* l : m_waterSystem->lakes()) {
		// Handles
		for(WaterSystem::SplineNode& n: l->nodes) {
			g.line(n.point - n.direction * n.a, n.point + n.direction * n.b, 0x000000);
		}
		// Splines
		for(size_t i=l->nodes.size()-1, j=0; j<l->nodes.size(); i=j++) {
			const WaterSystem::SplineNode& na = l->nodes[i];
			const WaterSystem::SplineNode& nb = l->nodes[j];
			drawSpline(na.point, na.direction*na.b, nb.point, nb.direction*nb.a, 0xffffff);
		}
	}
	static const vec3 up(0,1,0);
	for(WaterSystem::River* r : m_waterSystem->rivers()) {
		// Handles
		for(WaterSystem::RiverNode& n: r->nodes) {
			vec3 side = n.direction.cross(up).normalise();
			vec3 pa = n.point - n.direction * n.a;
			vec3 pb = n.point + n.direction * n.b;
			g.line(pa, pb, 0x000000);
			g.line(n.point - side * n.left, n.point + side * n.right, 0x0080ff);
			if(n.direction.y != 0) {
				g.line(pa, vec3(pa.x, n.point.y, pa.z), 0x000000);
				g.line(pb, vec3(pb.x, n.point.y, pb.z), 0x000000);
			}
		}
		// Splines
		for(size_t i=1; i<r->nodes.size(); ++i) {
			const WaterSystem::RiverNode& na = r->nodes[i-1];
			const WaterSystem::RiverNode& nb = r->nodes[i];
			vec3 sa = na.direction.cross(up).normalise();
			vec3 sb = nb.direction.cross(up).normalise();
			drawSpline(na.point, na.direction*na.b, nb.point, nb.direction*nb.a, 0xffffff);
			drawSpline(na.point-sa*na.left, na.direction*na.b, nb.point-sb*nb.left, nb.direction*nb.a, 0x0080ff);
			drawSpline(na.point+sa*na.right, na.direction*na.b, nb.point+sb*nb.right, nb.direction*nb.a, 0x0080ff);
		}
	}
	g.flush();
}

