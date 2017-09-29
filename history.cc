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

History::History()
{}

void History::Init()
{
    memset((char*)history, (char)0, NumHistoryItems*sizeof(history[0]));
    pos = 0;
}

void History::Set(UInt pos, const char *pat, UInt maxlen)
{
    if (history[pos])
	MemHandleFree(history[pos]);
    UInt l = strlen(pat)+1;
    if (l > maxlen) l = maxlen;
    VoidHand h = history[pos] = MemHandleNew(l);
    if (h)
    {
        char *s = (char *)MemHandleLock(h);
	if  (s) 
	{
	    strncpy(s, pat, (UInt)(l-1));
	    s[l-1] = 0;
	}
	MemHandleUnlock(h);
    }
}

void History::Add(const char *pat)
{
    UInt idx = (pos+NumHistoryItems-1)%NumHistoryItems;
    if (history[idx])
    {
	char *s = (char *)MemHandleLock(history[idx]);
	int isdup = (s && strncmp(s, pat, MAX_PAT_LEN) == 0);
	MemHandleUnlock(history[idx]);
	if (!isdup)
	{
	    (void)Set(pos, pat, MAX_PAT_LEN);
	    pos = (pos+1)%NumHistoryItems;
	}
    }
}

unsigned char *History::Save(unsigned char *ptr) const
{
    for (UInt i = 0; i < NumHistoryItems; i++, ptr += MAX_PAT_LEN)
    {
	strncpy((char*)ptr, (const char *)Get(i), MAX_PAT_LEN-1);
	ptr[MAX_PAT_LEN-1] = 0;
    }
    *((UInt*)ptr)++ = pos;
    return ptr;
}

unsigned char *History::Restore(unsigned char *ptr)
{
    for (UInt i = 0; i < NumHistoryItems; i++, ptr += MAX_PAT_LEN)
    {
        Set(i, (const char *)ptr, MAX_PAT_LEN);
    }
    pos = *((UInt*)ptr)++;
    return ptr;
}

History::~History()
{
    for (int i = 0; i < NumHistoryItems; i++)
	if (history[i])
	    MemHandleFree(history[i]);
}

#endif // IS_FOR_PALM

