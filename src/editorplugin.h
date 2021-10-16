#pragma once

#include <base/vec.h>
#include <base/point.h>
#include <base/xml.h>

class MapGrid;
class TerrainMap;
class FileSystem;
namespace gui { class Widget; }
namespace base { class Camera; struct Mouse; }

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
	virtual ~EditorPlugin() {}
	virtual void setup(gui::Widget* toolPanel) {}	// initialisation
	virtual void update(const base::Mouse&, const Ray&, base::Camera*, InputState& state) {}		// frame update
	virtual void setContext(const TerrainMap*) {}													// Set active terrain data
	virtual base::XMLElement save(const TerrainMap* context) const { return base::XMLElement(); }	// Save data to file
	virtual void load(const base::XMLElement&, const TerrainMap* context) {};						// Load data from file
	virtual void clear() {}		// ?
	virtual void close() {}		// Close all assodiated gui elements
};




