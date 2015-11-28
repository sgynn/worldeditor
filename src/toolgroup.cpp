#include "toolgroup.h"
#include "gui/widgets.h"
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

void ToolGroup::addButton(const char* icon) {
	// Create gui button
	int index = m_panel->getWidgetCount();
	Button* button = m_root->createWidget<Button>("menuoption");
	button->setPosition( index * 34 + 2, 2);
	if(icon) button->setIcon(icon);
	button->eventPressed.bind(this, &ToolGroup::selectTool);
	m_panel->add(button);
	m_panel->setSize(index * 34 + 36, 36);
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

MaterialToolGroup::MaterialToolGroup(EditableTexture* ix, EditableTexture* wt) : ToolGroup("Indexed Material") {
	m_tool = new MaterialTool(wt, ix);
}
MaterialToolGroup::~MaterialToolGroup() {
	delete m_tool;
}
void MaterialToolGroup::addTexture(const char* icon, int index) {
	addButton(icon);
	addTool(m_tool, index, index);
}
void MaterialToolGroup::setResolution(const vec2& offset, const vec2& size, float res) {
	res = (float) size.x / m_tool->weightMap->getWidth();
	m_tool->setResolution(res, offset);
}

// --------------------------------------------------------------------------------------------- //


ColourToolGroup::ColourToolGroup(const char* name, EditableTexture* image) : ToolGroup(name) {
	m_tool = new ColourTool(image);
}
ColourToolGroup::~ColourToolGroup() {
	delete m_tool;
}
void ColourToolGroup::setup(Root* r) {
	ToolGroup::setup(r);
}
void ColourToolGroup::setResolution(const vec2& offset, const vec2& size, float res) {
	res = (float) size.x / m_tool->texture->getWidth();
	m_tool->setResolution(res, offset);
}


