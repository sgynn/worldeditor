#include "worldeditor.h"
#include <base/game.h>
#include <base/input.h>
#include <base/fpscamera.h>
#include <base/png.h>
#include <base/directory.h>
#include <base/opengl.h>

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
#include "widgets/orderableitem.h"

#include <cstdarg>

#include "terraineditor/heighttools.h"
#include "terraineditor/texturetools.h"

#include "dynamicmaterial.h"
#include "materialeditor.h"
#include "minimap.h"

using namespace gui;
using namespace base;

#define INIFILE "settings.cfg"

#ifdef WIN32
#define main _main
int main(int argc, char* argv[]);
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,int nCmdShow) {
	freopen("editor.log", "w", stdout);
	return main(0,0);
}
#endif

// Application entry point
int main(int argc, char* argv[]) {
	base::INIFile cfg = base::INIFile::load(INIFILE);
	base::INIFile::Section wnd = cfg["window"];

	// System options
	int w = wnd.get("width", 1280);
	int h = wnd.get("height", 1024);
	int aa = wnd.get("antialiasing", 0);
	bool fs = wnd.get("fullscreen", false);


	base::Game* game = base::Game::create(w,h,32,fs,aa);
	game->setInitialState( new WorldEditor(cfg) );
	game->run();

	delete game;
	return 0;
}


//// Make a world - this file will be a complete mess while I test stuff ////

WorldEditor::WorldEditor(const INIFile& ini) : m_editor(0), m_heightMap(0), m_materials(0) {
	m_scene = new Scene;
	m_library = new Library;
	m_library->addPath("data");

	// Load editor options
	INIFile::Section options = ini["settings"];
	m_options.speed      = options.get("speed", 4.0f);
	m_options.detail     = options.get("detail", 4.0f);
	m_options.collide    = options.get("collision", true);
	m_options.tabletMode = options.get("tablet", true);
	m_options.distance   = options.get("distance", 10000);
	m_options.fov        = options.get("fov", 90.0f);

	// Run an fps camera for now
	base::FPSCamera* cam = new base::FPSCamera(m_options.fov, base::Game::aspect(), 0.01, m_options.distance);
	cam->setSpeed(m_options.speed, 0.004);
	cam->setEnabled(false);
	cam->lookat( vec3(10, 50, 10), vec3(100,0,100));
	m_camera = cam;

	// Set up gui
	Root::registerClass<FileDialog>();
	Root::registerClass<ToolButton>();
	Root::registerClass<OrderableItem>();
	m_gui = new Root(Game::width(), Game::height());
	m_gui->setRenderer( new gui::Renderer() );
	m_gui->load("data/gui.xml");
	m_mapMarker = 0;

	// Setup event callbacks
	#define BIND(Type, name, event, callback) { Type* w = m_gui->getWidget<Type>(name); if(w) w->event.bind(this, &WorldEditor::callback); else printf("Missing widget %s\n", name); }
	BIND(Button, "newmap",  eventPressed, showNewDialog);
	BIND(Button, "openmap", eventPressed, showOpenDialog);
	BIND(Button, "savemap", eventPressed, showSaveDialog);
	BIND(Button, "options", eventPressed, showOptionsDialog);
	BIND(Button, "minimap", eventPressed, showWorldMap);

	BIND(Combobox, "toollist", eventSelected, selectToolGroup);
	BIND(Button, "materialbutton", eventPressed, showMaterialList);
	BIND(Button, "texturebutton",  eventPressed, showTextureList);

	BIND(Widget, "mapimage", eventMouseDown, moveWorldMap);
	BIND(Widget, "mapimage", eventMouseMove, moveWorldMap);

	BIND(Scrollbar, "brushsize",     eventChanged, changeBrushSlider);
	BIND(Scrollbar, "brushstrength", eventChanged, changeBrushSlider);
	BIND(Scrollbar, "brushfalloff",  eventChanged, changeBrushSlider);

	// New map dialog
	Widget* newPanel = m_gui->getWidget<Widget>("newdialog");
	newPanel->getWidget<Button>("create")->eventPressed.bind(this, &WorldEditor::createNewTerrain);
	newPanel->getWidget<Button>("cancel")->eventPressed.bind(this, &WorldEditor::cancelNewTerrain);
	newPanel->getWidget<Combobox>("mode")->eventSelected.bind(this, &WorldEditor::changeTerrainMode);
	newPanel->getWidget<Button>("sourcebutton")->eventPressed.bind(this, &WorldEditor::browseTerrainSource);

	// Settings
	BIND(Scrollbar, "viewdistance",  eventChanged, changeViewDistance);
	BIND(Scrollbar, "terraindetail", eventChanged, changeDetail);
	BIND(Scrollbar, "cameraspeed",   eventChanged, changeSpeed);
	BIND(Checkbox,  "tabletmode",    eventChanged, changeTabletMode);
	BIND(Checkbox,  "collision",     eventChanged, changeCollision);
	BIND(gui::Window, "settings",    eventClosed,  saveSettings);

	m_gui->getWidget<Scrollbar>("viewdistance")->setValue((m_options.distance - 1000) / 10);
	m_gui->getWidget<Scrollbar>("cameraspeed")->setValue(m_options.speed * 100);
	m_gui->getWidget<Scrollbar>("terraindetail")->setValue((16-m_options.detail)/15 * 1000);
	m_gui->getWidget<Checkbox>("tabletmode")->setSelected(m_options.tabletMode);
	m_gui->getWidget<Checkbox>("collision")->setSelected(m_options.collide);

	// Initail slignment - would be good to do this in the xml somehow
	Widget* brush = m_gui->getWidget<Widget>("brushinfo");
	Widget* shelf = m_gui->getWidget<Widget>("toolshelf");
	if(brush) brush->setPosition( Game::width() - brush->getSize().x, 0);
	if(shelf) shelf->setSize( Game::width() - shelf->getPosition().x - brush->getSize().x - 8, shelf->getSize().y);

	// Initialise minimap
	m_minimap = new MiniMap();
	base::Texture& mapTex = m_minimap->getTexture();
	int id = m_gui->getRenderer()->addImage("minimap", mapTex.width(), mapTex.height(), mapTex.unit());
	m_gui->getWidget<Image>("mapimage")->setImage(id);

}

WorldEditor::~WorldEditor() {
	clear();
}

void WorldEditor::clear() {
	// Remove and delete all objects
	for(base::HashMap<Object*>::iterator i=m_objects.begin(); i!=m_objects.end(); ++i) {
		(*i)->removeFromScene(m_scene);
		delete *i;
	}
	// Delete the editor module
	if(m_editor) delete m_editor;
	if(m_heightMap) delete m_heightMap;
	for(uint i=0; i<m_groups.size(); ++i) delete m_groups[i];
	m_groups.clear();
	m_heightMap = 0;
	m_editor = 0;
	m_file = 0;
	m_streams.clear();

	// delete materials
	if(m_materials) delete m_materials;
	m_materials = 0;
	showMaterialList(0);
	showTextureList(0);

	// delete tools from dropdown list
	Combobox* list = m_gui->getWidget<Combobox>("toollist");
	if(list) { list->clearItems();
		Widget* panel = list->getParent();
		if(panel->getWidgetCount() > 3) panel->remove( panel->getWidget(3) );
	}
}

void WorldEditor::resized() {
	Point size = Game::getSize();
	m_gui->resize(size.x, size.y);
	glViewport(0,0,size.x, size.y);
}

void WorldEditor::update() {
	// Update GUI
	Point mouse;
	int mb = Game::Mouse(mouse.x, mouse.y);
	int mw = Game::MouseWheel();
	mouse.y = Game::height() - mouse.y;
	m_gui->mouseEvent(mouse, mb, mw);
	if(Game::LastKey()) m_gui->keyEvent(Game::LastKey(), Game::LastChar());
	m_gui->update();

	// Resized window
	if(Game::getSize() != m_gui->getRootWidget()->getSize()) resized();

	bool guiHasMouse = m_gui->getRootWidget()->getWidget(mouse) != m_gui->getRootWidget();
	bool editingText = m_gui->getFocusedWidget()->cast<Textbox>();

	int shift = 0;
	if(Game::Key(KEY_LSHIFT) || Game::Key(KEY_RSHIFT)) shift |= 1;
	if(Game::Key(KEY_LCTRL) || Game::Key(KEY_RCTRL)) shift |= 2;

	// Escape button - close stuff
	if(Game::Pressed(KEY_ESCAPE)) {
		// Close topmost window
		bool closedSomething = false;
		for(int i=m_gui->getRootWidget()->getWidgetCount()-1; i>0; --i) {
			Widget* w = m_gui->getRootWidget()->getWidget(i);
			if(w->isVisible() && w->cast<gui::Window>()) {
				w->setVisible(false);
				closedSomething = true;
				break;
			}
		}
		if(!closedSomething) changeState(0);	// exit
	}
	

	// Update camera
	FPSCamera* cam = static_cast<base::FPSCamera*>(m_camera);
	cam->setSpeed( editingText? 0: m_options.speed, 0.004 );
	cam->setEnabled( mb&4 );
	cam->grabMouse( (mb&4) && !m_options.tabletMode );
	cam->update();

	// Map marker
	if(m_mapMarker) {
		vec2 p = (m_camera->getPosition().xz() - m_terrainOffset) / m_terrainSize;
		const Point& ms = m_mapMarker->getParent()->getAbsoluteClientRect().size();
		const Point& ps = m_mapMarker->getSize(); 
		const vec3& dir = cam->getDirection();
		m_mapMarker->setPosition(p.x * ms.x - ps.x/2, p.y * ms.y - ps.y/2);
		m_mapMarker->cast<gui::Icon>()->setAngle( -atan2(dir.x, dir.z) );
	}

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
		if(guiHasMouse) mb = mw = 0;
		vec3 cp = m_camera->getPosition();
		vec3 cd = m_camera->unproject( vec3(mouse.x, Game::height()-mouse.y, 1), Game::getSize() ) - cp;
		m_editor->update(cp, cd, mb, mw, shift);
		if(mw) updateBrushSliders();
	}

	// Other shortcuts
	if(!editingText) {
		if(Game::Pressed(KEY_T)) showTextureList(0);
		if(Game::Pressed(KEY_R)) showMaterialList(0);
		if(Game::Pressed(KEY_P)) showOptionsDialog(0);
		if(Game::Pressed(KEY_M)) showWorldMap(0);
	}
	if(Game::Pressed(KEY_O) && shift==2) showOpenDialog(0);
	if(Game::Pressed(KEY_S) && shift==2) showSaveDialog(0);

	// Update any objects
	for(base::HashMap<Object*>::iterator i=m_objects.begin(); i!=m_objects.end(); ++i) {
		(*i)->update();
	}
}

void WorldEditor::drawScene() {
	m_scene->collect(m_camera);
	m_scene->render();

	// Draw editor stuff
	if(m_editor) m_editor->draw();
}

void WorldEditor::drawHUD() {
	scene::Material m;
	m.addPass()->blendMode = scene::BLEND_ALPHA;
	m.getPass(0)->bind();

	m_gui->draw();
}


// ======================= GUI Events ========================== //

void showDialog(Widget* w) {
	if(!w) return;
	w->setVisible(true);
	w->raise();
}

void WorldEditor::showNewDialog(Button*) {
	Widget* w = m_gui->getWidget<Widget>("newdialog");
	showDialog(w);
}
void WorldEditor::showOpenDialog(Button*) {
	FileDialog* d = m_gui->getWidget<FileDialog>("filedialog");
	d->eventConfirm.bind(this, &WorldEditor::loadWorld);
	d->setFilter("*.xml");
	d->setFileName("");
	d->showOpen();
}
void WorldEditor::showSaveDialog(Button*) {
	bool shift = Game::Key(KEY_LSHIFT) || Game::Key(KEY_RSHIFT);
	if(m_file && !shift) {
		saveWorld(m_file);
	} else {
		FileDialog* d = m_gui->getWidget<FileDialog>("filedialog");
		d->eventConfirm.bind(this, &WorldEditor::saveWorld);
		d->setFilter("*.xml");
		d->setFileName("");
		d->showSave();
	}
}
void WorldEditor::showOptionsDialog(Button*) {
	Widget* w = m_gui->getWidget<Widget>("settings");
	if(w && w->isVisible()) w->setVisible(false);
	else showDialog(w);
}
// ----------------------------------------------------------- //

void WorldEditor::createNewTerrain(gui::Button* b) {
	cancelNewTerrain(0);
	// Create new terrain
	Widget* panel = b->getParent();
	int mode = panel->getWidget<Combobox>("mode")->getSelectedIndex();
	//const char* source = panel->getWidget<Textbox>("source")->getText();
	int size = panel->getWidget<Combobox>("size")->getSelectedData().getValue(128);
	float height = panel->getWidget<Spinbox>("height")->getValue();
	switch(mode) {
	case 0: create(size, 1, height, false); break;
	case 1: create(size, 1, height, false); break;
	case 2: create(size, 1, height, false); break;
	case 3: create(size, 1, height, true); break;
	case 4: create(size, 1, height, true); break;
	}
	// ToDo: convert source
}
void WorldEditor::cancelNewTerrain(gui::Button*) {
	Widget* w = m_gui->getWidget<Widget>("newdialog");
	if(w) w->setVisible(false);
}
void WorldEditor::changeTerrainMode(gui::Combobox* list, int index) {
	bool streamed = index>2;
	Combobox* sizes = list->getParent()->getWidget<Combobox>("size");
	sizes->clearItems();
	char buffer[32];
	int min = streamed? 8: 6;
	int max = streamed? 14: 10;
	for(int i=min; i<=max; ++i) {
		sprintf(buffer, "%dx%d", 1<<i, 1<<i);
		sizes->addItem(buffer, 1<<i);
	}
	sizes->selectItem(0);
}
void WorldEditor::browseTerrainSource(gui::Button*) {
	FileDialog* d = m_gui->getWidget<FileDialog>("filedialog");
	d->eventConfirm.bind(this, &WorldEditor::setTerrainSource);
	d->setFilter("*.png,*.tif,*.tiff");
	d->setFileName("");
	d->showOpen();
}
void WorldEditor::setTerrainSource(const char* file) {
	Textbox* w = m_gui->getWidget<Textbox>("source");
	if(w) w->setText(file);
}

// ----------------------------------------------------------- //

void WorldEditor::changeViewDistance(Scrollbar*, int v) {
	m_options.distance = 1000 + v * 9;
	m_camera->adjustDepth(0.1, m_options.distance);
}
void WorldEditor::changeDetail(Scrollbar*, int v) {
	float value = v / 1000.f;
	m_options.detail = 16 - value * 15;
	if(m_heightMap) m_heightMap->setDetail( m_options.detail );
}
void WorldEditor::changeSpeed(Scrollbar*, int v) {
	float value = v / 1000.f;
	m_options.speed = value * 10;
	static_cast<base::FPSCamera*>(m_camera)->setSpeed(m_options.speed, 0.004);
}
void WorldEditor::changeTabletMode(Button* b) {
	m_options.tabletMode = b->isSelected();
}

void WorldEditor::changeCollision(Button* b) {
	m_options.collide = b->isSelected();
}

void WorldEditor::saveSettings(gui::Window*) {
	INIFile ini = INIFile::load(INIFILE);
	INIFile::Section& settings = ini["settings"];
	settings.set("distance", m_options.distance);
	settings.set("speed",    m_options.speed);
	settings.set("detail",   m_options.detail);
	settings.set("tablet",   m_options.tabletMode);
	settings.set("collision",m_options.collide);
	settings.set("fov",      m_options.fov);
	ini.save(INIFILE);
}

// ----------------------------------------------------------- //

void WorldEditor::showWorldMap(Button*) {
	Widget* w = m_gui->getWidget<Widget>("worldmap");
	if(m_heightMap && !w->isVisible()) showDialog(w);
	else w->setVisible(false);
	m_mapMarker = w->isVisible()? w->getWidget<Widget>("mapmarker"): 0;
	if(w->isVisible()) m_minimap->build();
}

void WorldEditor::moveWorldMap(Widget* w, const Point& p, int b) {
	if(b==1) {
		vec2 pos(p.x, p.y);
		pos.x /= w->getAbsoluteClientRect().width;
		pos.y /= w->getAbsoluteClientRect().height;
		pos = pos * m_terrainSize + m_terrainOffset;
		vec3 camPos(pos.x, 0, pos.y);
		camPos.y = m_camera->getPosition().y;
		m_camera->setPosition(camPos);
	}
}

// ----------------------------------------------------------- //

void WorldEditor::showMaterialList(Button*) {
	Widget* w = m_gui->getWidget<Widget>("materials");
	if(m_materials && !w->isVisible()) showDialog(w);
	else w->setVisible(false);
}

void WorldEditor::showTextureList(Button*) {
	Widget* w = m_gui->getWidget<Widget>("textures");
	if(m_materials && !w->isVisible()) showDialog(w);
	else w->setVisible(false);
}

void WorldEditor::setTerrainMaterial(DynamicMaterial* m) {
	m_heightMap->setMaterial(m);
}


void WorldEditor::selectToolGroup(Combobox* c, int index) {
	ToolGroup* g = *c->getItemData(index).cast<ToolGroup*>();
	Widget* p = c->getParent();
	if(p->getWidgetCount() > 3) p->remove( p->getWidget(3) );
	p->add( g->getPanel() );
	g->getPanel()->setPosition( c->getPosition().x + c->getSize().x + 4, 0 );
	m_editor->setTool(0);
	if(g->getPanel()->getWidgetCount()>0) {
		g->getPanel()->getWidget(0)->setSelected(false);
	}
}

void WorldEditor::selectTool(ToolInstance* t) {
	m_editor->setTool(t);
	updateBrushSliders();
}

// ----------------------------------------------------------- //

void WorldEditor::changeBrushSlider(Scrollbar* s, int ival) {
	if(!m_editor) return;
	float v = ival / 1000.f;
	Brush b = m_editor->getBrush();
	int index = s->getPosition().y / 18;
	switch(index) {
	case 0: b.radius   = powf(v, 2) * 100; break;
	case 1: b.strength = v; break;
	case 2: b.falloff  = v; break;
	default: return;
	}
	m_editor->setBrush(b);
}
void WorldEditor::updateBrushSliders() {
	if(!m_editor) return;
	const Brush& b = m_editor->getBrush();
	Scrollbar* radius   = m_gui->getWidget<Scrollbar>("brushsize");
	Scrollbar* strength = m_gui->getWidget<Scrollbar>("brushstrength");
	Scrollbar* falloff  = m_gui->getWidget<Scrollbar>("brushfalloff");
	if(radius)   radius->setValue( sqrt(b.radius/100) * 1000);
	if(strength) strength->setValue( b.strength * 1000 );
	if(falloff)  falloff->setValue( b.falloff * 1000 );
}

// ----------------------------------------------------------- //

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
	Point s = msg->getSkin()->getFont()->getSize(buffer, msg->getSkin()->getFontSize());

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
	// Select first one
	if(m_groups.size()==1) {
		list->selectItem(0);
		selectToolGroup(list, 0);
	}
}

// ======================= Loading and Saving Maps ========================== //

bool copyFile(const char* source, const char* dest) {
	char buffer[BUFSIZ];
	size_t size;
	FILE* src = fopen(source, "rb");
	if(!src) return false;
	FILE* dst = fopen(dest, "wb");
	while((size = fread(buffer, 1, BUFSIZ, src))) {
		fwrite(buffer, 1, size, dst);
	}
	fclose(src);
	fclose(dst);
	return true;
}
bool deleteFile(const char* file) {
	return remove(file) == 0;
}

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
	clear();
	if(streamed) {
		Streamer* map = new Streamer(scale);
		map->createStream("tmp.tiff", size, size, 1, 16);
		map->setLod( m_options.detail );
		map->addToScene(m_scene);
		m_heightMap = new StreamingHeightmapEditor(map);
		m_objects["terrain"] = map;
		m_terrainOffset = map->getOffset().xz();
		m_terrainScale = scale;
		m_terrainSize.x = m_terrainSize.y = size-1;
		m_streaming = true;
		m_streams.push_back(map);
		printf("Created %dx%d heightmap stream as %s\n", size,size, "tmp.tiff");

	} else {
		SimpleHeightmap* map = new SimpleHeightmap();
		map->create(size, size, res, 0.f);
		map->addToScene(m_scene);
		m_heightMap = new SimpleHeightmapEditor(map);
		m_objects["terrain"] = map;
		m_terrainOffset = vec2();
		m_terrainScale = scale;
		m_terrainSize.x = m_terrainSize.y = size-1;
		m_streaming = false;
		printf("Created %dx%d heightmap\n", size,size);
	}
	// Set up editor
	m_editor = new TerrainEditor();
	m_editor->setHeightmap(m_heightMap);
	setupHeightTools(res, m_terrainOffset);

	// Setup material editor
	m_materials = new MaterialEditor(m_gui, m_library, m_streaming);
	m_materials->eventChangeMaterial.bind(this, &WorldEditor::setTerrainMaterial);
	m_materials->addMaterial(0);

	// Minimap
	m_minimap->setWorld(m_heightMap, m_terrainSize, m_terrainOffset);
	m_minimap->setRange(100);
	m_minimap->build();
}

void WorldEditor::loadWorld(const char* file) {
	clear();
	char path[512]; directory(file, path);
	XML xml = XML::load(file);
	if(!(xml.getRoot() == "scene")) {
		messageBox("Load error", "Invalid scene file");
		m_file = 0;
		return;
	}

	const XMLElement& terrain = xml.getRoot().find("terrain");
	if(!terrain.name()) {
		messageBox("Load error", "Scene file missing terrain object");
		m_file = 0;
		return;
	}

	m_library->addPath(path);

	// Terrain info
	char buffer[2058];
	int size = terrain.attribute("width", 0);
	const XMLElement& info = terrain.find("data");
	float res = info.attribute("resolution", 1.f);
	float scale = info.attribute("scale", 1.f);
	const char* source = info.attribute("file");
	const char* format = info.attribute("type");
	if(!m_library->findFile(source, buffer)) {
		messageBox("Error", "Could not find file %s", source);
		return;
	}
	m_terrainFile = buffer;
	m_file = file;

	Streamer* streamer = 0;
	SimpleHeightmap* simple = 0;


	// Create terrain - maybe use a plugin system for different types?
	if(info.attribute("stream", 0)) {
		streamer = new Streamer(scale);
		if(streamer->openStream( m_terrainFile )) {
			streamer->addToScene(m_scene);
			streamer->setLod( m_options.detail );
			m_heightMap = new StreamingHeightmapEditor(streamer);
			m_objects["terrain"] = streamer;
			m_terrainOffset = streamer->getOffset().xz();
			m_streaming = true;
			size = streamer->width();
			if(size != streamer->height()) messageBox("Warning", "Terrian map is not square");
			m_terrainScale = scale;
			m_terrainSize.x = m_terrainSize.y = size-1;
			m_streams.push_back(streamer);
		} else {
			messageBox("Load error", "Failed to open stream %s", cat(path, source));
			return;	
		}
	}
	else {
		if(strcmp(format, "float")==0) {
			simple = new SimpleHeightmap();
			File data = File::load( m_terrainFile );
			simple->create(size, size, res, (const float*) data.contents());
			simple->addToScene(m_scene);
			m_heightMap = new SimpleHeightmapEditor(simple);
			m_terrainOffset = vec2();
			m_objects["terrain"] = simple;
			m_streaming = false;
		}
		else {
			messageBox("Load error", "Invalid terrain format %s", format);
			return;
		}
	}

	// Load materials
	m_materials = new MaterialEditor(m_gui, m_library, m_streaming);
	m_materials->eventChangeMaterial.bind(this, &WorldEditor::setTerrainMaterial);
	for(XML::iterator i=terrain.begin(); i!=terrain.end(); ++i) {
		if(*i == "material") m_materials->loadMaterial(*i);
		else if(*i == "texture") m_materials->loadTexture(*i);
	}
	m_materials->buildTextures();	// Build texture arrays
	


	// Set up terrain editor
	m_editor = new TerrainEditor();
	m_editor->setHeightmap( m_heightMap );
	setupHeightTools(res, m_terrainOffset);
	

	// Load editable texture maps
	const char* textureUsage[] = { "weight", "colour", "index", "cindex" };
	for(XML::iterator m=terrain.begin(); m!=terrain.end(); ++m) {
		if(*m == "map") {
			const char* usage = m->attribute("usage");
			const char* file  = m->attribute("file");
			const char* name  = m->attribute("name");
			const char* link  = m->attribute("link");
			bool stream = m->attribute("stream", 0);
			int  use = enumerate(usage, textureUsage, 4);
			
			// Already exists ?
			if(m_materials->getMap(name)) {
				messageBox("Warning", "Texture Map %s already exists", name);
				continue;
			}

			// resolve filename
			if(!m_library->findFile(file, buffer)) {
				messageBox("Error", "Could not find file %s", file);
				continue;
			}

			// Create texture
			EditableTexture* tex = 0;
			if(stream) {
				TextureStream* ts = new TextureStream();
				if(ts->openStream(buffer)) {
					tex = new EditableTexture(ts);
					ts->initialise(2048, true);
					m_streams.push_back(ts);
				} else messageBox("Error", "Failed to open stream %s", file);
			} else {
				tex = new EditableTexture(buffer, true);
			}

			// Add texture to world
			if(!tex) continue;
			m_materials->addMap(name, tex);
			ImageMapData* map = new ImageMapData();
			map->map = tex;
			map->file = file;
			map->name = name;
			map->link = link[0]? link: 0;
			map->usage = use;
			m_imageMaps.push_back(map);

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
				else if(m_materials->getMap(link)) {
					g = new MaterialToolGroup( m_materials->getMap(link), tex);
				}
				break;

			case 3:	// index
				if(m_materials->getMap(link)) {
					g = new MaterialToolGroup(tex, m_materials->getMap(link));
				}
				break;

			case 4:	// Coherent Index
				break;
			}
			// Add tool to list
			if(g) {
				g->setup(m_gui);
				g->setResolution(m_terrainOffset, vec2(size,size), res);
				addGroup(g, "texture");
			}
		}
	}

	// Set initial material
	m_materials->selectMaterial(0,0);

	// Camera
	const XMLElement& cam = xml.getRoot().find("camera");
	if(cam.name()) {
		vec3 cp, cd;
		cp.x = cam.attribute("x", 0.0);
		cp.y = cam.attribute("y", 50.0);
		cp.z = cam.attribute("z", 0.0);
		cd.x = cam.attribute("dx", 1.0);
		cd.y = cam.attribute("dy", -0.5);
		cd.z = cam.attribute("dz", 1.0);
		m_camera->lookat( cp, cp+cd*100 );
	}


	// Minimap
	m_minimap->setWorld(m_heightMap, m_terrainSize, m_terrainOffset);
	m_minimap->setRange(100);
	m_minimap->build();
}
void WorldEditor::saveWorld(const char* file) {
	if(m_file!=file) m_file = file;
	char path[512]; directory(file, path);
	char buffer[1024];

	// Flush all streams
	for(uint i=0; i<m_streams.size(); ++i) m_streams[i]->flush();

	// Write xml file
	XML xml;
	xml.setRoot( XMLElement("scene") );

	// Write Terrain
	XMLElement& e = xml.getRoot().add("terrain");
	e.setAttribute("width", m_terrainSize.x);
	e.setAttribute("height", m_terrainSize.y);

	// Heightmap
	XMLElement& map = e.add("data");
	map.setAttribute("file", m_terrainFile);
	if(m_streaming) {
		map.setAttribute("stream", 1);
		map.setAttribute("scale", m_terrainScale);
		map.setAttribute("type", "int16");
	} else {
		map.setAttribute("type", "float");
	}
	
	// Materials
	for(int i=0; i<m_materials->getMaterialCount(); ++i) {
		e.add( m_materials->serialiseMaterial(i) );
	}
	// Textures
	for(int i=0; i<m_materials->getTextureCount(); ++i) {
		e.add( m_materials->serialiseTexture(i) );
	}
	// Maps
	static const char* U[] = { "weight", "colour", "index", "cindex" };
	for(uint i=0; i<m_imageMaps.size(); ++i) {
		XMLElement& map = e.add("map");
		map.setAttribute("name", m_imageMaps[i]->name);
		map.setAttribute("file", m_imageMaps[i]->file);
		map.setAttribute("usage", U[m_imageMaps[i]->usage]);
		if(m_imageMaps[i]->map->getMode() >= EditableTexture::STREAM) map.setAttribute("stream", 1);

		if(Directory::isRelative( m_imageMaps[i]->file)) {
			sprintf(buffer, "%s/%s", path, (const char*)m_imageMaps[i]->file);
			m_imageMaps[i]->map->save( buffer );
		} else m_imageMaps[i]->map->save( m_imageMaps[i]->file );
	}

	// Save editor camera position (temp)
	XMLElement& cam = xml.getRoot().add("camera");
	cam.setAttribute("x", m_camera->getPosition().x);
	cam.setAttribute("y", m_camera->getPosition().y);
	cam.setAttribute("z", m_camera->getPosition().z);
	cam.setAttribute("dx", -m_camera->getDirection().x);
	cam.setAttribute("dy", -m_camera->getDirection().y);
	cam.setAttribute("dz", -m_camera->getDirection().z);


	// Save
	if(strcmp(file+strlen(file)-4, ".xml")) {
		sprintf(buffer, "%s.xml", file);
		file = buffer;
	}
	xml.save(file);
	printf("Saved %s\n", file);
}


