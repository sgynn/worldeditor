#include "filedialog.h"
#include "gui/lists.h"
#include "gui/skin.h"

#include <base/game.h>
#include <base/input.h>
#include <base/directory.h>
#include <cstring>
#include <cstdio>

using namespace base;
using namespace gui;


FileDialog::FileDialog(const Rect& r, Skin* s) : Window(r, s), m_filename(0), m_historyIndex(0), m_lastClick(0), m_folderIcon(0), m_fileIcon(0), m_saveMode(false) {
	m_extension[0]=0;
	m_filter[0]=0;
}
FileDialog::~FileDialog() {
	for(uint i=0; i<m_history.size(); ++i) free(m_history[i]);
}

void FileDialog::initialise(const Root* r, const PropertyMap& p) {
	Window::initialise(r, p);
	if(m_client->getWidgetCount()==0) return; // Nothing

	// Cache sub widgets
	m_list = getWidget<Listbox>("filelist");
	m_file = getWidget<Textbox>("filename");
	m_dir  = getWidget<Textbox>("path");
	m_confirm = getWidget<Button>("confirm");
	
	// Set up callbacks
	m_list->eventSelected.bind(this, &FileDialog::selectFile);
	m_list->eventMouseDown.bind(this, &FileDialog::clickFile);
	m_confirm->eventPressed.bind(this, &FileDialog::pressConfirm);
	m_file->eventChanged.bind(this, &FileDialog::changedFileName);
	m_file->eventSubmit.bind(this, &FileDialog::submitFileName);
	m_dir->eventSubmit.bind(this, &FileDialog::changedDirectory);
	
	
	Button* btn = getWidget<Button>("up");
	if(btn) btn->eventPressed.bind(this, &FileDialog::pressUp);
	btn = getWidget<Button>("back");
	if(btn) btn->eventPressed.bind(this, &FileDialog::pressBack);
	btn = getWidget<Button>("fwd");
	if(btn) btn->eventPressed.bind(this, &FileDialog::pressForward);

	// Cache icons
	m_folderIcon = m_list->getIconList()->getIconIndex("folder");
	m_fileIcon = m_list->getIconList()->getIconIndex("file");
	

	// Load properties
	const char* initialPath = ".";
	if(p.contains("filter")) setFilter( p["filter"] );
	if(p.contains("dir")) initialPath = p["dir"];

	// Initial directory
	char buffer[FILENAME_MAX];
	Directory::getFullPath(initialPath, buffer);
	setDirectory(buffer);
}

void FileDialog::setFilter(const char* f) {
	if(f==0 || (f[0]=='*' && (f[1]==0 || f[1]==','))) m_filter[0] = 0; // No filter
	else strcpy(m_filter, f);
	refreshFileList();
}
void FileDialog::setDirectory(const char* dir) {
	if(!m_history.empty() && strcmp(dir, getDirectory())==0) return; // no change
	printf("cd %s\n", dir);

	// Clear forward history
	while(m_history.size()>m_historyIndex+1) {
		free(m_history.back());
		m_history.pop_back();
	}

	// Button states
	getWidget("back")->setEnabled(true);
	getWidget("fwd")->setEnabled(false);

	// Add directory to history
	char buffer[1024];
	Directory::clean(dir, buffer);
	char* d = strdup(buffer);
	m_history.push_back(d);
	m_historyIndex = m_history.size()-1;
	
	// Update directory textbox
	if(m_dir) m_dir->setText(d);
	if(m_file) m_file->setText("");
	changedFileName(0,"");

	// Refresh
	refreshFileList();
}
void FileDialog::refreshFileList() {
	if(!isVisible()) return;
	m_list->clearItems();
	Directory d( getDirectory() );
	for(Directory::iterator i = d.begin(); i!=d.end(); ++i) {
		int icon = i->type==Directory::DIRECTORY? m_folderIcon: m_fileIcon;
		if(i->name[0]=='.') continue; // Ignore hidden files
		// Match filters
		if(i->type!=Directory::DIRECTORY && m_filter[0]) {
			bool matches = false;
			char* p0;
			for(char* p = m_filter; *p&&!matches; p=p0) {
				if(*p==',') ++p;
				for(p0=p; *p0&&*p0!=','; ++p0);
				if(*p0) {
					*p0=0; matches = match(i->name, p); *p0=',';
				} else matches = match(i->name, p);
			}
			if(!matches) continue;
		}
		// Add item to list
		m_list->addItem( i->name, Any(), icon );
	}
}

void FileDialog::setFileName(const char* name) {
	m_file->setText(name);
}

void FileDialog::setDefaultExtension(const char* ext) {
	memcpy(m_extension, ext, 16);
	if(m_file) makeFileName();
}

void FileDialog::makeFileName() {
	if(m_filename) free(m_filename);
	const char* name = m_file->getText();
	const char* dir = m_dir->getText();
	// Check extension
	if(m_extension[0]) {
		int sl = strlen(name);
		int el = strlen(m_extension);
		const char* e = name + sl - el - 1;
		// Add extension
		if(*e!='.' || strcmp(e+1, m_extension)!=0) {
			m_filename = (char*) malloc(strlen(dir) + sl + el + 3);
			sprintf(m_filename, "%s/%s.%s", dir, name, m_extension);
			return;
		}
	}
	// Extension already there
	m_filename = (char*) malloc(strlen(dir) + strlen(name) + 2);
	sprintf(m_filename, "%s/%s", dir, name);
}


bool FileDialog::match(const char* s, const char* p) {
	while(*s) {
		if(*p=='*') {	// Match n characters
			do{++p;} while(*p=='*'); // Ignore successive *s
			while(*s) if(match(s++, p)) return true; // Match substrings
			return false;
		} else if(*s == *p || *p=='?') {	// Mach single character
			++p; ++s;
		} else return false;
	}
	// If pattern is longer, check if it end in *
	while(*p=='*') ++p;
	return *p==0;
}

void FileDialog::showOpen() {
	setCaption("Open File");
	m_confirm->setCaption("Open");
	setVisible(true);
	refreshFileList();
	m_list->setFocus();
	m_confirm->setEnabled(false);
	m_saveMode = false;
	raise();
}
void FileDialog::showSave() {
	setCaption("Save File");
	m_confirm->setCaption("Save");
	setVisible(true);
	refreshFileList();
	m_file->setFocus();
	m_confirm->setEnabled(false);
	m_saveMode = true;
	raise();
}


void FileDialog::selectFile(Listbox* list, int index) {
	m_file->setText( list->getItem(index) );
	m_confirm->setEnabled( true );
	if(m_saveMode) {
		m_confirm->setCaption( list->getItemIcon(index) == m_folderIcon? "Open": "Save" );
	}
}

void FileDialog::clickFile(Widget*, const Point& p, int b) {
	// Detect double click - root shoud have this
	static Point lp;
	if(lp==p && b==1) {
		pressConfirm(0);
		lp.x = -100;
	} else lp = p;
}

void FileDialog::changedDirectory(Textbox* t) {
	printf("change dir\n");
	setDirectory(t->getText());
}
void FileDialog::changedFileName(Textbox*, const char* t) {
	m_confirm->setEnabled(t[0]);
	if(m_saveMode) m_confirm->setCaption("Save");
}
void FileDialog::submitFileName(Textbox*) {
	pressConfirm(0);
}

void FileDialog::pressUp(Button*) {
	if(strlen(getDirectory()) < 3) return;
	char buffer[1024];
	sprintf(buffer, "%s/..", getDirectory());
	setDirectory(buffer);
}

void FileDialog::pressBack(Button* b) {
	if(m_historyIndex) {
		--m_historyIndex;
		refreshFileList();
		m_dir->setText( getDirectory() );
		b->setEnabled( m_historyIndex>0 );
		getWidget("fwd")->setEnabled(true);
	}
}
void FileDialog::pressForward(Button* b) {
	if(m_historyIndex < m_history.size()-1) {
		++m_historyIndex;
		refreshFileList();
		m_dir->setText( getDirectory() );
		b->setEnabled( m_historyIndex < m_history.size()-1 );
		getWidget("back")->setEnabled(true);
	}
}
void FileDialog::pressConfirm(Button*) {
	// Directory selected
	if(m_list->getSelectionSize() && m_list->getItemIcon( m_list->getSelectedIndex() )==m_folderIcon) {
		char buffer[1024];
		sprintf(buffer, "%s/%s", getDirectory(), m_list->getSelectedItem());
		setDirectory(buffer);
	}
	// Confirm file operation
	else {
		makeFileName();
		setVisible(false);
		if(eventConfirm) eventConfirm(m_filename);
	}
}


