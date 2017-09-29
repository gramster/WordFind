#include "mypilot.h"
#ifdef IS_FOR_PALM
//#include "ptrsc.h"
#include "fw.h"
#include "history.h"
#endif

//#include "dict.h"

// derive the application

//---------------------------------------------------------
// Entry history

#ifdef IS_FOR_PALM

void HMemCopy(char *buf, MemHandle h, UInt16 maxlen)
{
    buf[0] = 0;
    if (h)
    {
	char *s = (char *)MemHandleLock(h);
	if (s)
	{
	    strncpy(buf, s, (Int16)maxlen-1);
	    buf[maxlen-1] = 0;
	}
	MemHandleUnlock(h);
    }
}

History::History(UInt16 len_in)
    : len(len_in), pos(0)
{
    memset((char*)history, (char)0, NumHistoryItems*sizeof(history[0]));
    rtn = (char*)MemPtrNew(len);
    rtn[0] = 0;
}

char *History::Get(UInt16 idx)
{
    if (history[idx]) HMemCopy(rtn, history[idx], len);
    else rtn[0] = 0;
    return rtn;
}

void History::Set(UInt16 pos, const char *pat, UInt16 maxlen)
{
    if (history[pos])
	MemHandleFree(history[pos]);
    UInt16 l = (UInt16)strlen(pat)+1;
    if (l > maxlen) l = maxlen;
    MemHandle h = history[pos] = MemHandleNew(l);
    if (h)
    {
        char *s = (char *)MemHandleLock(h);
	if  (s) 
	{
	    strncpy(s, pat, (Int16)(l-1));
	    s[l-1] = 0;
	}
	MemHandleUnlock(h);
    }
}

void History::Add(const char *pat)
{
    UInt16 idx = (pos+NumHistoryItems-1)%NumHistoryItems;
    int isdup = 0;
    if (history[idx])
    {
	char *s = (char *)MemHandleLock(history[idx]);
	isdup = (s && strncmp(s, pat, len) == 0);
	MemHandleUnlock(history[idx]);
    }
    if (!isdup)
    {
	(void)Set(pos, pat, len);
	pos = (pos+1)%NumHistoryItems;
    }
}

unsigned char *History::Save(unsigned char *ptr)
{
    for (UInt16 i = 0; i < NumHistoryItems; i++, ptr += len)
    {
	strncpy((char*)ptr, (const char *)Get(i), (Int16)len-1);
	ptr[len-1] = 0;
    }
    *((UInt16*)ptr)++ = pos;
    return ptr;
}

unsigned char *History::Restore(unsigned char *ptr)
{
    for (UInt16 i = 0; i < NumHistoryItems; i++, ptr += len)
    {
        Set(i, (const char *)ptr, len);
    }
    pos = *((UInt16*)ptr)++;
    // sanity
    if (pos < 0 || pos >= NumHistoryItems) pos = 0;
    return ptr;
}

History::~History()
{
    for (UInt16 i = 0; i < NumHistoryItems; i++)
	if (history[i])
	    MemHandleFree(history[i]);
    MemPtrFree(rtn);
}

#endif // IS_FOR_PALM

