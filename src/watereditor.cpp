#include "watereditor.h"
#include "water/water.h"
#include <base/gui/lists.h>
#include <base/scene.h>

using namespace gui;
using namespace base;

#define CONNECT(Type, name, event, callback) { Type* t=m_panel->getWidget<Type>(name); if(t) t->event.bind(this, &WaterEditor::callback); else printf("Missing widget: %s\n", name); }

WaterEditor::WaterEditor(Root* gui, FileSystem*, MapGrid* map, SceneNode* scene) : m_terrain(map) {
	m_waterSystem = new WaterSystem();

	createPanel(gui, "watereditor", "water.xml");
	createToolButton(gui, "Water");
	m_node = scene->createChild("Water");
	
	m_list = m_panel->getWidget<Listbox>("waterlist");
	CONNECT(Button, "removewater", eventPressed, deleteItem);
	CONNECT(Button, "dupicatewater", eventPressed, duplicateItem);
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
void WaterEditor::update(const Mouse&, const Ray&, base::Camera*, InputState&) {
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
	}
	else {
		m_lake = m_waterSystem->addLake(type==1);
		m_list->addItem(type==1?"Lake":"Ocean", m_river, 38);
	}
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
}


