#include <syslog.h>
#include "strutil.h"
#include "charbuff.h"

CharBuff::CharBuff(unsigned size_in, const char *file_in, int line_in)
    : buff(new char[size_in+4]),
      size(size_in),
      file(file_in),
      line(line_in)
{
    if (buff == 0)
	syslog(LOG_INFO, "Memory allocation failure in file %s:%d",
			file, line);
    else
    {
        buff += 2;
        buff[-2] = 0xde;
        buff[-1] = 0xad;
        buff[size] = 0xbe;
        buff[size+1] = 0xef;
    }
}

CharBuff::~CharBuff()
{
    buff-=2;
    unsigned char *b = (unsigned char*)buff;
    if (b[0]!=0xde || b[1]!= 0xad)
	syslog(LOG_INFO, "Buffer underrun (size %d, file %s:%d)",
		size, file, line);
    if (b[size+2]!=0xbe || b[size+3] != 0xef)
	syslog(LOG_INFO, "Buffer overrun (size %d, file %s:%d)",
		size, file, line);
    delete [] buff;
}

void CharBuff::Set(const char *v, const char *file_in, int line_in)
{
    CEQURUX_strlcpy(buff, v, size, size, file_in, line_in);
}

#ifdef TEST

main()
{
    CHARBUFF(x, 10);
    x.Set("0123456789");
}

#endif



