#include "objecteditor.h"
#include "heightmap.h"
#include "filesystem.h"
#include "gizmo.h"
#include "boxselect.h"
#include <base/gui/widgets.h>
#include <base/gui/lists.h>
#include <base/gui/tree.h>
#include <base/scene.h>
#include <base/drawablemesh.h>
#include <base/hardwarebuffer.h>
#include <base/xml.h>
#include <base/collision.h>
#include <base/directory.h>

#include "extensions.h"
#include <base/model.h>
#include <base/bmloader.h>
#include <base/shader.h>
#include <base/material.h>
#include <base/autovariables.h>
#include <base/texture.h>
#include <base/game.h>
#include <base/input.h>
#include <base/png.h>
#include <set>


using base::XMLElement;
using namespace gui;
using base::Model;
using base::Mesh;
using base::SceneNode;
using base::Drawable;
using base::DrawableMesh;

extern gui::String appPath;

#define CONNECT(Type, name, event, callback) { Type* t=m_panel->getWidget<Type>(name); if(t) t->event.bind(this, &ObjectEditor::callback); else printf("Missing widget: %s\n", name); }
#define CONNECT_I(Type, name, event, ...) { Type* t=m_panel->getWidget<Type>(name); if(t) t->event.bind(__VA_ARGS__); else printf("Error: Missing widget %s\n", name); }

ObjectEditor::ObjectEditor(gui::Root* gui, FileSystem* fs, MapGrid* terrain, SceneNode* scene)
	: m_fileSystem(fs), m_terrain(terrain), m_placement(0), m_mode(SINGLE), m_collideObjects(true), m_gizmo(0)
	, m_resource(0)
{
	createPanel(gui, "objecteditor", "objects.xml");
	createToolButton(gui, "Objects");

	m_node = scene->createChild("Objects");
	m_objectList = m_panel->getWidget<Listbox>("objectlist");
	m_resourceList = m_panel->getWidget<TreeView>("resources");
	
	CONNECT(Textbox,  "path", eventSubmit, changePath);
	CONNECT(Listbox, "objectlist", eventSelected, selectObject);
	CONNECT(Listbox, "objectlist", eventCustom, changeVisible)
	CONNECT(TreeView, "resources", eventSelected, selectResource);
	CONNECT(Textbox,  "property", eventLostFocus, setProperty);
	CONNECT_I(Textbox,  "property", eventSubmit, [this](Textbox*){m_panel->setFocus(); });

	CONNECT_I(Button, "move", eventPressed, [this](Button*) { setGizmoMode(0); });
	CONNECT_I(Button, "rotate", eventPressed, [this](Button*) { setGizmoMode(1); });
	CONNECT_I(Button, "scale", eventPressed, [this](Button*) { setGizmoMode(2); });
	CONNECT_I(Button, "local", eventPressed, [this](Button*) { setGizmoSpaceLocal(true); });
	CONNECT_I(Button, "global", eventPressed, [this](Button*) { setGizmoSpaceLocal(false); });

	CONNECT_I(Checkbox, "visible", eventChanged, [this](Button* b) { m_node->setVisible(b->isSelected()); });

	m_gizmo = new editor::Gizmo();
	m_gizmo->setRenderQueue(15);
	scene->attach(m_gizmo);
	m_gizmo->setVisible(false);
	m_selectGroup = m_node->createChild("Selection");
	setGizmoSpaceLocal(false);
	setGizmoMode(0);

	m_box = new BoxSelect();
	m_panel->getParent()->add(m_box);

	base::BMLoader::registerExtension("layout", LayoutExtension::construct);

	setupMaterials();
	setResourcePath(fs->getRootPath());
}

ObjectEditor::~ObjectEditor() {
	clear();
}

void ObjectEditor::close() {
	cancelPlacement();
	selectObject(0);
}

// ------------------------------------------------------------------------------ //

inline Object* getObject(const ListItem& item) {
	return item.getValue<Object*>(1, nullptr);
}

void ObjectEditor::load(const XMLElement& e, const TerrainMap* context) {
	const XMLElement& list = e.find("objects");
	for(const XMLElement& i: list) {
		if(i == "object") {
			Quaternion rot;
			vec3 pos, scale(1,1,1);
			const char* name = i.attribute("name");
			const char* file = i.attribute("file");
			const char* data = i.attribute("data");
			int meshIndex = i.attribute("mesh", -1);
			sscanf(i.attribute("scale"), "%g %g %g", &scale.x, &scale.y, &scale.z);
			sscanf(i.attribute("position"), "%g %g %g", &pos.x, &pos.y, &pos.z);
			sscanf(i.attribute("orientation"), "%g %g %g %g", &rot.w, &rot.x, &rot.y, &rot.z);

			
			Model* nope = (Model*)(size_t)1;
			String meshFile = m_fileSystem->getFile(file);
			Model* model = m_models.get(meshFile.str(), nope);
			if(model == nope) {
				model = base::BMLoader::load(meshFile);
				m_models[meshFile.str()] = model;
			}

			if(model && meshIndex < model->getMeshCount()) {
				Object* object;
				if(meshIndex<0) object = createObject(name, model, 0, 0); // Full model
				else object = createObject(name, 0, model->getMesh(meshIndex), model->getMaterialName(meshIndex));
				m_node->addChild(object);
				object->setTransform(pos, rot, scale);
				object->updateBounds();
				m_objectList->addItem(name, object, meshFile, meshIndex, true, data);
			}
		}
	}
}

XMLElement ObjectEditor::save(const TerrainMap* context) const {
	char buffer[256];
	XMLElement xml("objects");
	for(const ListItem& item: m_objectList->items()) {
		Object* object = getObject(item);
		if(!object) continue;
		const vec3 scl = object->getScale();
		const vec3& pos = object->getPosition();
		const Quaternion& rot = object->getOrientation();
		int mesh = item.getValue(3, -1);
		String data = item.getText(5);
		

		XMLElement& e = xml.add("object");
		e.setAttribute("name", item.getText());
		e.setAttribute("file", m_fileSystem->getRelative(item.getText(2)));
		if(mesh>=0) e.setAttribute("mesh", mesh);
		
		if(pos!=vec3()) {
			sprintf(buffer, "%g %g %g", pos.x, pos.y, pos.z);
			e.setAttribute("position", buffer);
		}
		if(rot!=Quaternion(1,0,0,0)) {
			sprintf(buffer, "%g %g %g %g", rot.w, rot.x, rot.y, rot.z);
			e.setAttribute("orientation", buffer);
		}
		if(scl!=vec3(1,1,1)) {
			sprintf(buffer, "%g %g %g", scl.z, scl.y, scl.z);
			e.setAttribute("scale", buffer);
		}
		if(!data.empty()) e.setAttribute("data", data.str());
	}

	return xml;
}

void ObjectEditor::clear() {
	m_node->deleteChildren(true);
	m_objectList->clearItems();
}

// ------------------------------------------------------------------------------ //

void ObjectEditor::changePath(Textbox* t) {
	setResourcePath(t->getText());
}

void ObjectEditor::setProperty(Widget* w) {
	String prop = w->cast<Textbox>()->getText();
	for(ListItem& i: m_objectList->selectedItems()) {
		i.setValue(5, prop);
	}
}

void ObjectEditor::setGizmoMode(int mode) {
	if(m_gizmo->isHeld()) return;
	m_gizmo->setMode((editor::Gizmo::Mode) mode);
	m_panel->getWidget("move")->setSelected(mode==0);
	m_panel->getWidget("rotate")->setSelected(mode==1);
	m_panel->getWidget("scale")->setSelected(mode==2);
}
void ObjectEditor::setGizmoSpaceLocal(bool local) {
	if(m_gizmo->isHeld()) return;
	if(local) m_gizmo->setLocalMode();
	else m_gizmo->setRelative(Matrix());
	m_panel->getWidget("local")->setSelected(local);
	m_panel->getWidget("global")->setSelected(!local);
}

void ObjectEditor::update(const Mouse& mouse, const Ray& ray, base::Camera* camera, InputState& state) {
	if(!m_panel->isVisible()) return;

	bool keys = !m_panel->getRoot()->getFocusedWidget()->cast<Textbox>();

	// Placement update
	if(m_placement) { 
		float t;
		
		// Altitude
		if(mouse.wheel && !state.consumedMouseWheel) {
			state.consumedMouseWheel = true;
			if(state.keyMask&ALT_MASK && m_mode==CHAIN) {
				m_chainStart += m_placement->getOrientation()*m_chainStep.normalised()*0.1*mouse.wheel;
			}
			else {
				m_altitude.y += mouse.wheel * 0.1;
				if(state.keyMask&SHIFT_MASK) m_chainStart.y += mouse.wheel * 0.1;
			}
		}

		// Position
		if(m_mode == SINGLE) {
			bool hit = m_terrain->trace(ray, t);
			if(m_collideObjects) hit |= pick(m_node, ray, true, t)!=0;
			if(hit) {
				m_placement->setPosition(ray.point(t) + m_altitude);
				m_placement->setVisible(true);
			}
			else m_placement->setVisible(false);
		}


		// Rotation
		if(m_mode==SINGLE && mouse.pressed==1 && !state.overGUI) {
			state.consumedMouseDown = true;
			m_pressed = mouse;
			m_mode = ROTATE;
			m_started = false;
			m_yawOffset = 0;
		}
		else if(m_mode == ROTATE) {
			vec3 pt;
			const vec3& ps = m_placement->getPosition();
			if(base::intersectRayPlane(ray.start, ray.direction, vec3(0,1,0), ps.y, pt)) {
				if(!m_started && abs(mouse.x-m_pressed.x)+abs(mouse.y-m_pressed.y)>5) {
					m_yawOffset = atan2(pt.x-ps.x, pt.z-ps.z) - m_placement->getOrientation().getAngle();
					m_started = true;
				}
				else if(m_started) {
					float yaw = atan2(pt.x-ps.x, pt.z-ps.z) - m_yawOffset;
					m_placement->setOrientation(Quaternion(vec3(0,1,0), yaw));
				}
			}
		}

		// Chain mode
		if(m_mode == CHAIN && m_terrain->trace(ray, t)) {
			if(!m_started) {
				m_chainStart = ray.point(t) + m_altitude;
				m_placement->setPosition(m_chainStart - m_placement->getOrientation() * m_chainStep * 0.5);
				if(!state.overGUI && mouse.released==1) { m_started = true; return; }
			}
			else {
				vec3 end = ray.point(t) + m_altitude;
				float distance = m_chainStart.distance(end);
				float step = m_chainStep.length();
				size_t count = ceil(distance / step);
				if(distance > 0 && step>0) {
					for(uint i=0; i<m_chain.size(); ++i) m_chain[i]->setVisible(i<count);
					for(size_t i=m_chain.size(); i<count; ++i) {
						m_placement = 0;
						selectResource(0, m_resource);
						m_chain.push_back(m_placement);
					}
					m_placement = m_chain[0];

					vec3 z = (end - m_chainStart) / distance;
					vec3 x = vec3(0,1,0).cross(z);
					vec3 y = z.cross(x);
					Quaternion rot(Matrix(x,y,z));
					rot *= Quaternion::arc(vec3(0,0,1), m_chainStep);
					for(size_t i=0; i<m_chain.size(); ++i) {
						m_chain[i]->setPosition(m_chainStart + z * step * (i+0.5));
						m_chain[i]->setOrientation(rot);
					}
				}
			}
		}
		if(m_mode==SINGLE && base::Game::Pressed(base::KEY_C) && keys) {
			Quaternion tmp = m_placement->getOrientation();
			m_placement->setOrientation(Quaternion());
			m_placement->updateBounds();
			m_placement->setOrientation(tmp);
			m_chainStep = m_placement->getAttachment(0)->getBounds().size();
			m_chainStep.y = 0;
			if(m_chainStep.x>m_chainStep.z) m_chainStep.z=0; else m_chainStep.x=0;
			if(m_chainStep.length2()>0) {
				m_chain.clear();
				m_chain.push_back(m_placement);
				m_mode = CHAIN;
				m_started = false;
			}
		}
		else if(m_mode == CHAIN && base::Game::Pressed(base::KEY_C) && keys) {
			Quaternion tmp = m_placement->getOrientation();
			tmp.x = tmp.z = 0;
			tmp.normalise();
			m_placement->setOrientation(tmp);
			for(size_t i=1; i<m_chain.size(); ++i) delete m_chain[i];
			m_chain.clear();
			m_mode = SINGLE;
		}


		// Finalise
		if(mouse.released&1) {
			if(!m_started && m_mode==ROTATE) m_mode = SINGLE;
			switch(m_mode) {
			case ROTATE:
				m_mode = SINGLE;
				base::Game::input()->warpMouse(m_pressed.x, m_pressed.y);
				break;
			
			case SINGLE:
				if(!state.overGUI) {
					Object* object = m_placement;
					placeObject(object, m_resource);
					m_placement = 0;
					if(state.keyMask&SHIFT_MASK) {
						selectResource(0, m_resource);
						m_placement->setOrientation(object->getOrientation());
					}
					else {
						cancelPlacement();
						selectObject(object); 
					}
				}
				break;

			case CHAIN:
				if(!state.overGUI) {
					int count = 0;
					float step = m_chainStep.length();
					for(Object* o: m_chain) {
						if(!o->isVisible()) break;
						placeObject(o, m_resource);
						++count;
					}
					m_chain.clear();
					selectResource(0, m_resource);
					m_chain.push_back(m_placement);
					m_chainStart += (ray.point(t)+m_altitude-m_chainStart).normalise() * (count*step);
				}
				break;
			}
		}

		if(mouse.pressed&4) {
			state.consumedMouseDown = true;
			cancelPlacement();
			m_mode=SINGLE;
			m_altitude.set(0,0,0);
		}
		return;
	}

	// Edit selected objects
	if(m_gizmo->isVisible()) {

		// Delete selection
		if(base::Game::Pressed(base::KEY_DEL) && keys) {
			while(m_objectList->getSelectionSize()) {
				delete getObject(*m_objectList->getSelectedItem());
				m_objectList->removeItem(m_objectList->getSelectedItem()->getIndex());
			}
			m_gizmo->setVisible(false);
			return;
		}

		// Gizmo update
		bool wasHeld = m_gizmo->isHeld();
		editor::MouseRay mouseRay(camera, mouse.x, mouse.y, base::Game::width(), base::Game::height());
		if(mouse.released&1) m_gizmo->onMouseUp();
		if((mouse.pressed&1) && m_gizmo->onMouseDown(mouseRay)) state.consumedMouseDown = true;
		if(mouse.delta.x || mouse.delta.y) {
			m_gizmo->onMouseMove(mouseRay);
			if(m_gizmo->isHeld()) {
				if(m_objectList->getSelectionSize() == 1) {
					Object* target = getObject(*m_objectList->getSelectedItem());
					target->setTransform(m_gizmo->getPosition(), m_gizmo->getOrientation(), m_gizmo->getScale());
					target->updateBounds();
				}
				else { // Moving multiple objects
					m_selectGroup->setTransform(m_gizmo->getPosition(), m_gizmo->getOrientation(), m_gizmo->getScale());
					for(SceneNode* n: m_selectGroup->children()) static_cast<Object*>(n)->updateBounds();
				}
			}
		}
		if(wasHeld) return;

		// Additional functions (buttons)
		if(!m_gizmo->isHeld()) {
			if(base::Game::Pressed(base::KEY_1)) setGizmoMode(editor::Gizmo::POSITION);
			if(base::Game::Pressed(base::KEY_2)) setGizmoMode(editor::Gizmo::ORIENTATION);
			if(base::Game::Pressed(base::KEY_3)) setGizmoMode(editor::Gizmo::SCALE);
			if(base::Game::Pressed(base::KEY_4)) setGizmoSpaceLocal(true);
			if(base::Game::Pressed(base::KEY_5)) setGizmoSpaceLocal(false);
			if(base::Game::Pressed(base::KEY_G)) { // Project to ground
				selectionChanged();
				for(ListItem& i: m_objectList->selectedItems()) {
					Object* o = getObject(i);
					float t = 100;
					vec3 pos = o->getPosition() + o->getParent()->getPosition();
					Ray down(pos + vec3(0,0.1,0), vec3(0,-1,0));
					pos.y = m_terrain->getHeight(pos);
					if(m_collideObjects && pick(m_node, down, true, t)) {
						float p = down.point(t).y;
						if(p>pos.y) pos.y = p;
					}
					o->setPosition(pos - o->getParent()->getPosition());
					o->updateBounds();
				}
				selectionChanged();
			}
		}
	}

	// Toggle visibility
	if(base::Game::Pressed(base::KEY_H) && keys && base::Game::input()->getKeyModifier()&base::MODIFIER_ALT) {
		for(ListItem& i: m_objectList->items()) {
			Object* o = i.getValue<Object*>(1, nullptr);
			if(o && !o->isVisible()) {
				o->setVisible(true);
				i.setValue(4, true);
			}
		}
		m_objectList->refresh();
	}
	else if(base::Game::Pressed(base::KEY_H) && keys) {
		for(ListItem& item: m_objectList->selectedItems()) {
			getObject(item)->setVisible(false);
			item.setValue(4, false);
		}
		m_objectList->refresh();
		clearSelection();
	}

	// Picking
	if(!m_placement && !m_gizmo->isHeld()) {
		if(mouse.pressed==1 && !state.consumedMouseDown) {
			printf("Start box\n");
			m_box->start();
		}
		else if(mouse.released&1) {
			if(m_box->isValid()) {
				printf("End box\n");
				m_box->updatePlanes(camera);
				if(~state.keyMask&SHIFT_MASK) clearSelection();
				for(SceneNode* node: m_node->children()) {
					Object* object = static_cast<Object*>(node);
					if(object->isVisible() && m_box->inside(object->getBounds())) selectObject(object, true);
				}
				m_box->clear();
			}
			else if(!state.overGUI) {
				printf("Point select\n");
				float t = 1e4f;
				m_terrain->trace(ray, t);
				Object* sel = pick(m_node, ray, false, t);
				selectObject(sel, state.keyMask&SHIFT_MASK);
			}
		}
	}
}

void ObjectEditor::cancelPlacement() {
	if(m_mode == CHAIN) for(Object* o: m_chain) { o->deleteChildren(true); delete o; }
	else if(m_placement) {
		m_placement->deleteChildren(true);
		delete m_placement;
	}
	m_chain.clear();
	m_placement = 0;
	m_resourceList->clearSelection();
	if(m_mode == ROTATE) m_mode = SINGLE;
}

void ObjectEditor::placeObject(Object* object, TreeNode* data) {
	// Properties: Name, Object, Model, MeshIndex, Visible, Custom
	const char* modelFile = data->getText(2);
	int meshIndex = modelFile? -1: data->getIndex();
	if(!modelFile) modelFile = data->getParent()->getText(2);
	m_objectList->addItem(object->getName(), object, modelFile, meshIndex, true);
	object->updateBounds();
}

void ObjectEditor::changeVisible(Listbox*, ListItem& item, Widget*) {
	Object* obj = getObject(item);
	obj->setVisible(item.getValue(4, true));
	if(isSelected(obj)) clearSelection();
}

void ObjectEditor::selectObject(Listbox*, ListItem& item) {
	selectObject(getObject(item));
}

void ObjectEditor::selectObject(Object* obj, bool append) {
	printf("Select object %s\n", obj? obj->getName(): "none");
	if(!obj && !append) clearSelection();
	if(!obj || isSelected(obj)) return;
	if(!append) clearSelection();

	ListItem* item = m_objectList->findItem([obj](ListItem&i){ return getObject(i)==obj; });
	if(item) {
		m_objectList->selectItem(item->getIndex());
		m_objectList->ensureVisible(item->getIndex());
		m_panel->getWidget<Textbox>("property")->setText(item->getText(5));
		// TODO: Other properties
	}

	selectionChanged();
}

void setRenderQueue(SceneNode* node, int queue) {
	for(Drawable* d: node->attachments()) d->setRenderQueue(queue);
	for(SceneNode* n: node->children()) setRenderQueue(n, queue);
}

void ObjectEditor::selectionChanged() {
	// Apply group transforms
	for(SceneNode* n: m_selectGroup->children()) n->setTransform(n->getDerivedTransform());
	m_selectGroup->setTransform(vec3(), Quaternion());

	std::vector<Object*> selectedObjects;
	selectedObjects.reserve(m_objectList->getSelectionSize());
	for(ListItem& i: m_objectList->selectedItems()) selectedObjects.push_back(getObject(i));


	SceneNode* gizmoRoot;
	if(selectedObjects.size() == 1) {
		// Single selection uses object directly
		while(m_selectGroup->getChildCount()) m_node->addChild(m_selectGroup->getChild((size_t)0));
		gizmoRoot = selectedObjects[0];
	}
	else {
		// Multi-selection puts everything in a sub-node
		vec3 centre;
		std::set<SceneNode*> removed(m_selectGroup->children().begin(), m_selectGroup->children().end());
		for(Object* o: selectedObjects) {
			centre += vec3(&o->getDerivedTransform()[12]);
			removed.erase(o);
			m_selectGroup->addChild(o);
		}
		centre /= selectedObjects.size();
		for(SceneNode* n: removed) m_node->addChild(n);
		for(Object* o: selectedObjects) o->move(-centre);
		m_selectGroup->setPosition(centre);
		gizmoRoot = m_selectGroup;
	}

	// Use render queue for selection highlight
	setRenderQueue(m_node, 0);
	for(Object* o: selectedObjects) setRenderQueue(o, 12);

	// Set up gizmo
	if(selectedObjects.empty()) m_gizmo->setVisible(false);
	else {
		m_gizmo->setVisible(true);
		m_gizmo->setPosition(gizmoRoot->getPosition());
		m_gizmo->setOrientation(gizmoRoot->getOrientation());
		m_gizmo->setScale(gizmoRoot->getScale());
		if(m_gizmo->isLocal()) m_gizmo->setLocalMode();
	}
}

bool ObjectEditor::isSelected(SceneNode* obj) const {
	for(const ListItem& i: m_objectList->selectedItems()) {
		if(getObject(i)==obj) return true;
	}
	return false;
}

void ObjectEditor::clearSelection() {
	m_objectList->clearSelection();
	selectionChanged();
}

Object* ObjectEditor::pick(SceneNode* node, const Ray& ray, bool ignoreSelection, float& t) const {
	if(ignoreSelection && isSelected(node)) return 0;
	if(!node->isVisible()) return 0;

	Object* obj = dynamic_cast<Object*>(node);
	if(obj) {
		float dist = t;
		const BoundingBox& box = obj->getBounds();
		if(!base::intersectRayAABB(ray.start, ray.direction, box.centre(), box.size()/2, dist)) return 0;
	}

	bool hit = false;
	for(Drawable* d: node->attachments()) {
		// Test drawables
		float dist = 0;
		const BoundingBox& box = d->getBounds();
		if(base::intersectRayAABB(ray.start, ray.direction, box.centre(), box.size()/2, dist)) {
			dist = t;
			if(dynamic_cast<DrawableMesh*>(d) && pickMesh(ray, static_cast<DrawableMesh*>(d)->getMesh(), node->getDerivedTransform(), dist)) {
				hit = true;
				t = dist;
			}
		}
	}

	Object* result = 0;
	if(hit) {
		for(SceneNode* n=node->getParent(); !obj && n; n=n->getParent()) obj = dynamic_cast<Object*>(n);
		result = obj;
	}
	for(SceneNode* n: node->children()) {
		Object* r = pick(n, ray, ignoreSelection, t);
		if(r) result = r;
	}
	return result;
}

bool ObjectEditor::pickMesh(const Ray& ray, const Mesh* mesh, const Matrix& transform, float& t) {
	const vec3 start = transform.untransform(ray.start);
	const vec3 direction = transform.unrotate(ray.direction);
	const vec3 end = start + direction;
	size_t count = mesh->getIndexCount();
	float r;
	bool result = false;
	for(uint i=0; i<count; i+=3) {
		if(base::intersectLineTriangle(start, end, mesh->getIndexedVertex(i), mesh->getIndexedVertex(i+1), mesh->getIndexedVertex(i+2), r) && r<t) {
			result = true;
			t = r;
		}
	}
	return result;
}


// ------------------------------------------------------------------------------ //

static const char* shaderSourceVS =
"#version 150\n"
"in vec4 vertex;\nin vec3 normal;\nin vec2 texCoord;\nin vec4 tangent;\nin vec4 vcolour;\n"
"uniform mat4 transform;\nuniform mat4 modelMatrix;\n"
"out vec2 texcoord;\nout vec3 worldNormal;\nout vec3 worldPos;\nout vec4 colour;\nout vec3 worldTangent;\nout vec3 worldBinormal;\n"
"void main() { gl_Position=transform*vertex; texcoord=texCoord; worldNormal=mat3(modelMatrix)*normal; worldPos=(modelMatrix * vertex).xyz;"
"worldTangent=mat3(modelMatrix)*tangent.xyz;\nworldBinormal=cross(worldNormal,worldTangent)*tangent.w; colour=vcolour; }\n";
static const char* shaderSourceFS =
"#version 150\n"
"in vec2 texcoord;\nin vec3 worldNormal;\nin vec3 worldPos;\nout vec4 fragment;\nin vec4 colour;\n"
"in vec3 worldTangent;\nin vec3 worldBinormal;\n"
"uniform sampler2D diffusemap;\nuniform sampler2D normalmap;\n\nuniform vec3 lightDirection;\n"
"void main() { vec4 diff=texture2D(diffusemap, texcoord.st);\n"
"#ifdef COLOUR\ndiff *= colour;\n#endif\n"
"if(diff.a < 0.5) discard;\n"
"#ifdef NORMALMAP\nvec3 norm=texture2D(normalmap, texcoord.st).xyz*2.0-1.0;\n"
"vec3 normal = mat3(normalize(worldTangent), normalize(worldBinormal), normalize(worldNormal)) * norm;\n"
"#else\nvec3 normal=normalize(worldNormal);\n#endif\n"
"float l = dot(normal, normalize(lightDirection));\n"
"float s = (l+1)/1.3*0.2+0.1;\n"
"diff.rgb *= max(s, l);\n"
"fragment = vec4(diff.rgb, 1.0); }";
static const char* shaderSourceSelectedFS = 
"#version 150\nout vec4 fragment;\nvoid main() { fragment = vec4(1,1,1,1); }\n";

bool findFile(char* path, const char* name, int limit=3) {
	int o = strlen(path);
	strcpy(path + o++, "/");
	base::Directory dir(path);
	for(const auto& file: dir) {
		if(strcmp(file.name, name)==0) {
			strcpy(path+o, file.name);
			return true;
		}
		else if(file.type == base::Directory::DIRECTORY && limit>0) {
			if(strcmp(file.name, ".")==0 || strcmp(file.name, "..")==0) continue;
			strcpy(path+o, file.name);
			if(findFile(path, name, limit-1)) return true;
		}
	}
	return false;
}

void ObjectEditor::setupMaterials() {
	char name[32];
	uint data = 0xffffffff;
	uint dataN = 0xff8080;
	base::Texture* white = new base::Texture(base::Texture::create(1, 1, base::Texture::RGB8, &data));
	base::Texture* flat =  new base::Texture(base::Texture::create(1, 1, base::Texture::RGB8, &dataN));
	char log[2048];

	base::ShaderPart* vs = new base::ShaderPart(base::VERTEX_SHADER, shaderSourceVS);
	
	// Selected material
	base::ShaderPart* fs = new base::ShaderPart(base::FRAGMENT_SHADER, shaderSourceSelectedFS);
	base::Shader* selectShader = new base::Shader();
	selectShader->attach(vs);
	selectShader->attach(fs);

	for(int i=0; i<4; ++i) {
		base::ShaderPart* fs = new base::ShaderPart(base::FRAGMENT_SHADER, shaderSourceFS);
		if(i&1) fs->define("NORMALMAP");
		if(i&2) fs->define("COLOUR");
		base::Shader* shader = new base::Shader();
		shader->attach(vs);
		shader->attach(fs);
		shader->bindAttributeLocation("vertex",  0);
		shader->bindAttributeLocation("normal",  1);
		shader->bindAttributeLocation("tangent", 2);
		shader->bindAttributeLocation("texCoord",3);
		shader->bindAttributeLocation("vcolour", 4);

		base::Material* material = new base::Material;
		base::Pass* pass = material->addPass("default");
		pass->setShader(shader);
		pass->getParameters().set("lightDirection", vec3(1,1,1));
		pass->getParameters().setAuto("transform", base::AUTO_MODEL_VIEW_PROJECTION_MATRIX);
		pass->getParameters().setAuto("modelMatrix", base::AUTO_MODEL_MATRIX);
		pass->setTexture("diffusemap", white);
		if(i&1) pass->setTexture("normalmap", flat);

		pass->compile();
		if(pass->getShader()->getLog(log, sizeof(log)))	printf(log);
		if(!pass->getShader()->isCompiled()) printf("Failed\n");

		pass = material->addPass("selected");
		pass->setShader(selectShader);
		pass->getParameters().setAuto("transform", base::AUTO_MODEL_VIEW_PROJECTION_MATRIX);
		pass->state.wireframe = true;
		pass->state.cullMode = base::CULL_FRONT;
		pass->compile();
		if(pass->getShader()->getLog(log, sizeof(log)))	printf(log);

		sprintf(name, "default|%d", i);
		m_materials[name] = material;
	}
}

base::Texture* ObjectEditor::findTexture(const char* name, const char* suffix, int limit) {
	char path[2048];
	char file[256];
	strcpy(path, m_fileSystem->getRootPath());
	sprintf(file, "%s%s", name, suffix);
	if(findFile(path, file, limit)) {
		base::PNG png = base::PNG::load(path);
		if(png.data) {
			return new base::Texture(base::Texture::create(png.width, png.height, png.bpp/8, png.data));
		}
	}
	return 0;
}

base::Material* ObjectEditor::getMaterial(const char* name, bool nmap, bool col) {
	int code = (nmap?1:0) | (col?2:0);
	char key[256];
	sprintf(key, "%s|%d", name&&*name?name: "default", code);
	base::Material* material = m_materials.get(key, 0);
	if(material) return material;

	material = getMaterial(0, nmap, col);
	base::Texture* diffuse = findTexture(name, ".png");
	base::Texture* normal = 0;
	if(nmap) {
		normal = findTexture(name, "_n.png");
		if(!normal) normal = findTexture(name, "_normal.png");
	}

	if(diffuse || normal) {
		material = material->clone();
		if(diffuse) material->getPass(0)->setTexture("diffusemap", diffuse);
		if(normal) material->getPass(0)->setTexture("normalmap", normal);
		material->getPass(0)->compile();
		material->getPass(1)->compile();
	}

	m_materials[key] = material;
	return material;
}

Object* ObjectEditor::createObject(const char* name, Model* model, Mesh* mesh, const char* material) {
	if(!model && !mesh) return 0;
	
	auto createDrawable = [&](Mesh* mesh) {
		bool hasTangents = mesh->getVertexBuffer()->attributes.hasAttrribute(base::VA_TANGENT);
		bool hasColour = mesh->getVertexBuffer()->attributes.hasAttrribute(base::VA_COLOUR);
		mesh->getVertexBuffer()->createBuffer();
		mesh->getIndexBuffer()->createBuffer();
		DrawableMesh* d = new DrawableMesh(mesh);
		d->setMaterial(getMaterial(material, hasTangents, hasColour));
		return d;
	};
	auto calculateMeshBounds = [](Mesh* m) { if(m->getBounds().isEmpty()) m->calculateBounds(); };

	Object* object = new Object();
	object->setName(name);
	m_node->addChild(object);

	if(model) {
		if(LayoutExtension* layout = model->getExtension<LayoutExtension>()) {
			vec3 pos, scale(1);
			Quaternion rot;
			int count=0;
			for(const LayoutExtension::Node* n: *layout) {
				SceneNode* node = new SceneNode(n->name);
				// Get all meshes with matching name
				if(n->mesh) for(int i=0; i<model->getMeshCount(); ++i) {
					if(strcmp(n->mesh, model->getMeshName(i))==0) {
						node->attach(createDrawable(model->getMesh(i)));
						calculateMeshBounds(model->getMesh(i));
						++count;
					}
				}
				LayoutExtension::getDerivedTransform(n, pos, rot);
				node->setTransform(pos, rot, scale);
				object->addChild(node);
			}
			printf("Created model %s (%d)\n", name, count);
		}
		else {
			printf("Created model %s (%d)\n", name, model->getMeshCount());
			for(int i=0; i<model->getMeshCount(); ++i) {
				object->attach(createDrawable(model->getMesh(i)));
				calculateMeshBounds(model->getMesh(i));
			}
		}
	}
	else if(mesh) {
		printf("Created single mesh %s\n", name);
		object->attach(createDrawable(mesh) );
		calculateMeshBounds(mesh);
	}
	return object;
}


void ObjectEditor::selectResource(TreeView*, TreeNode* resource) {
	if(m_mode!=CHAIN) selectObject(0);
	m_resource = resource;
	if(m_placement) cancelPlacement();
	if(resource) {
		const char* name = resource->getText();
		Model* model = resource->getData(1).getValue<Model*>(0);
		Mesh* mesh = resource->getData(1).getValue<Mesh*>(0);
		const char* material = resource->getText(3);
		m_placement = createObject(name, model, mesh, material);
	}
}

TreeNode* ObjectEditor::addModel(const char* path, const char* name) {
	Model* model = base::BMLoader::load(path);
	if(!model || model->getMeshCount()==0) return 0;
	TreeNode* node = new TreeNode(name);
	node->setData(1, model);
	node->setData(2, String(path));
	node->setData(3, model->getMaterialName(0));
	for(int i=0; i<model->getMeshCount(); ++i) model->getMesh(i)->calculateBounds();
	if(model->getMeshCount()>1) {
		// Allow placing individual meshes
		for(int i=0; i<model->getMeshCount(); ++i) {
			TreeNode* mesh = new TreeNode(model->getMeshName(i));
			mesh->setData(1, model->getMesh(i));
			mesh->setData(3, model->getMaterialName(i));
			node->add(mesh);
		}
	}
	return node;
}

TreeNode* ObjectEditor::addFolder(const char* path, const char* name) {
	char fullPath[1024];
	int o = sprintf(fullPath, "%s/", path);
	base::Directory dir(path);
	TreeNode* folder = 0;
	for(const auto& file: dir) {
		if(file.type == base::Directory::FILE && strcmp(file.name+file.ext, "bm")==0) {
			strcpy(fullPath+o, file.name);
			TreeNode* node = addModel(fullPath, file.name);
			if(node) {
				if(!folder) folder = new TreeNode(name);
				folder->add(node);
			}
		}
		else if(file.type == base::Directory::DIRECTORY) {
			if(strcmp(file.name, ".")==0 || strcmp(file.name, "..")==0) continue;

			strcpy(fullPath+o, file.name);
			TreeNode* node = addFolder(fullPath, file.name);
			if(node) {
				if(!folder) folder = new TreeNode(name);
				folder->add(node);
			}
		}
	}
	return folder;
}

void ObjectEditor::setResourcePath(const char* root) {
	char temp[1024];
	if(base::Directory::isRelative(root)) {
		base::Directory::getFullPath(root, temp, 1024);
		root = temp;
	}

	m_panel->getWidget<Textbox>("path")->setText(root);
	TreeNode* node = addFolder(root, "root");
	if(node) m_resourceList->setRootNode(node);
	else m_resourceList->setRootNode(new TreeNode(root));
	m_resourceList->getRootNode()->expandAll();
}


// ================================================================== //

void Object::updateBounds() {
	getDerivedTransformUpdated();
	m_bounds.setInvalid();
	for(Drawable* d: attachments()) {
		d->updateBounds();
		m_bounds.include(d->getBounds());
	}
	for(SceneNode* n : children()) {
		n->getDerivedTransformUpdated();
		for(Drawable* d: n->attachments()) {
			d->updateBounds();
			m_bounds.include(d->getBounds());
		}
	}
}



