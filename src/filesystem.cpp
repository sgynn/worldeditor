#include "filesystem.h"
#include <base/directory.h>
#include <cstdio>

using base::Directory;

FileSystem::FileSystem() : m_rootPath(".") {
}

void FileSystem::setRootPath(const char* path, bool isAFile) {
	if(isAFile) {
		char buffer[2048];
		strcpy(buffer, path);
		char* c = strrchr(buffer, '/');
		char* cw = strrchr(buffer, '\\');
		if(cw > c) c = cw;
		if(c) *c = 0;
		m_rootPath = buffer;
	}
	else m_rootPath = path;
}
String FileSystem::getFile(const char* file) {
	if(!file || !file[0]) return String();
	if(Directory::isRelative(file)) {
		char buffer[2048];
		snprintf(buffer, 2048, "%s/%s", m_rootPath.str(), file);
		return String(buffer);
	}
	else return file;
}
String FileSystem::getRelative(const char* in) {
	if(Directory::isRelative(in)) return in;

	const char* base = m_rootPath;
	// Match part
	int m=0, up=0;
	while(base[m] == in[m]) ++m;
	while(m>0 && (in[m]=='/' || in[m]=='\\')) ++m;
	for(const char* c = base + m; *c; ++c) if(*c=='/' || *c=='\\') ++up;

	// Build
	char buffer[2048];
	int k = 2;
	if(up==0) strcpy(buffer, "./");
	else {
		for(int i=0; i<up; ++i) strcpy(buffer+i*3, "../");
		k = up*3;
	}
	strncpy(buffer+k, in+m, 2048 - k);
	return buffer;
}
bool FileSystem::exists(const char* file) const {
	// May be a better way
	FILE* fp = fopen(file, "rb");
	if(!fp) return false;
	fclose(fp);
	return true;
}
bool FileSystem::copyFile(const char* source, const char* dest) {
	// copy temp to dest
	char buffer[BUFSIZ];
	size_t size;
	FILE* src = fopen(source, "rb");
	if(!src) return false;
	FILE* dst = fopen(dest, "wb");
	while((size = fread(buffer, 1, BUFSIZ, src))) {
		fwrite(buffer, 1, size, dst);
	}
	fclose(src);
	fclose(dst);
	return true;
}

