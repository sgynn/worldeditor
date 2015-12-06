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
#include "widgets/orderableitem.h"

#include <cstdarg>

#include "terraineditor/heighttools.h"
#include "terraineditor/texturetools.h"

#include "dynamicmaterial.h"

using namespace gui;
using namespace base;

#define INIFILE "settings.cfg"


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


/** Make a world - this file will be a complete mess while I test stuff */

WorldEditor::WorldEditor(const INIFile& ini) : m_editor(0), m_heightMap(0) {
	m_renderer = new Render;
	m_library = new Library;
	m_library->addPath("data");

	// Load editor options
	INIFile::Section options = ini["settings"];
	m_options.speed      = options.get("speed", 4);
	m_options.detail     = options.get("detail", 4);
	m_options.collide    = options.get("collision", true);
	m_options.tabletMode = options.get("tablet", true);
	m_options.distance   = options.get("distance", 10000);

	// Run an fps camera for now
	base::FPSCamera* cam = new base::FPSCamera(90, base::Game::aspect(), 0.01, m_options.distance);
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
	m_gui->load("gui.xml");

	// Setup event callbacks
	m_gui->getWidget<Button>("newmap")->eventPressed.bind(this, &WorldEditor::showNewDialog);
	m_gui->getWidget<Button>("openmap")->eventPressed.bind(this, &WorldEditor::showOpenDialog);
	m_gui->getWidget<Button>("savemap")->eventPressed.bind(this, &WorldEditor::showSaveDialog);
	m_gui->getWidget<Button>("options")->eventPressed.bind(this, &WorldEditor::showOptionsDialog);
	m_gui->getWidget<Combobox>("toollist")->eventSelected.bind(this, &WorldEditor::selectToolGroup);

	m_gui->getWidget<Scrollbar>("brushsize")->eventChanged.bind(this, &WorldEditor::changeBrushSlider);
	m_gui->getWidget<Scrollbar>("brushstrength")->eventChanged.bind(this, &WorldEditor::changeBrushSlider);
	m_gui->getWidget<Scrollbar>("brushfalloff")->eventChanged.bind(this, &WorldEditor::changeBrushSlider);

	// New map dialog
	Widget* newPanel = m_gui->getWidget<Widget>("newdialog");
	newPanel->getWidget<Button>("create")->eventPressed.bind(this, &WorldEditor::createNewTerrain);
	newPanel->getWidget<Button>("cancel")->eventPressed.bind(this, &WorldEditor::cancelNewTerrain);
	newPanel->getWidget<Combobox>("mode")->eventSelected.bind(this, &WorldEditor::changeTerrainMode);
	newPanel->getWidget<Button>("sourcebutton")->eventPressed.bind(this, &WorldEditor::browseTerrainSource);

	// Settings
	m_gui->getWidget<Scrollbar>("viewdistance")->eventChanged.bind(this, &WorldEditor::changeViewDistance);
	m_gui->getWidget<Scrollbar>("terraindetail")->eventChanged.bind(this, &WorldEditor::changeDetail);
	m_gui->getWidget<Scrollbar>("cameraspeed")->eventChanged.bind(this, &WorldEditor::changeSpeed);
	m_gui->getWidget<Checkbox>("tabletmode")->eventChanged.bind(this, &WorldEditor::changeTabletMode);
	m_gui->getWidget<Checkbox>("collision")->eventChanged.bind(this, &WorldEditor::changeCollision);
	m_gui->getWidget<gui::Window>("settings")->eventClosed.bind(this, &WorldEditor::saveSettings);

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
	FPSCamera* cam = static_cast<base::FPSCamera*>(m_camera);
	cam->setEnabled( mb&4 );
	cam->grabMouse( (mb&4) && !m_options.tabletMode );
	cam->update();

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
		if(mw) updateBrushSliders();
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
	if(w) w->setVisible(true);
}
void WorldEditor::showOpenDialog(Button*) {
	FileDialog* d = m_gui->getWidget<FileDialog>("filedialog");
	d->eventConfirm.bind(this, &WorldEditor::loadWorld);
	d->setFilter("*.xml");
	d->setFileName("");
	d->showOpen();
}
void WorldEditor::showSaveDialog(Button*) {
	FileDialog* d = m_gui->getWidget<FileDialog>("filedialog");
	d->eventConfirm.bind(this, &WorldEditor::saveWorld);
	d->setFilter("*.xml");
	d->setFileName("");
	d->showSave();
}
void WorldEditor::showOptionsDialog(Button*) {
	Widget* w = m_gui->getWidget<Widget>("settings");
	if(w) w->setVisible(true);
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
	case 0: create(size, 1, 1, false); break;
	case 1: create(size, 1, height/255.0, false); break;
	case 2: create(size, 1, height/65535.0, false); break;
	case 3: create(size, 1, height/255.0, true); break;
	case 4: create(size, 1, height/65535.0, true); break;
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
	if(m_streaming) {
		float value = v / 1000.f;
		m_options.detail = 16 - value * 15;
		// Umm - need access somehow...
	}
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
	ini.save(INIFILE);
}

// ----------------------------------------------------------- //


void WorldEditor::selectToolGroup(Combobox* c, int index) {
	ToolGroup* g = *c->getItemData(index).cast<ToolGroup*>();
	Widget* p = c->getParent();
	if(p->getWidgetCount() > 1) p->remove( p->getWidget(1) );
	p->add( g->getPanel() );
	g->getPanel()->setPosition( c->getSize().x + 4, 0 );
	m_editor->setTool(0);
	g->getPanel()->getWidget(0)->setSelected(false);
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

void readAutoParams(const XMLElement& e, AutoParams& p) {
	p.min   = e.attribute("min", p.min);
	p.max   = e.attribute("max", p.max);
	p.blend = e.attribute("blend", p.blend);
	p.noise = e.attribute("noise", p.noise);
}

void WorldEditor::create(int size, float res, float scale, bool streamed) {
	if(streamed) {
		Streamer* map = new Streamer(scale);
		map->createStream("tmp.tiff", size, size, 1, 16);
		map->setLod( m_options.detail );
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
	int size = terrain.attribute("width", 0);
	const XMLElement& info = terrain.find("data");
	float res = info.attribute("resolution", 1.f);
	float scale = info.attribute("scale", 1.f);
	const char* source = info.attribute("file");
	const char* format = info.attribute("type");

	Streamer* streamer = 0;
	SimpleHeightmap* simple = 0;

	m_library->addPath(path);

	// Material
	const XMLElement& matData = terrain.find("material");
	const char* material = matData.attribute("file", "default.mat");

	// Load materials
	const char* layerTypes[] = { "auto", "weight", "colour", "indexed" };
	const char* blendModes[] = { "normal", "height", "multiply", "add" };
	const char* channels[]   = { "r", "g", "b", "a", "x" };
	for(XML::iterator i=terrain.begin(); i!=terrain.end(); ++i) {
		if(*i == "material") {
			DynamicMaterial* mat = new DynamicMaterial();
			mat->setName( i->attribute("name") );
			m_materials.push_back(mat);
			// read layers
			for(XML::iterator j=i->begin(); j!=i->end(); ++j) {
				if(*j=="layer") {
					int mode = enumerate(j->attribute("type", "normal"), layerTypes, 4);
					MaterialLayer* layer = mat->addLayer((LayerType) mode);
					layer->name = j->attribute("name");
					layer->blend = (::BlendMode) enumerate(j->attribute("blend", "normal"), blendModes, 4);
					layer->blendScale = j->attribute("blendscale", 1.f);
					layer->opacity = j->attribute("opacity", 1.f);
					layer->texture = j->attribute("texture", -1);
					layer->colour = j->attribute("colour", 0xffffff);
					layer->triplanar = j->attribute("triplanar", 0);
					layer->scale = vec3(1,1,1) * j->attribute("scale", 10.f);

					if(mode<3) {
						layer->map = j->attribute("map");
						layer->mapData = enumerate(j->attribute("channel", "r"), channels, 5);
					} else {
						layer->map = j->attribute("weightmap");
						layer->map2 = j->attribute("indexmap");
						layer->mapData = j->attribute("offset", 0);
					}

					if(mode==0) {
						readAutoParams( j->find("height"), layer->height );
						readAutoParams( j->find("slope"), layer->slope );
						readAutoParams( j->find("concavity"), layer->concavity );
					}
				}
			}
			mat->compile();
		}
	}


	// Create terrain - maybe use a plugin system for different types?
	if(info.attribute("stream", 0)) {
		streamer = new Streamer(scale);
		if(streamer->openStream( cat(path, source) )) {
			streamer->addToScene(m_renderer);
			streamer->setMaterial( m_library->material(material)  );
			streamer->setLod( m_options.detail );
			m_heightMap = new StreamingHeightmapEditor(streamer);
			m_objects["terrain"] = streamer;
			m_terrainOffset = streamer->getOffset().xz();
			m_streaming = true;
			size = streamer->width();
			if(size != streamer->height()) messageBox("Warning", "Terrian map is not square");
		} else {
			messageBox("Load error", "Failed to open stream %s", cat(path, source));
			return;	
		}
	}
	else {
		if(strcmp(format, "float")==0) {
			simple = new SimpleHeightmap();
			File data = File::load( cat(path, source) );
			simple->create(size, size, res, (const float*) data.contents());
			simple->addToScene(m_renderer);
			simple->setMaterial( m_library->material(material)  );
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
				if(ts->openStream(buffer)) tex = new EditableTexture(ts);
				else messageBox("Error", "Failed to open stream %s", file);
				
				// Add to streamer
				ts->initialise(2048, true);
				if(tex && streamer) streamer->addTexture(name, ts);
			}
			else if(stream) {
				BufferedStream* bs = new BufferedStream();
				if(bs->openStream(buffer)) tex = new EditableTexture(bs);
				else messageBox("Error", "Failed to open stream %s", file);
			}
			else {
				tex = new EditableTexture(buffer, use>0);
			}

			if(!tex) continue;
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
				g->setup(m_gui);
				g->setResolution(m_terrainOffset, vec2(size,size), res);
				addGroup(g, "texture");
			}
		}
	}
}
void WorldEditor::saveWorld(const char* file) {
	

}


