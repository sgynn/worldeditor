#include "editorplugin.h"
#include "gui/widgets.h"

using namespace gui;

extern String appPath;

EditorPlugin::~EditorPlugin() {
	if(m_toolButton) m_toolButton->removeFromParent();
	delete m_toolButton;
}

bool EditorPlugin::createPanel(gui::Root* gui, const char* name, const char* file) {
	m_panel = gui->getWidget(name);
	if(!m_panel && file) {
		gui->load(appPath + "data/" + file);
		m_panel = gui->getWidget(name);
	}
	if(m_panel) {
		m_panel->setVisible(false);
		Window* window = m_panel->cast<Window>();
		if(window) window->eventClosed.bind(this, &EditorPlugin::panelClosed);
	}
	return m_panel;
}

void EditorPlugin::createToolButton(gui::Root* gui, const char* icon) {
	Button* button = gui->createWidget<Button>("iconbuttondark");
	button->setIcon(icon);
	button->eventPressed.bind(this, &EditorPlugin::toggleEditor);
	m_toolButton = button;
}

void EditorPlugin::registerPlugin(Widget* toolPanel) {
	if(m_toolButton) toolPanel->add(m_toolButton, 0);
}

void EditorPlugin::panelClosed(Window*) {
	if(m_toolButton) m_toolButton->setSelected(false);
	close();
}

void EditorPlugin::toggleEditor(Button* b) {
	b->setSelected(!b->isSelected());
	if(m_panel) m_panel->setVisible(b->isSelected());
	if(b->isSelected()) {
		activate();
		if(eventActivated) eventActivated(this);
	}
	else close();
}

void EditorPlugin::closeEditor() {
	if(m_panel) m_panel->setVisible(false);
	if(m_toolButton) m_toolButton->setSelected(false);
	close();
}

