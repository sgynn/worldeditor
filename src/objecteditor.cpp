#include "objecteditor.h"
#include "heightmap.h"
#include "filesystem.h"
#include "gizmo.h"
#include "boxselect.h"
#include "gui/widgets.h"
#include "gui/lists.h"
#include "gui/tree.h"
#include "scene/scene.h"
#include "scene/mesh.h"
#include <base/xml.h>
#include <base/collision.h>
#include <base/directory.h>

#include "model/model.h"
#include "model/bmloader.h"
#include "scene/shader.h"
#include "scene/material.h"
#include "scene/autovariables.h"
#include <base/texture.h>
#include <base/game.h>
#include <base/input.h>
#include <base/png.h>
#include <set>


using base::XMLElement;
using namespace gui;
using base::bmodel::Model;
using base::bmodel::Mesh;
using scene::SceneNode;
using scene::Drawable;
using scene::DrawableMesh;

extern gui::String appPath;

#define CONNECT(Type, name, event, callback) { Type* t=m_panel->getWidget<Type>(name); if(t) t->event.bind(this, &ObjectEditor::callback); else printf("Missing widget: %s\n", name); }

ObjectEditor::ObjectEditor(gui::Root* gui, FileSystem* fs, MapGrid* terrain, SceneNode* scene)
	: m_fileSystem(fs), m_terrain(terrain), m_placement(0), m_mode(SINGLE), m_gizmo(0)
	, m_resource(0)
{
	m_panel = gui->getWidget("objecteditor");
	if(!m_panel) {
		gui->load(appPath + "data/objects.xml");
		m_panel = gui->getWidget("objecteditor");
	}
	m_panel->setVisible(false);

	m_node = scene->createChild("Objects");
	m_sceneTree = m_panel->getWidget<TreeView>("tree");
	m_resourceList = m_panel->getWidget<TreeView>("resources");
	
	m_toolButton = gui->createWidget<Button>("iconbuttondark");
	m_toolButton->eventPressed.bind(this, &ObjectEditor::toggleEditor);
	m_toolButton->setIcon("Objects");

	CONNECT(Textbox,  "path", eventSubmit, changePath);
	CONNECT(TreeView, "tree", eventSelected, selectObject);
	CONNECT(TreeView, "resources", eventSelected, selectResource);

	m_gizmo = new editor::Gizmo();
	m_gizmo->setRenderQueue(5);
	scene->attach(m_gizmo);
	m_gizmo->setVisible(false);
	m_selectGroup = m_node->createChild("Selection");

	m_box = new BoxSelect();
	m_panel->getParent()->add(m_box);

	setResourcePath(fs->getRootPath());
}

ObjectEditor::~ObjectEditor() {
	m_toolButton->removeFromParent();
	delete m_toolButton;
}

void ObjectEditor::setup(gui::Widget* toolPanel) {
	toolPanel->add(m_toolButton, 1);
}

void ObjectEditor::close() {
	m_toolButton->setSelected(false);
	m_panel->setVisible(false);
	cancelPlacement();
	selectObject(0);
}

void ObjectEditor::toggleEditor(gui::Button*) {
	m_panel->setVisible(!m_panel->isVisible());
	m_toolButton->setSelected(m_panel->isVisible());
}

// ------------------------------------------------------------------------------ //

void ObjectEditor::load(const XMLElement& e, const TerrainMap* context) {
	const XMLElement& list = e.find("objects");
	for(const XMLElement& i: list) {
		if(i == "object") {
			Quaternion rot;
			vec3 pos, scale(1,1,1);
			const char* name = i.attribute("name");
			const char* file = i.attribute("file");
			int mesh = i.attribute("mesh", 0);
			sscanf(i.attribute("scale"), "%g %g %g", &scale.x, &scale.y, &scale.z);
			sscanf(i.attribute("position"), "%g %g %g", &pos.x, &pos.y, &pos.z);
			sscanf(i.attribute("orientation"), "%g %g %g %g", &rot.w, &rot.x, &rot.y, &rot.z);
			
			String f = m_fileSystem->getFile(file);
			Model* model = base::bmodel::BMLoader::load(f);
			if(model && mesh < model->getMeshCount()) {
				Object* o = new Object();
				model->getMesh(mesh)->calculateBounds();
				DrawableMesh* d = new DrawableMesh(model->getMesh(mesh), createMaterial(model->getMaterialName(mesh)));
				m_node->addChild(o);
				o->setName(name);
				o->attach(d);
				o->setTransform(pos, rot, scale);
				o->getDerivedTransformUpdated();
				d->updateBounds();

				TreeNode* node = new TreeNode(name);
				node->setData(1, o);
				node->setData(2, f);
				node->setData(3, mesh);
				m_sceneTree->getRootNode()->add(node);
			}
		}
	}
}

XMLElement ObjectEditor::save(const TerrainMap* context) const {
	char buffer[256];
	XMLElement xml("objects");
	for(TreeNode* n: *m_sceneTree->getRootNode()) {
		Object* object = n->getData(1).getValue<Object*>(0);
		if(!object) continue;
		const vec3 scl = object->getScale();
		const vec3& pos = object->getPosition();
		const Quaternion& rot = object->getOrientation();
		int mesh = n->getData(3).getValue(-1);
		

		XMLElement& e = xml.add("object");
		e.setAttribute("name", n->getText());
		e.setAttribute("file", m_fileSystem->getRelative(n->getText(2)));
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
	}

	return xml;
}

void ObjectEditor::clear() {
	m_node->deleteChildren(true);
	m_sceneTree->getRootNode()->clear();
}

// ------------------------------------------------------------------------------ //

template<class T> TreeNode* findTreeNode(TreeNode* node, const T& predicate) {
	if(predicate(node)) return node;
	else for(TreeNode* n: *node) {
		TreeNode* found = findTreeNode(n, predicate);
		if(found) return found;
	}
	return 0;
}

void ObjectEditor::changePath(Textbox* t) {
	setResourcePath(t->getText());
}

void ObjectEditor::update(const Mouse& mouse, const Ray& ray, int keyMask, base::Camera* camera) {
	if(!m_panel->isVisible()) return;
	bool overGUI = m_panel->getRoot()->getWidgetUnderMouse()!=m_panel->getRoot()->getRootWidget();

	// Placement update
	if(m_placement) { 
		float t;
		
		// Altitude
		if(mouse.wheel) {
			if(keyMask&ALT_MASK && m_mode==CHAIN) m_chainStart += m_placement->getOrientation()*m_chainStep.normalised()*0.1*mouse.wheel;
			else {
				m_altitude.y += mouse.wheel * 0.1;
				if(keyMask&SHIFT_MASK) m_chainStart.y += mouse.wheel * 0.1;
			}
		}


		// Position
		if(m_mode == SINGLE) {
			if(m_terrain->castRay(ray.start, ray.direction, t)) {
				m_placement->setPosition(ray.point(t) + m_altitude);
				m_placement->setVisible(true);
			}
			else m_placement->setVisible(false);
		}


		// Rotation
		if(m_mode==SINGLE && mouse.pressed==1 && !overGUI) {
			m_pressed = mouse.position;
			m_mode = ROTATE;
			m_started = false;
			m_yawOffset = 0;
		}
		else if(m_mode == ROTATE) {
			vec3 pt;
			const vec3& ps = m_placement->getPosition();
			if(base::intersectRayPlane(ray.start, ray.direction, vec3(0,1,0), ps.y, pt)) {
				if(!m_started && abs(mouse.position.x-m_pressed.x)+abs(mouse.position.y-m_pressed.y)>5) {
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
		if(m_mode == CHAIN && m_terrain->castRay(ray.start, ray.direction, t)) {
			if(!m_started) {
				m_chainStart = ray.point(t) + m_altitude;
				m_placement->setPosition(m_chainStart - m_placement->getOrientation() * m_chainStep * 0.5);
				if(!overGUI && mouse.released==1) { m_started = true; return; }
			}
			else {
				vec3 end = ray.point(t) + m_altitude;
				float distance = m_chainStart.distance(end);
				float step = m_chainStep.length();
				size_t count = ceil(distance / step);
				if(distance > 0 && step>0) {
					for(size_t i=1; i<m_selected.size(); ++i) m_selected[i]->setVisible(i<count);
					for(size_t i=m_selected.size(); i<count; ++i) {
						m_placement = 0;
						selectResource(0, m_resource);
						m_selected.push_back(m_placement);
					}
					m_placement = m_selected[0];

					vec3 z = (end - m_chainStart) / distance;
					vec3 x = vec3(0,1,0).cross(z);
					vec3 y = z.cross(x);
					Quaternion rot(Matrix(x,y,z));
					rot *= Quaternion::arc(vec3(0,0,1), m_chainStep);
					for(size_t i=0; i<m_selected.size(); ++i) {
						m_selected[i]->setPosition(m_chainStart + z * step * (i+0.5));
						m_selected[i]->setOrientation(rot);
					}
				}
			}
		}
		if(m_mode==SINGLE && base::Game::Pressed(base::KEY_C)) {
			Quaternion tmp = m_placement->getOrientation();
			m_placement->setOrientation(Quaternion());
			updateObjectBounds(m_placement);
			m_placement->setOrientation(tmp);
			m_chainStep = m_placement->getAttachment(0)->getBounds().size();
			m_chainStep.y = 0;
			if(m_chainStep.x>m_chainStep.z) m_chainStep.z=0; else m_chainStep.x=0;
			if(m_chainStep.length2()>0) {
				m_selected.clear();
				m_selected.push_back(m_placement);
				m_mode = CHAIN;
				m_started = false;
			}
		}
		else if(m_mode == CHAIN && base::Game::Pressed(base::KEY_C)) {
			Quaternion tmp = m_placement->getOrientation();
			tmp.x = tmp.z = 0;
			tmp.normalise();
			m_placement->setOrientation(tmp);
			for(size_t i=1; i<m_selected.size(); ++i) delete m_selected[i];
			m_selected.clear();
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
				if(!overGUI) {
					placeObject(m_placement, m_resource);
					selectObject(m_placement);
					m_placement = 0;
					if(keyMask&SHIFT_MASK) selectResource(0, m_resource);
					else cancelPlacement();
				}
				break;

			case CHAIN:
				if(!overGUI) {
					int count = 0;
					float step = m_chainStep.length();
					for(Object* o: m_selected) {
						if(!o->isVisible()) break;
						placeObject(o, m_resource);
						++count;
					}
					m_selected.clear();
					selectResource(0, m_resource);
					m_selected.push_back(m_placement);
					m_chainStart += (ray.point(t)+m_altitude-m_chainStart).normalise() * (count*step);
				}
				break;
			}
		}

		if(mouse.pressed&4) { cancelPlacement(); m_mode=SINGLE; m_altitude.set(0,0,0); }
		return;
	}

	// Edit selected objects
	if(m_gizmo->isVisible()) {

		// Delete selection
		if(base::Game::Pressed(base::KEY_DEL)) {
			for(Object* obj: m_selected) {
				TreeNode* node = findTreeNode(m_sceneTree->getRootNode(), [obj](TreeNode* n){return n->getData(1)==obj;});
				node->getParent()->remove(node);
				delete node;
				delete obj;
			}
			m_gizmo->setVisible(false);
			m_selected.clear();
			return;
		}

		// Gizmo update
		bool wasHeld = m_gizmo->isHeld();
		editor::MouseRay mouseRay(camera, mouse.position.x, mouse.position.y, base::Game::width(), base::Game::height());
		if(mouse.released&1) m_gizmo->onMouseUp();
		if(mouse.pressed&1) m_gizmo->onMouseDown(mouseRay);
		if(mouse.moved.x || mouse.moved.y) {
			m_gizmo->onMouseMove(mouseRay);
			if(m_gizmo->isHeld()) {
				SceneNode* target = m_selected.size()==1? m_selected[0]: m_selectGroup;
				target->setTransform(m_gizmo->getPosition(), m_gizmo->getOrientation(), m_gizmo->getScale());
				for(Object* obj: m_selected) updateObjectBounds(obj);
			}
		}
		if(wasHeld) return;

		// Additional functions (buttons)
		if(!m_gizmo->isHeld()) {
			if(base::Game::Pressed(base::KEY_1)) m_gizmo->setMode(editor::Gizmo::POSITION);
			if(base::Game::Pressed(base::KEY_2)) m_gizmo->setMode(editor::Gizmo::ORIENTATION);
			if(base::Game::Pressed(base::KEY_3)) m_gizmo->setMode(editor::Gizmo::SCALE);
			if(base::Game::Pressed(base::KEY_4)) m_gizmo->setLocalMode();
			if(base::Game::Pressed(base::KEY_5)) m_gizmo->setRelative(Matrix());
			if(base::Game::Pressed(base::KEY_G)) {
				vec3 pos = m_selected[0]->getPosition();
				pos.y = m_terrain->getHeight(pos);
				m_selected[0]->setPosition(pos);
				m_gizmo->setPosition(pos);
				updateObjectBounds(m_selected[0]);
			}
		}
	}

	// Picking
	if(!m_placement && !m_gizmo->isHeld()) {
		if(mouse.pressed==1) m_box->start();
		else if(mouse.released&1) {
			if(m_box->isValid()) {
				m_box->updatePlanes(camera);
				for(SceneNode* node: m_node->children()) {
					for(Drawable* d: node->attachments()) {
						if(m_box->inside(d->getBounds())) selectObject(static_cast<Object*>(node), true);
					}
				}
				m_box->clear();
			}
			else {
				float t = 1e8f;
				Object* sel = pick(m_node, ray, t);
				selectObject(sel, keyMask&SHIFT_MASK);
			}
		}
	}
}

void ObjectEditor::cancelPlacement() {
	if(m_mode == CHAIN) for(Object* o: m_selected) delete o;
	else delete m_placement;
	m_selected.clear();
	m_placement = 0;
	m_resourceList->clearSelection();
	if(m_mode == ROTATE) m_mode = SINGLE;
}

void ObjectEditor::placeObject(Object* object, TreeNode* data) {
	TreeNode* node = new TreeNode(object->getName());
	node->setData(1, object);
	if(m_resource->getText(2)) node->setData(2, data->getData(2));
	else {
		node->setData(2, data->getParent()->getData(2));
		node->setData(3, data->getIndex());
	}
	m_sceneTree->getRootNode()->add(node);
	updateObjectBounds(object);
}

void ObjectEditor::selectObject(TreeView*, TreeNode* node) {
	selectObject(node? node->getData(1).getValue<Object*>(0): 0);
}

void ObjectEditor::updateObjectBounds(Object* obj) {
	obj->getDerivedTransformUpdated();
	for(Drawable* d: obj->attachments()) d->updateBounds();
}

void ObjectEditor::selectObject(Object* obj, bool append) {
	printf("Select object %s\n", obj? obj->getName(): "none");
	if(!obj && !append) clearSelection();
	if(!obj || isSelected(obj)) return;
	if(!append) clearSelection();
	m_selected.push_back(obj);

	// select tree node
	TreeNode* node = findTreeNode(m_sceneTree->getRootNode(), [obj](TreeNode* n){return n->getData(1)==obj;});
	if(node) node->select();

	selectionChanged();
}

void ObjectEditor::selectionChanged() {
	// Apply group transforms
	for(SceneNode* n: m_selectGroup->children()) n->setTransform(n->getDerivedTransform());
	m_selectGroup->setTransform(vec3(), Quaternion());

	SceneNode* gizmoRoot;
	if(m_selected.size() == 1) {
		// Single selection uses object directly
		while(m_selectGroup->getChildCount()) m_node->addChild(m_selectGroup->getChild((size_t)0));
		gizmoRoot = m_selected[0];
	}
	else {
		// Multi-selection puts everything in a sub-node
		vec3 centre;
		std::set<SceneNode*> removed(m_selectGroup->children().begin(), m_selectGroup->children().end());
		for(Object* o: m_selected) {
			centre += vec3(&o->getDerivedTransform()[12]);
			removed.erase(o);
			m_selectGroup->addChild(o);
		}
		centre /= m_selected.size();
		for(SceneNode* n: removed) m_node->addChild(n);
		for(Object* o: m_selected) o->move(-centre);
		m_selectGroup->setPosition(centre);
		gizmoRoot = m_selectGroup;
	}

	// Set up gizmo
	if(m_selected.empty()) m_gizmo->setVisible(false);
	else {
		m_gizmo->setVisible(true);
		m_gizmo->setPosition(gizmoRoot->getPosition());
		m_gizmo->setOrientation(gizmoRoot->getOrientation());
		m_gizmo->setScale(gizmoRoot->getScale());
		if(m_gizmo->isLocal()) m_gizmo->setLocalMode();
	}
}

bool ObjectEditor::isSelected(Object* obj) const {
	for(Object* o: m_selected) if(o==obj) return true;
	return false;
}

void ObjectEditor::clearSelection() {
	m_selected.clear();
	selectionChanged();
}

Object* ObjectEditor::pick(SceneNode* node, const Ray& ray, float& t) const {
	Object* result = 0;
	for(Drawable* d: node->attachments()) {
		// Test drawables
		float dist = 0;
		const BoundingBox& box = d->getBounds();
		if(base::intersectRayAABB(ray.start, ray.direction, box.centre(), box.size()/2, dist) && dist<t) {
			// ToDo: trace individual polygons
			result = dynamic_cast<Object*>(node);
			t = dist;
		}
	}
	for(SceneNode* n: node->children()) {
		Object* r = pick(n, ray, t);
		if(r) result = r;
	}
	return result;
}

// ------------------------------------------------------------------------------ //

static const char* shaderSourceVS =
"#version 150\n"
"in vec4 vertex;\nin vec3 normal;\nin vec2 texCoord;\n"
"uniform mat4 transform;\nuniform mat4 modelMatrix;\n"
"out vec2 texcoord;\nout vec3 worldNormal;\nout vec3 worldPos;\n"
"void main() { gl_Position=transform*vertex; texcoord=texCoord; worldNormal=mat3(modelMatrix)*normal; worldPos=(modelMatrix * vertex).xyz; }\n";
static const char* shaderSourceFS =
"#version 150\n"
"in vec2 texcoord;\nin vec3 worldNormal;\nin vec3 worldPos;\nout vec4 fragment;\n"
"uniform sampler2D diffuse;\nuniform vec3 lightDirection;\n"
"void main() { vec4 diff=texture2D(diffuse, texcoord.st);\n"
"if(diff.a < 0.5) discard;\n"
"float l = dot(normalize(worldNormal), normalize(lightDirection));\n"
"float s = (l+1)/1.3*0.2+0.1;\n"
"fragment = vec4(diff.rgb * max(s,l), 1.0); }";

bool findFile(char* path, const char* name, int limit=3) {
	int o = strlen(path);
	path[o++] = '/';
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

scene::Material* ObjectEditor::createMaterial(const char* name) {
	if(!name || !name[0]) name = "default";
	scene::Material* material = m_materials.get(name, 0);
	if(material) return material;

	static scene::Shader* shader = 0;
	if(!shader) {
		scene::ShaderPart* vs = new scene::ShaderPart(scene::VERTEX_SHADER, shaderSourceVS);
		scene::ShaderPart* fs = new scene::ShaderPart(scene::FRAGMENT_SHADER, shaderSourceFS);
		shader = new scene::Shader();
		shader->attach(vs);
		shader->attach(fs);
		shader->bindAttributeLocation("vertex",  0);
		shader->bindAttributeLocation("normal",  1);
		shader->bindAttributeLocation("texCoord",3);
	}

	material = new scene::Material;
	scene::Pass* pass = material->addPass("default");
	pass->setShader(shader);
	pass->getParameters().set("lightDirection", vec3(1,1,1));
	pass->getParameters().setAuto("transform", scene::AUTO_MODEL_VIEW_PROJECTION_MATRIX);
	pass->getParameters().setAuto("modelMatrix", scene::AUTO_MODEL_MATRIX);
	
	// Find texture
	bool foundTexture = false;
	char fullName[128];
	char path[2048];
	sprintf(fullName, "%s.png", name);
	strcpy(path, m_fileSystem->getRootPath());
	if(findFile(path, fullName)) {
		base::PNG png = base::PNG::load(path);
		if(png.data) {
			base::Texture tex = base::Texture::create(png.width, png.height, png.bpp/8, png.data);
			pass->setTexture("diffuse", new base::Texture(tex));
			foundTexture = true;
		}
	}

	// Fallback texture
	if(!foundTexture) {
		static base::Texture* defaultTexture = 0;
		if(!defaultTexture) {
			uint data = 0xffffffff;
			defaultTexture = new base::Texture(base::Texture::create(1, 1, base::Texture::R8, &data));
		}
		pass->setTexture("diffuse", defaultTexture);
	}

	pass->compile();
	char buffer[2048]; buffer[0] = 0;
	pass->getShader()->getLog(buffer, sizeof(buffer));
	printf(buffer);
	if(!pass->getShader()->isCompiled()) printf("Failed\n");

	m_materials[name] = material;
	return material;
}


void ObjectEditor::selectResource(TreeView*, TreeNode* resource) {
	if(m_mode!=CHAIN) selectObject(0);
	m_resource = resource;
	if(m_placement) cancelPlacement();
	if(resource) {
		const char* name = resource->getText();
		Model* model = resource->getData(1).getValue<Model*>(0);
		Mesh* mesh = resource->getData(1).getValue<Mesh*>(0);
		if(model) mesh = model->getMesh(0);

		if(mesh) {
			printf("Creating object %s\n", name);
			DrawableMesh* d = new DrawableMesh(mesh);
			d->setMaterial(createMaterial(resource->getText(3)));
			m_placement = new Object();
			m_placement->setName(name);
			m_placement->attach(d);
			m_node->addChild(m_placement);
		}
	}
}

TreeNode* ObjectEditor::addModel(const char* path, const char* name) {
	Model* model = base::bmodel::BMLoader::load(path);
	if(!model) return 0;
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
			if(!folder) folder = new TreeNode(name);
			folder->add(node);
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




