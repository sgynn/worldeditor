#include "watereditor.h"
#include "water/water.h"
#include "heightmap.h"
#include <base/gui/lists.h>
#include <base/mesh.h>
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
#include <set>

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
	m_node->deleteChildren(true);
	m_list->clearItems();
	for(auto d: m_water) {
		delete d.second.system;
		delete d.second.mesh;
	}
	delete m_node;
}


void WaterEditor::notifyTileChanged(const Point& index, const TerrainMap* tile, const TerrainMap* previous) {
	base::SceneNode* node = m_nodes[index];
	if(previous) {
		node->deleteAttachments();
	}
	else if(tile) {
		char name[32];
		snprintf(name, 31, "%d,%d", index.x, index.y);
		m_nodes[index] = node = m_node->createChild(name, m_terrain->getOffset(index));
	}
	else {
		delete node;
		m_nodes.erase(index);
	}
	if(tile && m_water.find(tile) != m_water.end()) {
		WaterData& data = m_water[tile];
		if(data.system) {
			float s = m_terrain->getTileSize();
			BoundingBox bounds(0,-1000,0, s,1000,s);
			if(!data.mesh) data.mesh = data.system->buildGeometry(bounds, 4);
			node->attach(new DrawableMesh(data.mesh, getMaterial()));
		}
	}

	// Refresh list
	m_list->clearItems();
	std::set<const TerrainMap*> active;
	for(Point p: m_terrain->getUsedSlots()) active.insert(m_terrain->getMap(p));
	for(const TerrainMap* map : active) {
		if(WaterSystem* system = m_water[map].system) {
			for(River* river: system->rivers()) m_list->addItem("River", (Body*)river, 38, system);
			for(Lake* lake: system->lakes()) m_list->addItem(lake->inside? "Lake": "Ocean", (Body*)lake, 38, system);
		}
	}
}


void WaterEditor::load(const base::XMLElement& root, const TerrainMap* context) {
	if(!context) return;
	if(const XMLElement& water = root.find("water")) {
		WaterData& data = m_water[context];
		if(!data.system) data.system = new WaterSystem();
		for(const XMLElement& e: water) {
			if(e == "river") {
				WaterSystem::River* river = data.system->addRiver();
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
			}
			else if(e == "lake" || e=="ocean") {
				bool inside = e == "lake";
				WaterSystem::Lake* lake = data.system->addLake(inside);
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
			}
		}
	}
}

XMLElement WaterEditor::save(const TerrainMap* context) const {
	auto it = m_water.find(context);
	if(it == m_water.end() || !it->second.system || it->second.system->empty()) return XMLElement();
	WaterSystem* system = it->second.system;

	char buffer[128];
	auto printElements = [&buffer](std::initializer_list<float> values) {
		char* c = buffer;
		for(float v: values) c+=sprintf(c, "%g ", v);
		c[-1] = 0;
		return buffer;
	};

	XMLElement water("water");
	for(const WaterSystem::Lake* lake : system->lakes()) {
		XMLElement& out = water.add(lake->inside? "lake": "ocean");
		out.setAttribute("height", lake->nodes[0].point.y);
		for(const WaterSystem::SplineNode& n: lake->nodes) {
			XMLElement& e = out.add("node");
			e.setAttribute("point", printElements({n.point.x, n.point.z}));
			e.setAttribute("tangent", printElements({n.direction.x, n.direction.z}));
			e.setAttribute("handle", printElements({n.a, n.b}));
		}
	}
	
	for(const WaterSystem::River* river : system->rivers()) {
		XMLElement& out = water.add("river");
		for(const WaterSystem::RiverNode& n: river->nodes) {
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

	auto isRiver = [](Body* b) { return b && b->type == WaterSystem::Type::River? static_cast<River*>(b): nullptr; };
	auto isLake = [](Body* b) { return b && b->type != WaterSystem::Type::River? static_cast<Lake*>(b): nullptr; };

	// Detect mouse over
	if(m_held==Held::None && !state.consumedMouseDown) {
		Body* body = nullptr;
		int node = -1;
		Held over = Held::None;
		float closest = 4;
		vec3 closestPoint;
		Ray localRay = ray;

		auto overPoint = [&](const vec3& point) {
			vec3 r = base::closestPointOnLine(point, localRay.start, localRay.point(1000));
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
				float d = base::closestPointBetweenLines(localRay.start, localRay.point(1000), last, p, u,v);
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

		for(auto& tile: m_nodes) {
			WaterSystem* system = m_water[m_terrain->getMap(tile.first)].system;
			if(!system) continue;
			localRay.start = ray.start + tile.second->getPosition();

			auto setOver = [&](Body* activeBody, int nodeIndex, Held mode) {
				m_offset = tile.second->getPosition();
				m_system = system;
				body = activeBody;
				node = nodeIndex;
				over = mode;
			};

			// Over nodes
			for(WaterSystem::Lake* l : system->lakes()) {
				for(size_t i=0; i<l->nodes.size(); ++i) {
					WaterSystem::SplineNode& n = l->nodes[i];
					if(overPoint(n.point)) setOver(l, i, Held::Node);
					if(!over) {
						if(overPoint(n.point - n.direction*n.a)) setOver(l, i, Held::SplineA);
						if(overPoint(n.point + n.direction*n.b)) setOver(l, i, Held::SplineB);
					}
				}
			}
			for(WaterSystem::River* r: system->rivers()) {
				for(size_t i=0; i<r->nodes.size(); ++i) {
					WaterSystem::RiverNode& n = r->nodes[i];
					vec3 side = n.direction.cross(up).normalise();
					if(overPoint(n.point)) setOver(r, i, Held::Node);
					if(!over) {
						if(overPoint(n.point - n.direction*n.a)) setOver(r, i, Held::SplineA);
						if(overPoint(n.point + n.direction*n.b)) setOver(r, i, Held::SplineB);
						if(overPoint(n.point - side*n.left))  setOver(r, i, Held::SideLeft);
						if(overPoint(n.point + side*n.right)) setOver(r, i, Held::SideRight);
					}
				}
			}
			// Over Splines
			if((!over || closest > 1) && !(state.keyMask&CTRL_MASK)) {
				for(WaterSystem::Lake* l : system->lakes()) {
					for(size_t i=l->nodes.size()-1, j=0; j<l->nodes.size(); i=j++) {
						if(overSpline(l->nodes[i], l->nodes[j], 1, t)) setOver(l, i, Held::Split);
					}
				}
				for(WaterSystem::River* r: system->rivers()) {
					for(size_t i=1; i<r->nodes.size(); ++i) {
						if(overSpline(r->nodes[i-1], r->nodes[i], 1, t)) setOver(r, i - 1, Held::Split);
					}
				}
			}
		}

		if(over != Held::None) {
			const uint col[] = { 0xff0000, 0xff8000, 0xff8000, 0x80ff, 0x80ff, 0x00ff00 };
			static DebugGeometry circle(SDG_ALWAYS);
			float radius = 0.01 * ray.start.distance(closestPoint - m_offset);

			circle.circle(closestPoint - m_offset, up, radius, 16, col[(int)over-1]);
			if(mouse.pressed==1) {
				state.consumedMouseDown = true;
				m_held = over;
				m_activeNode = node;
				m_active = body;
				
				// Select in list
				if(ListItem* item = m_list->findItem(1, body)) m_list->selectItem(item->getIndex(), true);

				// Add node
				if(over == Held::Split) {
					++node;
					if(River* river = isRiver(body)) {
						river->nodes.insert(river->nodes.begin() + node, river->nodes[node-1]);
						river->nodes[node].point = closestPoint;
					}
					else if(Lake* lake = isLake(body)) {
						lake->nodes.insert(lake->nodes.begin() + node, lake->nodes[node-1]);
						lake->nodes[node].point = closestPoint;
					}
					m_held = Held::Node;
					m_activeNode = node;
				}
			}

			// MouseWheel to change velocity
			if(over==Held::Node && !state.consumedMouseWheel && mouse.wheel && isRiver(body)) {
				state.consumedMouseWheel = true;
				float& speed = isRiver(body)->nodes[node].speed;
				speed = speed * (1 + mouse.wheel * 0.1);
				updateGeometry(m_system);
				printf("Speed: %g\n", speed);
			}
		}
	}

	// delete node
	if(m_held != Held::None && mouse.pressed==4) {
		if(River* river = isRiver(m_active)) {
			if(river->nodes.size() > 2) {
				river->nodes.erase(river->nodes.begin() + m_activeNode);
				m_held = Held::None;
			}
		}
		if(Lake* lake = isLake(m_active)) {
			if(lake->nodes.size() > 2) {
				lake->nodes.erase(lake->nodes.begin() + m_activeNode);
				m_held = Held::None;
			}
		}
		if(!m_held) {
			updateLines();
			updateGeometry(m_system);
			state.consumedMouseDown = true;
		}
	}


	// Move node
	if(m_held != Held::None) {
		bool river = m_active->type == WaterSystem::Type::River;
		WaterSystem::SplineNode& node = river? isRiver(m_active)->nodes[m_activeNode]: isLake(m_active)->nodes[m_activeNode];

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
			WaterSystem::RiverNode* rnode = river? (WaterSystem::RiverNode*)&node: 0;
			if(m_held == Held::Node && !shift) { // Move node
				
				// Vertical
				if(state.keyMask&ALT_MASK) {
					vec3 low = node.point + m_offset, high = node.point + m_offset;
					low.y = -100;
					high.y = 1000;
					float s, t;
					base::closestPointBetweenLines(ray.start, ray.point(1000), low, high, s, t);
					node.point = lerp(low, high, t) - m_offset;
					// Lakes are horizontal - so move all nodes
					if(Lake* lake = isLake(m_active)) {
						for(WaterSystem::SplineNode& n: lake->nodes) n.point.y = node.point.y;
					}
				}
				else {
					node.point = ray.point(river? tt - 0.01: tp) + m_offset;

					static DebugGeometry dd(SDG_AUTOMATIC);
					dd.marker(ray.point(tt));
					
					if(river) {
						vec3 side = node.direction.cross(up);
						vec3 a = node.point + side * rnode->left;
						vec3 b = node.point - side * rnode->right;
						a.y = m_terrain->getHeight(a);
						b.y = m_terrain->getHeight(b);
						node.point.y = fmin(a.y, b.y);
					}
				}


			}
			else { // Move control points
				vec3 n = node.point - ray.point(tp) - m_offset;
				if(river && (m_held == Held::SplineA || m_held == Held::SplineB)) {
					if(state.keyMask&ALT_MASK) { // Adjust pitch
						float s, t;
						vec3 pos = node.point + node.direction * (m_held==Held::SplineA? -node.a: node.b);
						base::closestPointBetweenLines(ray.start - m_offset, ray.point(1000), pos-up*100, pos+up*100, s, t);
						n = node.point - (pos + up * (t * 200 - 100));
					}
					else if(node.direction.y != 0) {
						n.y = node.direction.y * n.length(); // Maintain pitch
						if(m_held==Held::SplineB) n.y = -n.y;
					}
				}

				float d = n.normaliseWithLength();
				switch(m_held) {
				default: break;
				case Held::Node: // When shift is held and the node is selected
				case Held::SplineA:	// Forward tangent
					node.direction = n;
					if(shift) node.a = node.b = d;
					else node.a = d;
					break;
				case Held::SplineB:	// Reverse tangent
					node.direction = -n;
					if(shift) node.a = node.b = d;
					else node.b = d;
					break;
				case Held::SideLeft:	// River left side
					node.direction = -n.cross(up);
					if(shift) rnode->left = rnode->right = d;
					else rnode->left = d;
					break;
				case Held::SideRight: // river right side
					node.direction = n.cross(up);
					if(shift) rnode->left = rnode->right = d;
					else rnode->right = d;
					break;
				}
			}
			updateLines();
		}
		if(!mouse.button) {
			updateGeometry(m_system);
			m_held = Held::None;
		}
	}

}

void WaterEditor::clear() {
	m_list->clearItems();
	m_node->deleteChildren(true);
	for(auto& i: m_water) {
		delete i.second.system;
		delete i.second.mesh;
	}
	m_water.clear();
	m_nodes.clear();
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

	TerrainMap* map = m_terrain->getMap(m_centre);
	if(!map) return;
	WaterData& data = m_water[map];
	if(!data.system) data.system = new WaterSystem();
	m_system = data.system;

	if(type == 0) {
		River* river = m_system->addRiver();
		WaterSystem::RiverNode node;
		node.point = m_centre - vec3(10,0,0);
		node.direction.set(1,0,0);
		river->nodes.push_back(node);
		node.point = m_centre + vec3(10,0,0);
		river->nodes.push_back(node);
		m_active = river;
		m_list->addItem("River", m_active, 38, m_system);
	}
	else {
		Lake* lake = m_system->addLake(type == 1);
		WaterSystem::SplineNode node;
		node.a = node.b = 4;
		node.point = m_centre + vec3(5,0,0);
		node.direction.set(0,0,1);
		lake->nodes.push_back(node);
		node.point = m_centre + vec3(0,0,5);
		node.direction.set(-1,0,0);
		lake->nodes.push_back(node);
		node.point = m_centre + vec3(-5,0,0);
		node.direction.set(0,0,-1);
		lake->nodes.push_back(node);
		node.point = m_centre + vec3(0,0,-5);
		node.direction.set(1,0,0);
		lake->nodes.push_back(node);
		m_active = lake;
		m_list->addItem(type==1?"Lake":"Ocean", m_active, 38, m_system);
	}
	updateLines();
	updateGeometry(m_system);
}

void WaterEditor::deselect() {
	m_list->clearSelection();
	m_panel->getWidget("duplicatewater")->setEnabled(false);
	m_panel->getWidget("removewater")->setEnabled(false);
	m_active = nullptr;
}

void WaterEditor::selectItem(Listbox* list, ListItem& item) {
	m_active = item.findValue<Body*>();
	m_system = item.findValue<WaterSystem*>();
	m_panel->getWidget("duplicatewater")->setEnabled(m_active);
	m_panel->getWidget("removewater")->setEnabled(m_active);
}

void WaterEditor::deleteItem(Button*) {
	if(!m_active) return;
	if(m_active->type == WaterSystem::Type::River) m_system->destroyRiver((River*)m_active);
	else m_system->destroyLake((Lake*)m_active);
	m_list->removeItem(m_list->getSelectedIndex());
	updateGeometry(m_system);
	updateLines();
}


void WaterEditor::duplicateItem(Button*) {
}

extern String appPath;
Material* WaterEditor::getMaterial() {
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
	return material;
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

	for(auto& tile : m_nodes) {
		vec3 offset = tile.second->getPosition();
		TerrainMap* map = m_terrain->getMap(tile.first);
		WaterSystem* system = m_water[map].system;
		if(!system) continue;


		for(WaterSystem::Lake* l : system->lakes()) {
			// Handles
			for(WaterSystem::SplineNode& n: l->nodes) {
				g.line(n.point - n.direction * n.a + offset, n.point + n.direction * n.b + offset, 0x000000);
			}
			// Splines
			for(size_t i=l->nodes.size()-1, j=0; j<l->nodes.size(); i=j++) {
				const WaterSystem::SplineNode& na = l->nodes[i];
				const WaterSystem::SplineNode& nb = l->nodes[j];
				drawSpline(na.point+offset, na.direction*na.b, nb.point+offset, nb.direction*nb.a, 0xffffff);
			}
		}
		static const vec3 up(0,1,0);
		for(WaterSystem::River* r : system->rivers()) {
			// Handles
			for(WaterSystem::RiverNode& n: r->nodes) {
				vec3 point = n.point + offset;
				vec3 side = n.direction.cross(up).normalise();
				vec3 pa = point - n.direction * n.a;
				vec3 pb = point + n.direction * n.b;
				g.line(pa, pb, 0x000000);
				g.line(point - side * n.left, point + side * n.right, 0x0080ff);
				if(n.direction.y != 0) {
					g.line(pa, vec3(pa.x, point.y, pa.z), 0x000000);
					g.line(pb, vec3(pb.x, point.y, pb.z), 0x000000);
				}
			}
			// Splines
			for(size_t i=1; i<r->nodes.size(); ++i) {
				const WaterSystem::RiverNode& na = r->nodes[i-1];
				const WaterSystem::RiverNode& nb = r->nodes[i];
				vec3 sa = na.direction.cross(up).normalise();
				vec3 sb = nb.direction.cross(up).normalise();
				drawSpline(na.point+offset, na.direction*na.b, nb.point+offset, nb.direction*nb.a, 0xffffff);
				drawSpline(na.point-sa*na.left+offset, na.direction*na.b, nb.point-sb*nb.left+offset, nb.direction*nb.a, 0x0080ff);
				drawSpline(na.point+sa*na.right+offset, na.direction*na.b, nb.point+sb*nb.right+offset, nb.direction*nb.a, 0x0080ff);
			}
		}
	}
	g.flush();
}

void WaterEditor::updateGeometry(WaterSystem* system) {
	for(auto& data: m_water) {
		if(data.second.system == system) {
			delete data.second.mesh;
			float s = m_terrain->getTileSize();
			BoundingBox bounds(0,-1000,0, s,1000,s);
			data.second.mesh = data.second.system->buildGeometry(bounds, 4);
			// Update all drawables
			for(auto& n: m_nodes) {
				if(m_terrain->getMap(n.first) == data.first) {
					if(n.second->getAttachmentCount()) static_cast<DrawableMesh*>(n.second->getAttachment(0))->setMesh(data.second.mesh);
					else n.second->attach(new DrawableMesh(data.second.mesh, getMaterial()));
				}
			}
			break;
		}
	}
}

