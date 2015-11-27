#ifndef _FILE_
#define _FILE_

/** File class - reads entire file as string*/
class File {
	public:
	File();
	File(const char* name);
	~File();
	unsigned size() const { return m_size; }
	const char* contents() const { return m_buffer; }
	operator const char*() { return m_buffer; }
	unsigned read(char* data, unsigned length);
	unsigned write(const char* data, unsigned length);
	void seek(int s);
	void seek(int s, int from);
	bool load();
	bool save();
	static File load(const char* f);
	protected:
	char* m_file;
	unsigned m_size;
	char* m_buffer;
	char* m_pos;
};

#endif

