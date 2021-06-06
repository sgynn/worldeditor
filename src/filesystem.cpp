#include "filesystem.h"
#include <base/directory.h>
#include <cstdio>

using base::Directory;

FileSystem::FileSystem() {
	setRootPath(".");
}

void FileSystem::setRootPath(const char* path, bool isAFile) {
	char buffer[2048];
	Directory::getFullPath(path, buffer, 2048);
	if(isAFile) {
		char* c = strrchr(buffer, '/');
		char* cw = strrchr(buffer, '\\');
		if(cw > c) c = cw;
		if(c) *c = 0;
	}
	// Ensure it ends with '/'
	int len = strlen(buffer);
	if(len>0 && buffer[len-1]!='/' && buffer[len-1]!='\\') sprintf(buffer+len, "/");
	m_rootPath = buffer;
}

String FileSystem::getFile(const char* file) {
	if(!file || !file[0]) return String();
	if(Directory::isRelative(file)) {
		char buffer[2048];
		if(strncmp(file, "./", 2)==0) file += 2;
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
String FileSystem::getUniqueFile(const char* name) {
	String file = getFile(name);
	if(!exists(file)) return name;
	
	// Try to make it unique
	char tmp[512];
	strcpy(tmp, name);
	const char* ext = strrchr(name, '.');
	char* end = tmp + (ext-name);
	// try some new numbers
	int index = 0;
	while(true) {
		sprintf(end, "_%d%s", ++index, ext);
		if(!exists( getFile(tmp) )) return tmp;
	}
	return 0; // can't get here
}



