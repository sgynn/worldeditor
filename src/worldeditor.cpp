#include "worldeditor.h"
#include <base/game.h>
#include <base/input.h>
#include <base/fpscamera.h>
#include <base/png.h>
#include <base/directory.h>
#include <base/opengl.h>
#include <base/window.h>

#include <base/inifile.h>

#include "resource/file.h"
#include "simple/heightmap.h"
#include "dynamic/dynamicmap.h"
#include "streaming/streamer.h"
#include "streaming/texturestream.h"
#include "streaming/tiff.h"

#include "foliageeditor.h"

#include "gui/skin.h"
#include "gui/font.h"
#include "gui/widgets.h"
#include "gui/lists.h"
#include "widgets/filedialog.h"
#include "widgets/toolbutton.h"
#include "widgets/orderableitem.h"
#include "widgets/colourpicker.h"

#include <cstdarg>

#include "terraineditor/heighttools.h"
#include "terraineditor/texturetools.h"

#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/renderer.h"
#include "dynamicmaterial.h"
#include "materialeditor.h"
#include "minimap.h"

using namespace gui;
using namespace base;

#define INIFILE "settings.cfg"

// GUI Macros
#define BIND(Type, name, event, callback) { Type* w = m_gui->getWidget<Type>(name); if(w) w->event.bind(this, &WorldEditor::callback); else printf("Missing widget %s\n", name); }
#define ENABLE_BUTTON(name)  { Button* b = m_gui->getWidget<Button>(name); if(b) b->setEnabled(true), b->setIconColour(0xffffff); }
#define DISABLE_BUTTON(name) { Button* b = m_gui->getWidget<Button>(name); if(b) b->setEnabled(false), b->setIconColour(0x606060); }

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
	scene::Shader::getSupportedVersion();
	game->setInitialState( new WorldEditor(cfg) );
	game->run();

	delete game;
	return 0;
}


//// Make a world - this file will be a complete mess while I test stuff ////

WorldEditor::WorldEditor(const INIFile& ini) : m_materials(0), m_foliage(0), m_editor(0), m_activeGroup(0), m_terrain(0) {
	m_scene = new scene::Scene;
	m_renderer = new scene::Renderer;
	m_fileSystem = new FileSystem;

	// Load editor options
	INIFile::Section options = ini["settings"];
	m_options.speed      = options.get("speed", 4.0f);
	m_options.detail     = options.get("detail", 4.0f);
	m_options.collide    = options.get("collision", true);
	m_options.tabletMode = options.get("tablet", true);
	m_options.distance   = options.get("distance", 10000);
	m_options.fov        = options.get("fov", 90.0f);

	// Run an fps camera for now
	if(m_options.fov<=0) m_options.fov = 90; // causes nothing to appear but no errors
	base::FPSCamera* cam = new base::FPSCamera(m_options.fov, base::Game::aspect(), 0.01, m_options.distance);
	cam->setSpeed(m_options.speed, 0.004);
	cam->setEnabled(false);
	cam->lookat( vec3(10, 50, 10), vec3(100,0,100));
	m_camera = cam;

	// Set up gui
	Root::registerClass<FileDialog>();
	Root::registerClass<ToolButton>();
	Root::registerClass<OrderableItem>();
	Root::registerClass<ColourPicker>();
	m_gui = new Root(Game::width(), Game::height());
	m_gui->setRenderer( new gui::Renderer() );
	m_gui->load("data/gui.xml");
	m_gui->load("data/foliage.xml");
	m_mapMarker = 0;

	// Setup event callbacks
	BIND(Button, "newmap",  eventPressed, showNewDialog);
	BIND(Button, "openmap", eventPressed, showOpenDialog);
	BIND(Button, "savemap", eventPressed, showSaveDialog);
	BIND(Button, "options", eventPressed, showOptionsDialog);
	BIND(Button, "minimap", eventPressed, showWorldMap);

	BIND(Combobox, "toollist", eventSelected, selectToolGroup);
	BIND(Button, "materialbutton", eventPressed, showMaterialList);
	BIND(Button, "texturebutton",  eventPressed, showTextureList);
	BIND(Button, "foliagebutton",  eventPressed, showFoliageList);

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

	// New editor dialog
	BIND(Combobox, "editormode", eventSelected, validateNewEditor);
	BIND(Combobox, "editorsize", eventSelected, validateNewEditor);
	BIND(Button, "editorcreate", eventPressed, createNewEditor);
	BIND(Button, "editorcancel", eventPressed, cancelNewEditor);
	BIND(gui::Window, "neweditor", eventClosed, cancelNewEditor);

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

	// Disable some buttons
	DISABLE_BUTTON( "savemap" );
	DISABLE_BUTTON( "minimap" );
	DISABLE_BUTTON( "materialbutton" );
	DISABLE_BUTTON( "texturebutton" );
	DISABLE_BUTTON( "foliagebutton" );

	// context menu
	m_contextMenu = m_gui->getWidget<Popup>("contextmenu");
	BIND(Button, "newtile", eventPressed, createNewTile);
	BIND(Button, "settile", eventPressed, showTileList);
	BIND(Button, "loadtile", eventPressed, loadTile);
	BIND(Button, "copytile", eventPressed, duplicateTile);
	BIND(Button, "locktile", eventPressed, lockTile);
	BIND(Button, "hidetile", eventPressed, unloadTile);
	BIND(Button, "nametile", eventPressed, showRenameTile);
	BIND(Listbox, "tilelist", eventSelected, assignTile);
	BIND(Button, "applyname", eventPressed, renameTile);

	// Initial alignment - would be good to do this in the xml somehow
	Widget* brush = m_gui->getWidget<Widget>("brushinfo");
	Widget* shelf = m_gui->getWidget<Widget>("toolshelf");
	if(brush) brush->setPosition( Game::width() - brush->getSize().x, 0);
	if(shelf) shelf->setSize( Game::width() - shelf->getPosition().x - brush->getSize().x - 8, shelf->getSize().y);

	// Initialise minimap
	m_minimap = new MiniMap();
	base::Texture& mapTex = m_minimap->getTexture();
	int id = m_gui->getRenderer()->addImage("minimap", mapTex.width(), mapTex.height(), mapTex.unit());
	m_gui->getWidget<gui::Image>("mapimage")->setImage(id);

}

WorldEditor::~WorldEditor() {
	clear();
}

void WorldEditor::clear() {
	// Remove and delete all objects
	for(base::HashMap<Object*>::iterator i=m_objects.begin(); i!=m_objects.end(); ++i) {
		delete i->value;
	}
	// Delete the editor module
	for(uint i=0; i<m_groups.size(); ++i) delete m_groups[i];
	m_groups.clear();
	m_file = 0;
	m_streams.clear();

	// Delete scene data
	delete m_terrain;

	// Delete tiles
	for(TerrainMap* map: m_maps) {
		for(EditableMap* t: map->maps) delete t;
		delete map->heightMap;
		delete map;
	}
	m_maps.clear();

	// delete materials
	if(m_materials) delete m_materials;
	if(m_foliage) delete m_foliage;
	m_materials = 0;
	m_foliage = 0;
	showMaterialList(0);
	showTextureList(0);

	// delete editor
	delete m_editor;
	m_editor = 0;

	// delete tools from dropdown list
	Combobox* list = m_gui->getWidget<Combobox>("toollist");
	if(list) {
		list->clearItems();
		m_activeGroup = 0;
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
	cam->updateFrustum();

	// Change speed with mouse wheel
	if(mw && mb==4) {
		m_options.speed *= 1 + mw * 0.1;
		mw = 0;
	}

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
	if(m_terrain && m_options.collide) {
		vec3 p = m_camera->getPosition();
		float h = m_terrain->getHeight( p );
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

		// Right click menu
		static int trigger = 0;
		static Point lp = mouse;
		if(mb==0 && trigger==4) {
			float t;
			if(!m_terrain->castRay(cp, cd, t)) t = -cp.y / cd.y;
			vec3 p = cp + cd * t;
			m_currentTile = m_terrain->getTile(p);
			TerrainMap* map = m_terrain->getMap(m_currentTile);
			for(int i=3; i<m_contextMenu->getWidgetCount(); ++i) m_contextMenu->getWidget(i)->setEnabled(map);
			m_editor->setTool(0);
			m_contextMenu->popup(m_gui, mouse);
		}
		if(mb==0) trigger = mb;
		else if(mouse!=lp) trigger |= 0x80;
		trigger = (trigger&0x80) | mb;
		lp = mouse;
	}

	// update foliage
	if(m_foliage) m_foliage->updateFoliage(m_camera->getPosition());


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
		i->value->update();
	}

	// Scene graph
	m_scene->updateSceneGraph();
}

void WorldEditor::drawScene() {
	// Render scene
	m_renderer->clear();
	m_renderer->getState().setCamera(m_camera);
	m_scene->collect(m_renderer, m_camera);
	m_renderer->render();
	m_renderer->getState().reset();

	// Draw editor stuff
	if(m_editor) m_editor->draw();
}

void WorldEditor::drawHUD() {
	scene::Material m;
	m.addPass()->blend = scene::BLEND_ALPHA;
	m.getPass(0)->bind();

	m_gui->draw();
}

void WorldEditor::updateTitle() {
	char title[1024];
	snprintf(title, 1024, "World Editor - %s", m_file.empty()? "Unnamed": m_file.str());
	Game::window()->setTitle(title);
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
	Widget* panel = b->getParent();
	int mode = panel->getWidget<Combobox>("mode")->getSelectedIndex();
	int size = panel->getWidget<Combobox>("size")->getSelectedData().getValue(129);
//	float height = panel->getWidget<Spinbox>("height")->getValue();
	float hmin = panel->getWidget<Spinbox>("minheight")->getValue();
	float hmax = panel->getWidget<Spinbox>("maxheight")->getValue();

	SaveFormat formats[] = { SaveFormat::RAW, SaveFormat::TIF16, SaveFormat::PNG16  };
	m_heightFormat = formats[mode];
	m_streaming = false; // This could be in the options ?
	m_resolution = 1;
	if(hmin >= hmax) m_heightRange.set(-1e8f, 1e8f);
	else m_heightRange.set(hmin, hmax);

	createNewTerrain(size);
	
	// Default material
	m_materials->addMaterial(0);
	m_materials->getMaterial()->setName("Default");
	
	// Create first tile
	TerrainMap* tile = createTile( "Tile 0,0" );
	m_terrain->assign( Point(0,0), tile );
	m_terrain->assign( Point(1,0), tile );
	m_terrain->assign( Point(0,1), tile );
	m_terrain->assign( Point(1,1), tile );
	m_minimap->build();
}
void WorldEditor::cancelNewTerrain(gui::Button*) {
	Widget* w = m_gui->getWidget<Widget>("newdialog");
	if(w) w->setVisible(false);
}
void WorldEditor::changeTerrainMode(gui::Combobox* list, int index) {
	Combobox* sizes = list->getParent()->getWidget<Combobox>("size");
	sizes->clearItems();
	char buffer[32];
	int min = 6, max=15; // powes of 2
	for(int i=min; i<=max; ++i) {
		int size = (1<<i) + 1;
		sprintf(buffer, "%dx%d", size, size);
		sizes->addItem(buffer, size);
	}
	sizes->selectItem(0);
	list->getParent()->getWidget<gui::Button>("create")->setEnabled( true );
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

void WorldEditor::createNewTerrain(int size) {
	clear();
	m_mapSize = size;

	// Create terrain
	m_terrain = new MapGrid( (size-1)*m_resolution, m_heightRange );
	m_terrain->eventMapCreated.bind(this, &WorldEditor::textureMapCreated);
	m_scene->add(m_terrain);


	// Setup material editor
	m_materials = new MaterialEditor(m_gui, m_fileSystem, m_streaming);
	m_materials->eventChangeMaterial.bind(this, &WorldEditor::setTerrainMaterial);
	m_materials->eventChangeTextureList.bind(this, &WorldEditor::textureListChanged);
	m_materials->setTerrainSize( vec2(size,size) );

	// Setup foliage editor
	m_foliage = new FoliageEditor(m_gui, m_fileSystem, m_terrain, m_scene);

	// Minimap
	m_minimap->setWorld(m_terrain);
	m_minimap->setRange(100);

	// Setup editor
	m_editor = new TerrainEditor(m_terrain);
	setupHeightTools(m_resolution);

	// Buttons
	ENABLE_BUTTON( "savemap" );
	ENABLE_BUTTON( "minimap" );
	ENABLE_BUTTON( "materialbutton" )
	ENABLE_BUTTON( "texturebutton" )
	ENABLE_BUTTON( "foliagebutton" )
	
}

void WorldEditor::createNewTile(Button*) {
	char name[32];
	int e = sprintf(name, "Tile %d,%d", m_currentTile.x, m_currentTile.y);
	int alt = 0;
	bool used = true;
	while(used) {
		used = false;
		for(TerrainMap* m: m_maps) {
			if(m->name == name) { sprintf(name+e, " [%d]", ++alt); used=true; break; }
		}
	}
	TerrainMap* map = createTile(name);
	m_terrain->assign(m_currentTile, map);
	m_contextMenu->hide();
}
void WorldEditor::showTileList(Button*) {
	// open dialogue
	gui::Window* w = m_gui->getWidget<gui::Window>("assigntile");
	char title[32]; sprintf(title, "Assign tile %d,%d", m_currentTile.x, m_currentTile.y);
	w->setCaption(title);
	w->setVisible(true);

	Listbox* list = w->getWidget(0)->cast<Listbox>();
	list->clearItems();
	for(TerrainMap* m: m_maps) list->addItem(m->name);
	m_contextMenu->hide();
}
void WorldEditor::duplicateTile(::Button*) {
	TerrainMap* src = m_terrain->getMap(m_currentTile);
	if(!src) return;

	TerrainMap* map = createTile(src->name);
	// Copy height data
	float* data = new float[src->heightMap->getDataSize()];
	src->heightMap->getData(data);
	map->heightMap->setData(data);
	delete [] data;
	// ToDo: copy texture maps too
	m_terrain->assign(m_currentTile, map);
	m_contextMenu->hide();
}
void WorldEditor::lockTile(Button*) {
	TerrainMap* map = m_terrain->getMap(m_currentTile);
	if(map) map->locked = !map->locked;
	m_contextMenu->hide();
}
void WorldEditor::loadTile(::Button*) {
	printf("Load tile not implemented\n");
	m_contextMenu->hide();
}
void WorldEditor::unloadTile(::Button*) {
	m_terrain->assign(m_currentTile, 0);
	m_contextMenu->hide();
}

void WorldEditor::assignTile(Listbox* list, int index) {
	if(index<0) return;
	m_terrain->assign(m_currentTile, m_maps[index]);
	list->getParent()->setVisible(false);
}
void WorldEditor::showRenameTile(Button*) {
	TerrainMap* map = m_terrain->getMap(m_currentTile);
	Widget* w = m_gui->getWidget<Widget>("renametile");
	if(map && w) {
		w->getWidget(0)->cast<Textbox>()->setText(map->name);
		w->setVisible(true);
	}
	m_contextMenu->hide();
}
void WorldEditor::renameTile(Button*) {
	TerrainMap* map = m_terrain->getMap(m_currentTile);
	Widget* w = m_gui->getWidget<Widget>("renametile");
	map->name = w->getWidget(0)->cast<Textbox>()->getText();
	w->setVisible(false);
}

// ----------------------------------------------------------- //

WorldEditor::ImageMapData* WorldEditor::createMapData(EditableTexture* tex, const char* name, const char* file, MapUsage usage) {
	ImageMapData* map = new ImageMapData();
	map->map = tex;
	map->map2 = 0;
	map->file = file;
	map->name = name;
	map->usage = usage;
	m_imageMaps.push_back(map);
	return map;
}

int WorldEditor::createUniqueMapName(const char* pattern, char* out) const {
	int index=0;
	do { sprintf(out, pattern, ++index); }
	while(false); // FIXME: broken
	return index;
}


void WorldEditor::createNewEditor(gui::Button* b) {
	cancelNewEditor(); // to hide window
	Widget* panel = b->getParent();
	int mode = panel->getWidget<Combobox>("editormode")->getSelectedIndex();
	const char* name = panel->getWidget<Textbox>("editorname")->getText();
	int size = atoi(panel->getWidget<Combobox>("editorsize")->getSelectedItem());
	if(size > 8192) return; // TODO: streaming textures
	ToolGroup* group = 0;
	int mapIndex = 0;
	int channels = 4;

	
	// Create tool group
	switch(mode) {
	case 0: // Colour map
		mapIndex = m_terrain->createTextureMap(size, channels, 0);
		group = new ColourToolGroup(name, mapIndex);
		break;
	case 1: // Weight map
		mapIndex = m_terrain->createTextureMap(size, channels, 1);
		group = new WeightToolGroup(name, channels, mapIndex);
		break;
	case 2: // Index map
		mapIndex = m_terrain->createTextureMap(size, channels, 2);
		if(channels>1) group = new IndexWeightToolGroup(name, mapIndex);
		else group = new IndexToolGroup(name, mapIndex);
		break;
	default: return;
	}

	// Register map with material editor
	m_materials->addMap(mapIndex, name, size, channels, mode);
	
	// Add group
	group->setup(m_gui);
	addGroup(group, "texture", true);

	// Set up a material for it ? - add checkbox option
	DynamicMaterial* mat = m_materials->createMaterial(name);
	MaterialLayer* layer = mat->addLayer(mode<2? LAYER_COLOUR: LAYER_INDEXED);
	layer->name = "Map";
	layer->mapIndex = mapIndex;
	m_materials->selectMaterial(mat);
}

void WorldEditor::cancelNewEditor(gui::Button*) {
	Widget* w = m_gui->getWidget<Widget>("neweditor");
	if(w) w->setVisible(false);

	Combobox* list = m_gui->getWidget<Combobox>("toollist");
	list->selectItem(0);
	selectToolGroup(list, 0);
}
void WorldEditor::cancelNewEditor(gui::Window*) { cancelNewEditor(); }
void WorldEditor::validateNewEditor(gui::Combobox* c, int) {
	Combobox* mode = m_gui->getWidget<Combobox>("editormode");
	Combobox* size = m_gui->getWidget<Combobox>("editorsize");
	bool valid = mode->getSelectedIndex()>=0 && size->getSelectedIndex()>=0;
	m_gui->getWidget<gui::Button>("editorcreate")->setEnabled(valid);
	// Initial name
	if(c==mode) {
		Textbox* name = m_gui->getWidget<Textbox>("editorname");
		name->setText(c->getSelectedItem());
	}
}

// ----------------------------------------------------------- //

void WorldEditor::changeViewDistance(Scrollbar*, int v) {
	m_options.distance = 1000 + v * 9;
	m_camera->adjustDepth(0.1, m_options.distance);
}
void WorldEditor::changeDetail(Scrollbar*, int v) {
	float value = v / 1000.f;
	m_options.detail = 16 - value * 15;
	for(TerrainMap* map: m_maps) map->heightMap->setDetail( m_options.detail );
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
	if(m_terrain && !w->isVisible()) showDialog(w);
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
	for(TerrainMap* map: m_maps) map->heightMap->setMaterial(m, map->maps);
}

void WorldEditor::textureListChanged() {
	int count = m_materials->getTextureCount();
	for(size_t i=0; i<m_groups.size(); ++i) {
		m_groups[i]->setTextures(count);
	}
}

void WorldEditor::textureMapCreated(TerrainMap* map) {
	DynamicMaterial* m = m_materials->getMaterial();
	map->heightMap->setMaterial(m, map->maps);
}

void WorldEditor::showFoliageList(Button*) {
	Widget* w = m_gui->getWidget<Widget>("foliage");
	if(m_materials && !w->isVisible()) showDialog(w);
	else w->setVisible(false);
}

void WorldEditor::selectToolGroup(Combobox* c, int index) {
	ToolGroup* group = 0;
	c->getItemData(index).read(group);
	m_editor->setTool(0);
	Widget* p = c->getParent();
	if(m_activeGroup) p->remove( m_activeGroup->getPanel() );
	// Set up toolbar for editor
	if(group) {
		p->add( group->getPanel() );
		group->getPanel()->setPosition( c->getPosition().x + c->getSize().x + 4, 0 );
		group->setActive();
		m_activeGroup = group;
	}
	// New group
	else {
		gui::Window* panel = m_gui->getWidget<gui::Window>("neweditor");
		if(panel) panel->setVisible(true);
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

void closeMessageBox(Button* b) {
	b->getParent()->setVisible(false);
}
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

void WorldEditor::setupHeightTools(float res) {
	GeometryToolGroup* group = new GeometryToolGroup();
	group->setup(m_gui);
	group->addTool("raise",   new HeightTool(), 0, 1);
	group->addTool("smooth",  new SmoothTool(), 0, 0);
	group->addTool("level",   new LevelTool(), 0, 1);
	group->addTool("flatten", new FlattenTool(), 0, 1);
	addGroup(group, "terrain", true);

	// Add 'New Layer' option
	Combobox* list = m_gui->getWidget<Combobox>("toollist");
	if(list) {
		int iconIndex = list->getIconList()->getIconIndex("neweditor");
		list->addItem("New Editor", group, iconIndex);
	}
}

void WorldEditor::addGroup(ToolGroup* group, const char* icon, bool select) {
	m_groups.push_back(group);
	Combobox* list = m_gui->getWidget<Combobox>("toollist");
	if(!list) return;
	int iconIndex = list->getIconList()->getIconIndex(icon);
	int index = list->getItemCount()? list->getItemCount()-1: 0;
	list->insertItem(index, group->getName(), group, iconIndex);
	group->eventToolSelected.bind(this, &WorldEditor::selectTool);
	group->setTextures( m_materials->getTextureCount() );
	// Select it
	if(select) {
		list->selectItem(index);
		selectToolGroup(list, index);
	}
}

// ======================= Loading and Saving Maps ========================== //

bool deleteFile(const char* file) {
	return remove(file) == 0;
}

inline int enumerate(const char* key, const char** values, int size) {
	for(int i=0; i<size; ++i) {
		if(strcmp(key, values[i])==0) return i;
	}
	return -1;
}

inline const char* cat(const char* a, const char* b, const char* c=0, const char* d=0) {
	static char buffer[1024];
	strcpy(buffer, a);
	if(b) strcat(buffer, b);
	if(c) strcat(buffer, c);
	if(d) strcat(buffer, d);
	return buffer;
}

inline const char* sanitise(const char* in) {
	static char buffer[1024];
	strcpy(buffer, in);
	char* b = buffer;
	for(const char* c=in; *c; ++c, ++b) {
		if(*c==' ') *b='_';
		else if(*c==',') *b = '.';
		else if(*c>='A' && *c<='Z') *b = *c + ('a'-'A');
		else *b = *c;
	}
	return buffer;
}

// ==================== Tiles ============================================================ //

TerrainMap* WorldEditor::createTile(const char* name) {
	TerrainMap* map = new TerrainMap;
/*	if(m_streaming) {
		Streamer* data = new Streamer(m_verticalScale);
		data->createStream("tmp.tiff", m_mapSize, m_mapSize, 1, m_heightFormat==HEIGHT_UINT8? 8: 16);
		data->setLod( m_options.detail );
		m_streams.push_back(data);
		map->heightMap = data;
		map->editor = new StreamingHeightmapEditor(data);
	}
	else*/ {
		DynamicHeightmap* data = new DynamicHeightmap();
		data->create(m_mapSize, m_mapSize, m_resolution, 0.f);
		data->setDetail( m_options.detail );
		map->heightMap = data;
		map->maps.push_back( new DynamicHeightmapEditor(data) );
	}
	map->heightMap->setMaterial(m_materials->getMaterial(), map->maps);
	map->heightMap->setHeightRange(m_heightRange);
	map->name = name;
	map->locked = false;
	m_maps.push_back(map);
	return map;
}

// ======================================================================================= //

bool saveHeightMapData(int size, float* data, SaveFormat format, const Rangef& range, const char* file) {
	uint count = size*size;
	if(format == SaveFormat::RAW) {
		FILE* fp = fopen(file, "wb");
		if(!fp) return false;
		fprintf(fp, "RAWFLOAT");
		fwrite(&count, 4, 1, fp);
		fwrite(data, 4, count, fp);
		fclose(fp);
		printf("Saved %s\n", file);
		return true;
	}

	if(format == SaveFormat::TIF16) {
		uint16* raw = new uint16[count];
		float mul = 0xffff / range.size();
		for(size_t i=0; i<count; ++i) raw[i] = (uint16)((range.clamp(data[i]) - range.min) * mul);
		TiffStream* tiff = TiffStream::createStream(file, size, size, 1, 16, TiffStream::WRITE, raw, count*2);
		if(tiff) delete tiff;
		delete [] raw;
		printf("Saved %s\n", file);
		return tiff;
	}

	return false;
}

bool loadHeightMapData(int size, float* data, const Rangef& range, const char* file) {
	const char* ext = strrchr(file, '.');
	if(strcmp(ext, ".tif")==0 || strcmp(ext, ".tiff")==0) {
		// Load tiff file
		TiffStream* tiff = TiffStream::openStream(file);
		if(!tiff) return false;
		if(tiff->bpp()==16 && tiff->channels()==1) {
			uint16* raw = new uint16[tiff->width() * tiff->height()];
			tiff->readBlock(0, 0, size, size, raw);
			uint count = size * size;
			float scale = range.size() / 0xffff;
			for(uint i=0; i<count; ++i) data[i] = raw[i] * scale + range.min;
			delete [] raw;
			printf("Loaded %s\n", file);
			return true;
		}
		else printf("Invalid tiff heightmap: %s: Must be uncompressed 16bit greyscale", file);
		delete tiff;
	}
	else if(strcmp(ext, ".raw")==0) {
		FILE* fp = fopen(file, "rb");
		if(!fp) return false;
		char header[8];
		fread(header, 1, 8, fp);
		if(memcmp(header, "RAWFLOAT", 8)==0) {
			int total = 0;
			fread(&total, 4, 1, fp);
			if(total > size*size) total = size*size; // Invalid ??
			fread(data, 4, total, fp);
		}
		else printf("Invalid raw float data file: %s\n", file);
		fclose(fp);
		printf("Loaded %s\n", file);
		return true;
	}
	return false;
}

// ----------------------------------------------------------------------------------- //

void WorldEditor::saveWorld(const char* file) {
	if(!file && !m_file) return;
	if(m_file != file) m_file = file;
	m_fileSystem->setRootPath(m_file, true);
	char buffer[1024];
	updateTitle();

	char name[128];
	const char* nameStart = strrchr(file, '/');
	strcpy(name, nameStart? nameStart+1: file);
	char* removeExtension = strrchr(name, '.');
	if(removeExtension) *removeExtension = 0;

	// Flush all streams
	for(uint i=0; i<m_streams.size(); ++i) m_streams[i]->flush();

	// Write xml file
	XML xml;
	xml.setRoot( XMLElement("scene") );
	xml.getRoot().setAttribute("mapsize", m_mapSize);
	xml.getRoot().setAttribute("min", m_heightRange.min);
	xml.getRoot().setAttribute("max", m_heightRange.max);

	// Terrain
	for(TerrainMap* map: m_maps) {
		// Write Terrain
		XMLElement& e = xml.getRoot().add("terrain");
		e.setAttribute("name", map->name);
		e.setAttribute("size", m_mapSize);
		if(map->locked) e.setAttribute("locked", 1);
		String mapName = cat(name, "_", sanitise(map->name));

		// Save height data
		const char* ext[] = { ".raw", ".tif", ".png" };
		if(!map->file) map->file = m_fileSystem->getUniqueFile( cat(mapName, ext[(int)m_heightFormat] ) );
		size_t size = map->heightMap->getDataSize();
		float* rawData = new float[size];
		map->heightMap->getData(rawData);
		saveHeightMapData(m_mapSize, rawData, m_heightFormat, m_heightRange, m_fileSystem->getFile(map->file));
		delete [] rawData;
		
		// Height Data file name
		XMLElement& data = e.add("data");
		data.setAttribute("file", map->file);

		// Texture maps
		for(size_t i=1; i<map->maps.size(); ++i) {
			if(!map->maps[i]) continue;
			sprintf(buffer, "%s_map%d.png", mapName.str(), (int)i);
			XMLElement& tex = e.add("map");
			tex.setAttribute("index", (int)i);
			tex.setAttribute("file", buffer);
			// save images
			static_cast<EditableTexture*>(map->maps[i])->save(buffer);
		}
	}

	// Overlays
	for(uint i=0; m_materials->getMap(i).flags; ++i) {
		const MaterialEditor::MapData& map = m_materials->getMap(i);
		XMLElement& e = xml.getRoot().add("overlay");
		const char* modes [] = { "weight", "colour", "index" };
		e.setAttribute("index", (int)i);
		e.setAttribute("name", map.name);
		e.setAttribute("size", map.size);
		e.setAttribute("channels", map.channels);
		e.setAttribute("type", modes[map.flags-1]);
	}

	// Grid
	std::vector<Point> slots = m_terrain->getUsedSlots();
	for(Point& p: slots) {
		XMLElement& slot = xml.getRoot().add("tile");
		slot.setAttribute("x", p.x);
		slot.setAttribute("y", p.y);
		slot.setAttribute("map", m_terrain->getMap(p)->name);
	}

	// Materials
	XMLElement& m = xml.getRoot().add("materials");
	m.setAttribute("active", m_materials->getMaterial()->getName());

	// Materials
	for(int i=0; i<m_materials->getMaterialCount(); ++i) {
		m.add( m_materials->serialiseMaterial(i) );
	}
	// Textures
	for(int i=0; i<m_materials->getTextureCount(); ++i) {
		m.add( m_materials->serialiseTexture(i) );
	}
	// Foliage
	if(m_foliage) xml.getRoot().add( m_foliage->save() );


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
// ----------------------------------------------------------------------------------- //
void WorldEditor::loadWorld(const char* file) {
	XML xml = XML::load(file);
	if(!(xml.getRoot() == "scene")) {
		messageBox("Load error", "Invalid scene file");
		return;
	}

	m_mapSize = xml.getRoot().attribute("mapsize", 0);
	if(m_mapSize==0) return;
	m_streaming = false;
	m_resolution = 1;
	m_heightRange.set(xml.getRoot().attribute("min",0.f), xml.getRoot().attribute("max", 0.f));

	createNewTerrain(m_mapSize);
	m_fileSystem->setRootPath(file, true);
	m_file = file;

	// Load overlay definitions
	for(const XMLElement& e: xml.getRoot()) {
		if(e=="overlay") {
			const char* modes [] = { "weight", "colour", "index" };
			const char* name = e.attribute("name");
			int index = e.attribute("index",0);
			int size = e.attribute("size",0);
			int channels = e.attribute("channels", 0);
			int flags = enumerate(e.attribute("type"), modes, 3);
			m_materials->addMap(index, name, size, channels, flags);
		}
	}

	// Load materials
	const XMLElement& mat = xml.getRoot().find("materials");
	for(const XMLElement& e: mat) {
		if(e == "material") m_materials->loadMaterial(e);
		else if(e == "texture") m_materials->loadTexture(e, m_materials->getTextureCount());
	}
	m_materials->buildTextures();	// Build texture arrays
	if(m_materials->getMaterialCount()==0) {
		m_materials->addMaterial(0);
		m_materials->getMaterial()->setName("Default");
	}
	else {
		DynamicMaterial* active = m_materials->getMaterial(mat.attribute("active"));
		if(!active) active = m_materials->getMaterial(0);
		m_materials->selectMaterial(active);
	}
	
	// Load terrain tiles
	for(const XMLElement& e: xml.getRoot()) {
		if(e == "terrain") {
			int size = e.attribute("size", 0);
			const char* mapFile = e.find("data").attribute("file");
			if(size != m_mapSize) { printf("Error: Map size mismatch\n"); continue; }
			TerrainMap* map = createTile(e.attribute("name"));
			map->locked = e.attribute("locked", 0);
			map->file = mapFile;
			// load data
			float* raw = new float[size*size];
			if(loadHeightMapData(size, raw, m_heightRange, m_fileSystem->getFile(map->file))) {
				map->heightMap->setData(raw);
			}
			delete [] raw;

			for(const XMLElement& m: e) {
				if(m=="map") {
					// Load maps - ToDo: type for double map
					size_t index = m.attribute("index", 0);
					String file = m_fileSystem->getFile(m.attribute("file"));
					EditableTexture* tex = new EditableTexture(file, true);
					if(map->maps.size()<=index) map->maps.resize(index+1, 0);
					map->maps[index] = tex;
				}
			}

			// Height format
			const char* ext = strrchr(map->file, '.');
			if(ext && (strcmp(ext, ".tif")==0 || strcmp(ext, ".tiff")==0)) m_heightFormat = SaveFormat::TIF16;
			else m_heightFormat = SaveFormat::RAW;
		}
	}

	// Load map grid
	for(const XMLElement& e: xml.getRoot()) {
		if(e == "tile") {
			Point point;
			TerrainMap* map = 0;
			point.x = e.attribute("x", 0);
			point.y = e.attribute("y", 0);
			const char* name = e.attribute("map");
			for(TerrainMap* m: m_maps) if(m->name==name) { map=m; break; }
			if(map) m_terrain->assign(point, map);
			else printf("Load error: Map %s not found\n", name);
		}
	}
	
	// load foliage
	if(m_foliage) m_foliage->load( xml.getRoot().find("foliage") );


	m_minimap->build();
}



