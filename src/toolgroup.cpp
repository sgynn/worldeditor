#include "toolgroup.h"
#include "gui/skin.h"
#include "gui/widgets.h"
#include "widgets/colourpicker.h"
#include "toolgroup.h"

using namespace gui;

ToolGroup::ToolGroup(const char* name): m_root(0), m_panel(0), m_currentTool(0) {
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

Button* ToolGroup::addButton(const char* icon) {
	// Create gui button
	int index = m_panel->getWidgetCount();
	Button* button = m_root->createWidget<Button>("menuoption");
	button->setPosition( index * 34 + 2, 2);
	if(icon) button->setIcon(icon);
	button->eventPressed.bind(this, &ToolGroup::selectTool);
	m_panel->add(button);
	m_panel->setSize(index * 34 + 36, 36);
	return button;
}

void ToolGroup::selectTool(Button* b) {
	// Assume buttons are in order
	for(m_currentTool=0; m_currentTool<m_tools.size(); ++m_currentTool) {
		if(m_panel->getWidget(m_currentTool) == b) break;
	}
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

void GeometryToolGroup::setResolution(const vec2& offset, const vec2& size, float res) {
	for(size_t i=0; i<m_tools.size(); ++i) {
		m_tools[i]->tool->setResolution(res, offset);
	}
}

// --------------------------------------------------------------------------------------------- //

WeightToolGroup::WeightToolGroup(const char* name, EditableTexture* image) : ToolGroup(name) {
	m_tool = new TextureTool(image);
	// Create tool for each channel
	for(int i=0; i<image->getChannels(); ++i) {
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

void WeightToolGroup::setResolution(const vec2& offset, const vec2& size, float res) {
	res = (float) size.x / m_tool->texture->getWidth();
	m_tool->setResolution(res, offset);
}

// --------------------------------------------------------------------------------------------- //

IndexToolGroup::IndexToolGroup(const char* name, EditableTexture* ix) : ToolGroup(name) {
	m_tool = new IndexTool(ix);
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
void IndexToolGroup::setResolution(const vec2& offset, const vec2& size, float res) {
	res = (float) size.x / m_tool->indexMap->getWidth();
	m_tool->setResolution(res, offset);
}

// --------------------------------------------------------------------------------------------- //

IndexWeightToolGroup::IndexWeightToolGroup(const char* name, EditableTexture* ix, EditableTexture* wt) : IndexToolGroup(name, ix) {
	delete m_tool;
	m_tool = new IndexWeightTool(ix, wt);
}



// --------------------------------------------------------------------------------------------- //


ColourToolGroup::ColourToolGroup(const char* name, EditableTexture* image) : ToolGroup(name) {
	m_tool = new ColourTool(image);
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
void ColourToolGroup::setResolution(const vec2& offset, const vec2& size, float res) {
	res = (float) size.x / m_tool->texture->getWidth();
	m_tool->setResolution(res, offset);
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

