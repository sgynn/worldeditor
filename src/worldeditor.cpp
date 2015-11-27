#include "worldeditor.h"
#include <base/game.h>
#include <base/input.h>
#include <base/fpscamera.h>
#include <base/png.h>
#include <base/directory.h>

#include <base/inifile.h>

#include "resource/file.h"
#include "simple/heightmap.h"
#include "streaming/streamer.h"
#include "streaming/texturestream.h"


#include "gui/skin.h"
#include "gui/font.h"
#include "gui/widgets.h"
#include "gui/lists.h"
#include "widgets/filedialog.h"
#include "widgets/toolbutton.h"

#include <cstdarg>

#include "terraineditor/heighttools.h"
#include "terraineditor/texturetools.h"

using namespace gui;
using namespace base;


// Application entry point
int main(int argc, char* argv[]) {
	base::INIFile cfg = base::INIFile::load("settings.cfg");
	base::INIFile::Section wnd = cfg["window"];

	// System options
	int w = wnd.get("width", 1280);
	int h = wnd.get("height", 1024);
	int aa = wnd.get("antialiasing", 0);
	bool fs = wnd.get("fullscreen", false);


	base::Game* game = base::Game::create(w,h,32,fs,aa);
	game->setInitialState( new WorldEditor() );
	game->run();

	delete game;
	return 0;
}


/** Make a world - this file will be a complete mess while I test stuff */

WorldEditor::WorldEditor() : m_editor(0), m_heightMap(0) {
	m_renderer = new Render;
	m_library = new Library;
	m_library->addPath("data");

	// Editor options - ToDo: load from cfg file
	m_options.speed = 10;
	m_options.lod = 4;
	m_options.collide = true;

	// Run an fps camera for now
	base::FPSCamera* cam = new base::FPSCamera(90, base::Game::aspect(), 0.01, 1000);
	cam->setSpeed(1, 0.004);
	cam->setEnabled(false);
	cam->lookat( vec3(10, 50, 10), vec3(100,0,100));
	m_camera = cam;

	// Set up gui
	Root::registerClass<FileDialog>();
	Root::registerClass<ToolButton>();
	m_gui = new Root(Game::width(), Game::height());
	m_gui->setRenderer( new gui::Renderer() );
	m_gui->load("gui.xml");

	// Setup event callbacks
	m_gui->getWidget<Button>("newmap")->eventPressed.bind(this, &WorldEditor::showNewDialog);
	m_gui->getWidget<Button>("openmap")->eventPressed.bind(this, &WorldEditor::showOpenDialog);
	m_gui->getWidget<Button>("savemap")->eventPressed.bind(this, &WorldEditor::showSaveDialog);
	m_gui->getWidget<Combobox>("toollist")->eventSelected.bind(this, &WorldEditor::selectToolGroup);
}

WorldEditor::~WorldEditor() {
	clear();
}

void WorldEditor::clear() {
	// Remove and delete all objects
	for(base::HashMap<Object*>::iterator i=m_objects.begin(); i!=m_objects.end(); ++i) {
		(*i)->removeFromScene(m_renderer);
		delete *i;
	}
	// Delete the editor module
	if(m_editor) delete m_editor;
	if(m_heightMap) delete m_heightMap;
	for(uint i=0; i<m_groups.size(); ++i) delete m_groups[i];
	m_groups.clear();
	m_heightMap = 0;
	m_editor = 0;

	// delete textures
	for(HashMap<EditableTexture*>::iterator i=m_textures.begin(); i!=m_textures.end(); ++i) delete *i;
	m_textures.clear();

	// delete tools from dropdown list
	Combobox* list = m_gui->getWidget<Combobox>("toollist");
	if(list) { list->clearItems();
		Widget* panel = list->getParent();
		if(panel->getWidgetCount() > 1) panel->remove( panel->getWidget(1) );
	}
}

void WorldEditor::update() {
	base::SceneState::update();

	// Update GUI
	Point mouse;
	int mb = Game::Mouse(mouse.x, mouse.y);
	int mw = Game::MouseWheel();
	mouse.y = Game::height() - mouse.y;
	m_gui->mouseEvent(mouse, mb, mw);
	if(Game::LastKey()) m_gui->keyEvent(Game::LastKey(), Game::LastChar());
	m_gui->update();

	bool guiHasMouse = m_gui->getRootWidget()->getWidget(mouse) != m_gui->getRootWidget();
	

	// Update camera
	m_camera->update();
	static_cast<base::FPSCamera*>(m_camera)->setEnabled( base::Game::Mouse()&4 );

	// Collide with terrain
	if(m_heightMap && m_options.collide) {
		vec3 p = m_camera->getPosition();
		float h = m_heightMap->getHeight( p );
		if(p.y < h + 0.1) {
			p.y = h + 0.1;
			m_camera->setPosition( p );
		}
	}

	// Update editor
	if(m_editor) {
		int shift = 0;
		if(Game::Key(KEY_LSHIFT) || Game::Key(KEY_RSHIFT)) shift |= 1;
		if(Game::Key(KEY_LCTRL) || Game::Key(KEY_RCTRL)) shift |= 2;
		if(guiHasMouse) mb = mw = 0;
		vec3 cp = m_camera->getPosition();
		vec3 cd = m_camera->unproject( vec3(mouse.x, Game::height()-mouse.y, 1), Game::getSize() ) - cp;
		m_editor->update(cp, cd, mb, mw, shift);
	}

	// Update any objects
	for(base::HashMap<Object*>::iterator i=m_objects.begin(); i!=m_objects.end(); ++i) {
		(*i)->update();
	}
}

void WorldEditor::drawScene() {
	m_renderer->collect(m_camera);
	m_renderer->render();


	// Draw editor stuff
	if(m_editor) m_editor->draw();
}

void WorldEditor::drawHUD() {
	m_gui->draw();
}


// ======================= GUI Events ========================== //

void WorldEditor::showNewDialog(Button*) {
	Widget* w = m_gui->getWidget<Widget>("newdialog");
	w->setVisible(true);
}
void WorldEditor::showOpenDialog(Button*) {
	FileDialog* d = m_gui->getWidget<FileDialog>("filedialog");
	d->eventConfirm.bind(this, &WorldEditor::loadWorld);
	d->setFileName("");
	d->setFilter("*.xml");
	d->showOpen();
}
void WorldEditor::showSaveDialog(Button*) {
	FileDialog* d = m_gui->getWidget<FileDialog>("filedialog");
	d->eventConfirm.bind(this, &WorldEditor::saveWorld);
	d->setFileName("");
	d->setFilter("*.xml");
	d->showSave();
}

void WorldEditor::selectToolGroup(Combobox* c, int index) {
	ToolGroup* g = *c->getItemData(index).cast<ToolGroup*>();
	Widget* p = c->getParent();
	if(p->getWidgetCount() > 1) p->remove( p->getWidget(1) );
	p->add( g->getPanel() );
	g->getPanel()->setPosition( c->getSize().x + 4, 0 );
}

void WorldEditor::selectTool(ToolInstance* t) {
	m_editor->setTool(t);
}

void closeMessageBox(Button* b) { b->getParent()->getParent()->setVisible(false); }
void WorldEditor::messageBox(const char* c, const char* m, ...) {
	static char buffer[2048];
	va_list args;
	va_start(args, m);
	vsprintf(buffer, m, args);
	va_end(args);
	printf("%s: %s\n", c, buffer);

	gui::Window* base = m_gui->getWidget<gui::Window>("messagebox");
	gui::Window* box = base->clone()->cast<gui::Window>();
	Label* msg = box->getWidget<Label>("message");
	Button* btn = box->getWidget<Button>("button");
	Point s = msg->getSkin()->m_font->getSize(buffer, msg->getSkin()->m_fontSize);

	msg->setCaption(buffer);
	btn->setCaption("  Ok  ");
	btn->eventPressed.bind(&closeMessageBox);
	box->setCaption(c);
	box->setSize(s.x + 30, s.y + 64);
	box->setVisible(true);
	m_gui->getRootWidget()->add(box);
}

// ======================= Tool Setup ========================== //

void WorldEditor::setupHeightTools(float res, const vec2& offset) {
	GeometryToolGroup* group = new GeometryToolGroup();
	group->setup(m_gui);
	group->addTool("raise",   new HeightTool(m_heightMap), 0, 1);
	group->addTool("smooth",  new SmoothTool(m_heightMap), 0, 0);
	group->addTool("level",   new LevelTool(m_heightMap), 0, 1);
	group->addTool("flatten", new FlattenTool(m_heightMap), 0, 1);
	group->setResolution(offset, offset * -2, res);
	addGroup(group, "terrain");
}

void WorldEditor::addGroup(ToolGroup* group, const char* icon) {
	m_groups.push_back(group);
	Combobox* list = m_gui->getWidget<Combobox>("toollist");
	if(list) {
		int iconIndex = list->getIconList()->getIconIndex(icon);
		list->addItem(group->getName(), group, iconIndex);
	}
	group->eventToolSelected.bind(this, &WorldEditor::selectTool);
}

// ======================= Loading and Saving Maps ========================== //


inline int enumerate(const char* key, const char** values, int size) {
	for(int i=0; i<size; ++i) {
		if(strcmp(key, values[i])==0) return i;
	}
	return -1;
}

inline void directory(const char* path, char* dir) {
	strcpy(dir, path);
	for(int i=strlen(dir); i>=0 && dir[i]!='/' && dir[i]!='\\'; --i) dir[i] = 0;
	int l = strlen(dir);
	if(l==0 || (dir[l-1]!='/' && dir[l-1]!='\\')) strcpy(dir+l, "/");
}
inline const char* cat(const char* a, const char* b, const char* c=0, const char* d=0) {
	static char buffer[1024];
	strcpy(buffer, a);
	if(b) strcat(buffer, b);
	if(c) strcat(buffer, c);
	if(d) strcat(buffer, d);
	return buffer;
}

void WorldEditor::create(int size, float res, float scale, bool streamed) {
	if(streamed) {
		Streamer* map = new Streamer(scale);
		map->createStream("tmp.tiff", size, size, 1, 16);
		map->addToScene(m_renderer);
		m_heightMap = new StreamingHeightmapEditor(map);
		m_objects["terrain"] = map;
		m_streaming = true;

	} else {
		SimpleHeightmap* map = new SimpleHeightmap();
		map->create(size, size, res, 0.f);
		map->addToScene(m_renderer);
		m_heightMap = new SimpleHeightmapEditor(map);
		m_objects["terrain"] = map;
		m_streaming = false;
	}
	// Set up editor
	m_editor = new TerrainEditor();
	m_editor->setHeightmap(m_heightMap);
}

void WorldEditor::loadWorld(const char* file) {
	clear();
	char path[512]; directory(file, path);
	XML xml = XML::load(file);
	if(!(xml.getRoot() == "scene")) {
		messageBox("Load error", "Invalid scene file");
		return;
	}

	const XMLElement& terrain = xml.getRoot().find("terrain");
	if(!terrain.name()) {
		messageBox("Load error", "Scene file missing terrain object");
		return;
	}

	// Terrain info
	int size = terrain.attribute("size", 0);
	const XMLElement& info = terrain.find("data");
	float res = info.attribute("resolution", 1.f);
	float scale = info.attribute("scale", 1.f);
	const char* source = info.attribute("file");
	const char* format = info.attribute("type");

	m_library->addPath(path);

	// Material
	const XMLElement& matData = terrain.find("material");
	const char* material = matData.attribute("file", "default.mat");
	

	// Create terrain - maybe use a plugin system for different types?
	if(info.attribute("stream", 0)) {
		Streamer* map = new Streamer(scale);
		if(map->openStream( cat(path, source) )) {
			map->addToScene(m_renderer);
			map->setMaterial( m_library->material(material)  );
			m_heightMap = new StreamingHeightmapEditor(map);
			m_objects["terrain"] = map;
			m_terrainOffset = map->getOffset().xz();
			m_streaming = true;
		} else {
			messageBox("Load error", "Failed to open stream %s", cat(path, source));
			return;	
		}
	}
	else {
		if(strcmp(format, "float")==0) {
			SimpleHeightmap* map = new SimpleHeightmap();
			File data = File::load( cat(path, source) );
			map->create(size, size, res, (const float*) data.contents());
			map->addToScene(m_renderer);
			map->setMaterial( m_library->material(material)  );
			m_heightMap = new SimpleHeightmapEditor(map);
			m_terrainOffset = vec2();
			m_objects["terrain"] = map;
			m_streaming = false;
		}
		else {
			messageBox("Load error", "Invalid terrain format %s", format);
			return;
		}
	}

	// Set up terrain editor
	m_editor = new TerrainEditor();
	m_editor->setHeightmap( m_heightMap );
	setupHeightTools(res, m_terrainOffset);

	// Load editable texture maps
	char buffer[1024];
	const char* textureUsage[] = { "map", "colour", "weight", "index", "cindex" };
	for(XML::iterator m=terrain.begin(); m!=terrain.end(); ++m) {
		if(*m == "map") {
			const char* usage = m->attribute("usage");
			const char* file  = m->attribute("file");
			const char* name  = m->attribute("name");
			const char* link  = m->attribute("link");
			bool stream = m->attribute("stream", 0);
			int  use = enumerate(usage, textureUsage, 5);
			
			// Already exists ?
			if(m_textures.contains(name)) {
				messageBox("Warning", "Texture %s already exists", name);
				continue;
			}

			// resolve filename
			if(!m_library->findFile(file, buffer)) {
				messageBox("Error", "Could not find file %s", file);
				return;
			}


			// Create texture
			EditableTexture* tex = 0;
			if(stream && use > 0) {
				TextureStream* ts = new TextureStream();
				ts->initialise(2048, true);
				if(ts->openStream(buffer)) tex = new EditableTexture(ts);
				else messageBox("Error", "Failed to open stream %s", file);
			}
			else if(stream) {
				BufferedStream* bs = new BufferedStream();
				if(bs->openStream(buffer)) tex = new EditableTexture(bs);
				else messageBox("Error", "Failed to open stream %s", file);
			}
			else {
				tex = new EditableTexture(buffer, use>0);
			}

			if(!tex) return;
			m_textures[name] = tex;


			// Create tool
			ToolGroup* g = 0;
			switch(use) {
			case 0: // Map
				g = new WeightToolGroup(name, tex);
				break;
			case 1:	// Colour
				g = new ColourToolGroup(name, tex);
				break;
				
			case 2:	// Weight
				if(*link==0) g = new WeightToolGroup(name, tex);
				else if(m_textures.contains(link)) {
					g = new MaterialToolGroup( m_textures[link], tex);
				}
				break;

			case 3:	// index
				if(m_textures.contains(link)) {
					g = new MaterialToolGroup(tex, m_textures[link]);
				}
				break;

			case 4:	// Coherent Index
				break;
			}
			// Add to list - need to add to a listbox
			if(g) {
				g->setResolution(m_terrainOffset, vec2(size,size), res);
				addGroup(g, "texture");
			}
		}
	}
}
void WorldEditor::saveWorld(const char* file) {
	

}


