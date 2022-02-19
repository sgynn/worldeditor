#include "toolgroup.h"
#include <base/gui/skin.h>
#include <base/gui/widgets.h>
#include <base/gui/layouts.h>
#include "widgets/colourpicker.h"
#include "toolgroup.h"

using namespace gui;

ToolGroup::ToolGroup(const char* name): m_root(0), m_panel(0), m_currentTool(-1) {
	strncpy(m_name, name, 64);
}

ToolGroup::~ToolGroup() {
	if(m_panel && m_panel->getParent()) {
		m_panel->getParent()->remove(m_panel);
		delete m_panel;
	}
	for(size_t i=0; i<m_tools.size(); ++i) {
		delete m_tools[i];
	}
}

void ToolGroup::setup(Root* root) {
	m_root = root;
	m_panel = root->createWidget<Widget>( Rect(0,0,64,36), "default" );
	m_panel->setLayout(new HorizontalLayout());
	m_panel->setAutosize(true);
}

ToolInstance* ToolGroup::getTool() const {
	if(m_currentTool < m_tools.size()) return m_tools[m_currentTool];
	else return 0;
}

Widget* ToolGroup::getPanel() const {
	return m_panel;
}

const char* ToolGroup::getName() const {
	return m_name;
}

void ToolGroup::deselect() {
	Widget* w = m_panel->getWidget(m_currentTool);
	if(w) w->setSelected(false);
	m_currentTool = -1;
	if(eventToolSelected) eventToolSelected( 0 );
}

Button* ToolGroup::addButton(const char* icon) {
	Button* button = m_root->createWidget<Button>("menuoption");
	if(icon) button->setIcon(icon);
	button->eventPressed.bind(this, &ToolGroup::selectTool);
	m_panel->add(button);
	return button;
}

void ToolGroup::selectTool(Button* b) {
	m_currentTool = b->getIndex();
	for(Widget* w: *m_panel) w->setSelected(w==b);
	if(eventToolSelected) eventToolSelected( getTool() );
}

void ToolGroup::addTool(Tool* tool, int flags, int shift) {
	ToolInstance* inst = new ToolInstance;
	inst->tool = tool;
	inst->flags = flags;
	inst->shift = shift;
	m_tools.push_back(inst);
}

void ToolGroup::setActive() {
	if(m_panel->getWidgetCount()) m_panel->getWidget(0)->setSelected(false);
}

// --------------------------------------------------------------------------------------------- //

GeometryToolGroup::GeometryToolGroup() : ToolGroup("Geometry") {
}
GeometryToolGroup::~GeometryToolGroup() {
	for(size_t i=0; i<m_tools.size(); ++i) {
		delete m_tools[i]->tool;
	}
}
void GeometryToolGroup::addTool(const char* icon, Tool* tool, int flag, int shift) {
	addButton(icon);
	ToolGroup::addTool(tool, flag, shift);
}


// --------------------------------------------------------------------------------------------- //

WeightToolGroup::WeightToolGroup(const char* name, int channels, unsigned map) : ToolGroup(name) {
	m_tool = new TextureTool(map);
	// Create tool for each channel
	for(int i=0; i<channels; ++i) {
		addTool(m_tool, i|4, i);
	}
}
WeightToolGroup::~WeightToolGroup() {
	delete m_tool;
}

void WeightToolGroup::setup(Root* r) {
	ToolGroup::setup(r);
	const char* icons[] = { "red", "green", "blue", "alpha" };
	for(size_t i=0; i<m_tools.size(); ++i) {
		addButton( icons[i] );
	}
}

void WeightToolGroup::setChannel(int index, const char* icon) {
	Button* b = m_panel->getWidget(index)->cast<Button>();
	if(b) b->setIcon(icon);
}

// --------------------------------------------------------------------------------------------- //

IndexToolGroup::IndexToolGroup(const char* name, unsigned map) : ToolGroup(name) {
	m_tool = new IndexTool(map);
}
IndexToolGroup::~IndexToolGroup() {
	delete m_tool;
}
void IndexToolGroup::setTextures(int count) {
	int existing = m_panel->getWidgetCount();
	gui::IconList* icons = m_root->getIconList("textureIcons");
	// Add new ones
	for(int i=existing; i<count; ++i) {
		Button* b = addButton();
		b->setIcon(icons, -1);
		addTool(m_tool, i, i);
	}
	// Update icons
	for(int i=0; i<count; ++i) {
		Button* b = m_panel->getWidget(i)->cast<Button>();
		if(i>=icons->size() || icons->getIconRect(i).width==0) b->setIcon(-1);
		else b->setIcon(i);
	}

	// Remove extraneous
	for(int i=count; i<existing; ++i) {
		Widget* button = m_panel->getWidget(i);
		m_panel->remove(button);
		delete button;
		delete m_tools.back();
		m_tools.pop_back();
	}
}

// --------------------------------------------------------------------------------------------- //

IndexWeightToolGroup::IndexWeightToolGroup(const char* name, unsigned map) : IndexToolGroup(name, map) {
	delete m_tool;
	m_tool = new IndexWeightTool(map);
}



// --------------------------------------------------------------------------------------------- //


ColourToolGroup::ColourToolGroup(const char* name, unsigned map) : ToolGroup(name) {
	m_tool = new ColourTool(map);
	addTool(m_tool, 0xffffff, 0);
	m_colour = white;
}
ColourToolGroup::~ColourToolGroup() {
	delete m_tool;
}
void ColourToolGroup::setup(Root* r) {
	ToolGroup::setup(r);
	Button* b = addButton( "white" );
	b->eventPressed.bind(this, &ColourToolGroup::openPicker);
}

void ColourToolGroup::openPicker(Button* b) {
	ColourPicker* picker = m_root->getWidget<ColourPicker>("picker");
	if(picker) {
		picker->setColour( m_colour );
		picker->setVisible(true);
		picker->eventSubmit.bind(this, &ColourToolGroup::colourChanged);
	}
	selectTool(b);
}
void ColourToolGroup::colourChanged(const Colour& c) {
	m_colour = c;
	m_panel->getWidget(0)->cast<Button>()->setIconColour(c);
	getTool()->flags = c.toRGB();
}

void ColourToolGroup::setActive() {
	Button* b = m_panel->getWidget(0)->cast<Button>();
	selectTool(b);
}

