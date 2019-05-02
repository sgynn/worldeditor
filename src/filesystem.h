#ifndef _FILESYSTEM_
#define _FILESYSTEM_

#include "gui/gui.h"
using gui::String;


class FileSystem {
	public:
	void setRootPath(const char* path, bool isAFile=false);
	String getFile(const char*);		/// get file from relative path
	String getRelative(const char*);	/// create a relative path
	bool exists(const char*) const;
	void copyFile(const char* dest, const char* source);
	String addTemporaryFile(const char* name);
	void copyTemporaryFiles();

	private:
	String m_rootPath;
};

#endif

