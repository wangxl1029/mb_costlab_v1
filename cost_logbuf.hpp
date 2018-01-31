#pragma once
#include <stdlib.h>
#include <stdio.h>
class CCostLogBuffer
{ 
	public:	// life cycle
		CCostLogBuffer();
		virtual ~CCostLogBuffer();
		// method
		size_t write(const char*, size_t);	
		virtual void flush();
		virtual void close();
		const char* makePathWithExt(const char*);
	private:
		void open_once();
		char m_buffer[4096];
		size_t m_bufpos;
		FILE* m_file;
		char m_basname[BUFSIZ];
		char m_dir[BUFSIZ];
};
