#pragma once

#include <base/gui/widgets.h>

namespace gui {
	class Listbox;
	class ListItem;
}

class FileDialog : public gui::Window {
	WIDGET_TYPE(FileDialog);
	FileDialog(const Rect&, gui::Skin*);
	~FileDialog();


	/** Set the current directory of the dialog */
	void setDirectory(const char* dir);
	/** Set the current file name */
	void setFileName(const char* name);
	/** Set the file name filter. Multiple filters separated by ',' */
	void setFilter(const char* filter);
	/** Set the default file extension appended to new files */
	void setDefaultExtension(const char* ext);

	/** Get the full file path */
	const char* getFile() const { return m_filename; } 
	/** Get the current directory */
	const char* getDirectory() const { return m_history[m_historyIndex]; }

	/** Show the dialog */
	void showOpen();
	void showSave();

	public:
	Delegate<void(const char*)> eventConfirm;

	protected:
	void  makeFileName();									// Create filename string
	void  refreshFileList();								// Update file list
	bool  match(const char* string, const char* pattern);	// String pattern matching
	char* m_filename;				// Full file path
	char  m_filter[32];				// File filter
	char  m_extension[16];			// Default file extension
	std::vector<char*> m_history;	// Dialog history
	uint  m_historyIndex;			// Position in history list of current directory (for forward)
	float m_lastClick; // Time of last click to detect double clicking - shoule be in base or gui

	protected:
	void initialise(const gui::Root*, const gui::PropertyMap&);
	gui::Listbox* m_list;
	gui::Textbox* m_dir;
	gui::Textbox* m_file;
	gui::Button*  m_confirm;
	bool m_saveMode;

	void selectFile(gui::Listbox*, gui::ListItem&);
	void clickFile(gui::Widget*, const Point&, int);
	void submitFileName(gui::Textbox*);
	void changedFileName(gui::Textbox*, const char*);
	void changedDirectory(gui::Textbox*);
	void pressUp(gui::Button*);
	void pressBack(gui::Button*);
	void pressForward(gui::Button*);
	void pressConfirm(gui::Button*);
};


