#include "file.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

File::File(): m_file(0), m_size(0), m_buffer(0), m_pos(0) {}
File::File(const char* name): m_size(0), m_buffer(0), m_pos(0) {
	m_file = strdup(name);
}
File::~File() {
	if(m_buffer) delete [] m_buffer;
	if(m_file) free(m_file);
}
unsigned File::read(char* data, unsigned length) {
	size_t max = m_buffer+m_size - m_pos;
	if(length>max) length=max;
	memcpy(data, m_pos, length);
	return length;
}
unsigned File::write(const char* data, unsigned length) {
	// Resize file
	if(m_pos+length >= m_buffer+m_size) {
		size_t newSize = m_pos + length - m_buffer;
		char* tmp = m_buffer;
		m_buffer = new char[newSize];
		memcpy(m_buffer, tmp, m_size);
		m_size = newSize;
		delete [] tmp;
	}
	memcpy(m_pos, data, length);
	return length;
}
void File::seek(int s) { m_pos += s; }
void File::seek(int s, int from) {
	switch(from) {
	case SEEK_CUR: m_pos += s; break;
	case SEEK_END: m_pos = m_buffer+m_size-s; break;
	case SEEK_SET: m_pos = m_buffer+s; break;
	}
}
bool File::load() {
	// ToDo: Allow loading from pack file
	FILE * file = fopen(m_file, "r");
	if(file) {
		fseek(file, 0, SEEK_END);
		m_size = ftell(file);
		rewind(file);
		m_buffer = new char[m_size+1];
		m_size = fread(m_buffer, 1, m_size, file);
		m_buffer[m_size]=0; // terminate string for non-binary files
		fclose(file);
		// return file object
		return true;
	} else return false;
}
bool File::save() {
	FILE* file = fopen(m_file, "w");
	if(file) {
		fwrite(m_buffer, 1, m_size, file);
		fclose(file);
		return true;
	} else return false;
}

File File::load(const char* name) {
	File f(name);
	if(!f.load()) { delete [] f.m_file; f.m_file=0; }
	return f;
}


