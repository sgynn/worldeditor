#pragma once

#include <base/vec.h>
#include <base/point.h>
#include <base/xml.h>
#include "gui/delegate.h"

class MapGrid;
class TerrainMap;
class FileSystem;
namespace gui { class Root; class Widget; class Button; class Window; }
namespace base { class Camera; struct Mouse; }
namespace scene { class SceneNode; }

enum KeyMask { CTRL_MASK=1, SHIFT_MASK=2, ALT_MASK=4 };

using Mouse = base::Mouse;

struct InputState {
	const int keyMask;
	const bool overGUI;
	bool consumedMouseWheel;
	bool consumedMouseDown;
};

// Generic plugin interface
class EditorPlugin {
	protected:
	EditorPlugin() {}
	public:
	virtual ~EditorPlugin();
	virtual void update(const base::Mouse&, const Ray&, base::Camera*, InputState& state) {}		// frame update
	virtual void setContext(const TerrainMap*) {}													// Set active terrain data
	virtual base::XMLElement save(const TerrainMap* context) const { return base::XMLElement(); }	// Save data to file
	virtual void load(const base::XMLElement&, const TerrainMap* context) {};						// Load data from file
	virtual void clear() {}		// ?
	virtual void close() {}		// Close all associated gui elements
	virtual void activate() {}	// Make plugin active - open panel etc.

	void registerPlugin(gui::Widget* toolPanel);
	Delegate<void(EditorPlugin*)> eventActivated;
	gui::Widget* getPanel() const { return m_panel; }
	void closeEditor();
	protected:
	bool createPanel(gui::Root*, const char* name, const char* file=0);
	void createToolButton(gui::Root*, const char* icon);
	private:
	void panelClosed(gui::Window*);
	void toggleEditor(gui::Button*);
	gui::Widget* m_toolButton = 0;
	protected:
	gui::Widget* m_panel = 0;
};




