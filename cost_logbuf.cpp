#include "stdafx.h"
#include "cost_logbuf.hpp"

CCostLogBuffer::CCostLogBuffer()
	: m_bufpos(0)
	, m_file(NULL)
{
	strncpy(m_dir, "log/", BUFSIZ);
}

CCostLogBuffer::~CCostLogBuffer()
{
	close();
}

void CCostLogBuffer::open_once()
{
	if(!m_file)
	{
		char buf[BUFSIZ];
		time_t tval = time(NULL);
		struct tm* tm = localtime(&tval); 
		snprintf(m_basname, BUFSIZ, "wxl%02d_%02d%02d%02d", tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
		snprintf(buf,BUFSIZ, "%s%s.log", m_dir, m_basname);
		m_file = fopen(buf, "wt");
	}
}

const char* CCostLogBuffer::makePathWithExt(const char* ext)
{
	static char buf[BUFSIZ];
	if(ext && m_file){
		snprintf(buf, BUFSIZ, "%s%s.%s", m_dir, m_basname, ext);
		return buf;
	}

	return buf;
}

size_t CCostLogBuffer::write(const char* buf, size_t n)
{
	open_once();
	if(NULL != m_file && NULL != buf && n > 0 )
	{
		if(n > sizeof(m_buffer))
		{
			n = sizeof(m_buffer);
		}

		if(n > sizeof(m_buffer) - m_bufpos)
		{
			flush();
		}

		if(n <= sizeof(m_buffer) - m_bufpos)
		{
			memcpy(m_buffer + m_bufpos, buf, sizeof(m_buffer) - m_bufpos);
			m_bufpos += n;
		}
	}

	return n;
}

void CCostLogBuffer::flush()
{
	if(m_file)
	{
		(void)fwrite(m_buffer, 1, m_bufpos, m_file);
		m_bufpos = 0;
	}
}

void CCostLogBuffer::close()
{
	if(m_file)
	{
		(void)fwrite(m_buffer, 1, m_bufpos, m_file);
		fclose(m_file);
		m_file = NULL;
		m_bufpos = 0;
	}

}
