#ifndef HISTORY_H
#define HISTORY_H

#define MAX_PAT_LEN		(64)
#define NumHistoryItems		(10)

class History
{
  protected:
    VoidHand history[NumHistoryItems];
    UInt pos;
    char rtn[MAX_PAT_LEN];
  public:
    History();
    void Init();
    void Set(UInt pos, const char *pat, UInt maxlen);
    void Add(const char *pat);
    int NumItems() const
    {
        return NumHistoryItems;
    }
    char *Get(UInt idx)
    {
	HMemCopy(rtn, history[idx], MAX_PAT_LEN);
	return rtn;
    }
    char *GetItem(UInt idx)
    {
        UInt p = ((NumHistoryItems-1) + pos-idx)%NumHistoryItems;
	return Get(p);
    }
    unsigned char *Save(unsigned char *ptr) const;
    unsigned char *Restore(unsigned char *ptr);
    ~History();
};

#endif

