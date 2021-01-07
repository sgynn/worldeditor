#ifndef _FILESYSTEM_
#define _FILESYSTEM_

#include "gui/gui.h"
using gui::String;


class FileSystem {
	public:
	FileSystem();
	void setRootPath(const char* path, bool isAFile=false);
	String getFile(const char*);		/// get file from relative path
	String getRelative(const char*);	/// create a relative path
	bool exists(const char*) const;		/// Does a file exist
	String createTemporaryFile();		/// Create a temporary file for streaming
	void copyTemporaryFiles();
	String getUniqueFile(const char*);	/// Get a filename that does not exist yet
	const String& getRootPath() const { return m_rootPath; }

	private:
	static bool copyFile(const char* from, const char* to);
	static bool moveFile(const char* from, const char* to);
	String m_rootPath;
};

#endif

