#ifndef _FILE_
#define _FILE_

#include <cstddef>

/** File class - reads entire file as string*/
class File {
	public:
	File();
	File(const char* name);
	File(const File&);
	File(File&&);
	File& operator=(const File&);
	~File();
	size_t size() const { return m_size; }
	const char* contents() const { return m_buffer; }
	operator const char*() { return m_buffer; }
	unsigned read(char* data, size_t length);
	unsigned write(const void* data, size_t length);
	void seek(int s);
	void seek(int s, int from);
	bool load();
	bool save();
	static File load(const char* f);
	protected:
	char*  m_file;		// filename
	size_t m_size;		// file size
	char*  m_buffer;	// file data
	char*  m_pos;		// read/write pointer
};

#endif

