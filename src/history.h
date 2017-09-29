#ifndef HISTORY_H
#define HISTORY_H

// XXX Make MAX_PAT_LEN a variable passed in through the constructor

#define NumHistoryItems		(11u)

class History
{
  protected:
    UInt16 len;
    MemHandle history[NumHistoryItems];
    UInt16 pos;
    char *rtn;
  public:
    History(UInt16 len_in);
    void Set(UInt16 pos, const char *pat, UInt16 maxlen = 0);
    void Add(const char *pat);
    UInt16 MaxItems() const
    {
        return NumHistoryItems;
    }
    char *Get(UInt16 idx);
    char *GetItem(Int16 idx)
    {
        if (idx>=0 && idx<(Int16)NumHistoryItems)
        {
            UInt16 p = ((NumHistoryItems-1) + pos-idx)%NumHistoryItems;
	    return Get(p);
	}
	return 0;
    }
    unsigned char *Save(unsigned char *ptr);
    unsigned char *Restore(unsigned char *ptr);
    ~History();
};

#endif
