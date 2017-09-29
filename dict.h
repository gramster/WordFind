#ifndef DICT_H
#define DICT_H

#define USEALL	0
#define PREFIX	1
#define SUFFIX	2
#define MULTI	3

#define ALL	    (0x3FFFFFFl)
#define VOWELS      ((1l<<0) | (1l<<4) | (1l<<8) | (1l<<14) | (1l<<20))
#define CONSONANTS  (ALL - VOWELS)
#define MAXPATLEN	(30)

char *MatchTypeName(UInt t);
char *LimitName(UInt n);

#define NodesPerRecord		1000
#define DictDBType		'dict'

class DictionaryDatabase
#ifndef TEST
    : public Database
#endif
{
#ifndef TEST
    class Form *owner;
#endif
    int nodesize;
    int allocpool[26], fixedpool[26], maxpool[26], need[26];
    // node stack for non-recursive matching
    unsigned long nstk[30];
    int sp, start, laststart, stop, vecoff, swcnt;
    int stepnum, matches, progress;
    int starts[32];
    unsigned long anchormasks[MAXPATLEN],
    				varpool[MAXPATLEN], pmask;
    int vlen, plen, wildfloats, vtot;
    int matchtype, mincnt, maxcnt, minlen, maxlen, idx;
    int numlens, lennow, wlen[32], vpos[32];
    char lastmulti[80];
    int progpoint;
    int has_float_ranges;
    int has_place_constraints;

#ifdef TEST // UNIX dawg is differnt for i386
#define TVSH	29
#define PVSH	30
#define IVSH	24
#define IDLT	1
#define FIRSTNODE	(1l)

#else
#define TVSH	30
#define PVSH	31
#define IVSH	25
#define IDLT	0
#define FIRSTNODE	(1l)
#endif

  public:
    unsigned long LetterMask(char c)
    {
        return (1ul << (c-'A'));
    }
    struct DictionaryNode
    {
        unsigned long v;

	    DictionaryNode(unsigned long v_in = 0)
	    	: v(v_in)
	    { }
        inline int IsTerminal() const
        {
            return (int)((v>>TVSH)&1);
        }
        inline int IsLastPeer() const
        {
            return (int)((v>>PVSH)&1);
        }
        inline int Index() const
        {
            return (int)((v>>IVSH)&0x1F)-IDLT;
        }
        inline char Char() const
        {
            return (char)(Index()+'A');
        }
        inline unsigned long Mask() const
        {
            return (1ul << Index());
        }
        inline unsigned long FirstChild() const
        {
#ifdef TEST
            return (v&0xFFFFFFul);
#else
            //return (v&0x1FFFFFFul);
            return (v&0xFFFFFFul);
#endif
        }
#ifdef TEST
	void Print()
	{
	    printf(" %c  %c%c\n", Char(),
			(IsTerminal()?'T':' '),
			(IsLastPeer()?'L':' '));
	}
#endif
    };
  protected:
    DictionaryNode Nstk[30];
    int Node2Record(unsigned long n)
    {
        return (int)(n/NodesPerRecord);
    }
    
    unsigned char *current_record;
#ifdef TEST
    FILE *fp;
#else
    VoidHand current_handle;
#endif
    int current_recnum;

    int FindPoolAllocation(int pos, int tot, int wilds,
						unsigned long pmask = 0xfffffffful);
    int RemoveFromPool(int c);
    void ReplaceInPool(int c);
    int GrabLetter(DictionaryNode &n);
    void ReplaceLetter(DictionaryNode &n);
    char *NextPat(char *pat, int pos);
    void InitStack(int l, int first_vec);
    void GetWord(char *word, int stacklen);
    int NextSingleWordAllLettersStep(char *line);
    int NextStep(char *line);
    DictionaryNode GetNode(unsigned long n);
    int MustStop();
  public:
    int StartConsult(char *pat, int type_in = USEALL,
    			int multi_in = 0,
    			int minlen_in=0, int maxlen_in=0, 
			int mincnt_in=0, int maxcnt_in=0);
    int NextMatch(char *line);
    void Reset();
    inline int Matches() const
    {
        return matches;
    }
    void ClearProgress()
    {
        progress = 0;
    }
    Boolean Open()
    {
#ifdef TEST
	fp = fopen("dawg", "r");
	return fp ? 1 : 0;
#else
        return Database::Open(DictDBType);
#endif
    }
#ifdef TEST
    DictionaryDatabase();
#else
    DictionaryDatabase(class Form *owner_in);
#endif
    ~DictionaryDatabase();
};

#endif


