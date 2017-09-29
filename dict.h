#ifndef DICT_H
#define DICT_H

#define USEALL	0
#define PREFIX	1
#define SUFFIX	2
#define MULTI	3

#define ALL	    (0x3FFFFFFl)
#define VOWELS      ((1l<<0) | (1l<<4) | (1l<<8) | (1l<<14) | (1l<<20))
#define CONSONANTS  (ALL - VOWELS)
#define MAXPATLEN   (30)

#define VARNUM		(0x80000000ul)
#define WORDEND		(0x40000000ul)

const char *ltoa(unsigned long l);

//#define NodesPerRecord		1000 // version 1.8 and befor
#define NodesPerRecord		4096
#define DictDBType		'dict'

class VariableSet
{
    unsigned long defmask;
    char vars[26];
    unsigned long used[26]; // current set of matched letters
    unsigned long constraints[26]; // cumulative intersection of used
  public:
    void ClearAssignments()
    {
        memset(vars, (char)0, 26);
	defmask = 0x3fffffful;
    }
    char Assignment(int v) const
    {
        return vars[v];
    }
    void Assign(int v, char c)
    {
        if (vars[v])
	    defmask |= 1ul << (vars[v]-'A');
	defmask &= ~(1ul << (c-'A'));
        vars[v] = c;
    }
    unsigned long Constraint(int v) const
    {
	return (defmask & constraints[v]);
    }
    unsigned long AvailableChoicesMask(int v) const
    {
        return (vars[v] ? (1ul << (vars[v]-'A')) : Constraint(v));
    }
    int CanUseIdx(int v, int idx) const
    {
        return (AvailableChoicesMask(v) & (1ul<<idx))!=0;
    }
    inline int CanUseChar(int v, char c) const
    {
        return CanUseIdx(v, (int)(c-'A'));
    }
    int IsIdxInConstraint(int v, int idx) const
    {
        return (Constraint(v) & (1ul<<idx))!=0;
    }
    inline int IsChInConstraint(int v, char c) const
    {
        return IsIdxInConstraint(v, c-'A');
    }
    void ClearUsed()
    {
        memset((char*)used, (char)0, 26*sizeof(long));
    }
    void ResetConstraints()
    {
        memset((char*)constraints, (char)0xff, 26*sizeof(long));
    }
    void SetConstraint(int v, unsigned long m)
    {
        constraints[v] = m;
    }
    void UseIdx(int v, int idx)
    {
        used[v] |= 1ul << idx;
    }
    void UseCh(int v, char c)
    {
        UseIdx(v, (int)(c-'A'));
    }
    void ComputeConstraints();
    VariableSet()
    { }
    void Init()
    {
        ClearAssignments();
	ResetConstraints();
    }
    void Print(char *buf, int v) const;
    void Show() const;
    ~VariableSet()
    {}
};

class DictionaryDatabase
#ifdef IS_FOR_PALM
    : public Database
#endif
{
#ifdef IS_FOR_PALM
    class Form *owner;
#else
    char dbname[32];
#endif
    int nodesize;
    int allocpool[26], fixedpool[26], maxpool[26], need[26];
    int varset[26], refcount[27];

    VariableSet *vars;

    // node stack for non-recursive matching
    unsigned long nstk[30];
    int sp, start, laststart, stop, vecoff, swcnt;
    int stepnum, matches, progress;
    int starts[32];
    unsigned long anchormasks[MAXPATLEN],
    		  varpool[MAXPATLEN], 
		  pmask;
    int vlen, plen, wildfloats, vtot;
    int matchtype, mincnt, maxcnt, minlen, maxlen; //, idx;
    int numlens, lennow, wlen[32], vpos[32];
    char lastmulti[80];
    int progpoint;
    int has_float_ranges;
    int simple_search;
    int has_place_constraints;
    int has_wordend_constraints;

#ifdef UNIX // UNIX dawg is differnt for i386
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

    virtual Boolean OnOpen();
  public:
    static const char *MatchTypeName(UInt t);
    static const char *LimitName(UInt n);
#ifdef IS_FOR_PALM
    void SetProgressOwner(Form *o)
    {
        owner = o;
    }
#endif
    VariableSet *GetVariables() const
    {
        return vars;
    }
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
            return (v&0xFFFFFFul);
        }
#ifndef IS_FOR_PALM
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

#ifndef OLD_GETNODE
    VoidHand *handles;
    unsigned char **records;
    void InitRecords();
  public:
    void FreeRecords();
  protected:
#endif
    
    unsigned char *current_record;
#ifdef IS_FOR_PALM
    VoidHand current_handle;
#else
    FILE *fp;
    int numrecords;
    unsigned long nodeStart;
#endif
    int current_recnum;

    void ShowProgress(DictionaryNode &N);
    int FindPoolAllocation(unsigned pos, int tot, int wilds,
			   unsigned long pmask = 0xfffffffful);
    int RemoveFromPool(int c);
    void ReplaceInPool(int c);
    int GrabLetter(DictionaryNode &n);
    void ReplaceLetter(DictionaryNode &n);

    unsigned long NextPatternElt(char *&pat, int &is_float, int &is_single);
    char *NextPat(char *pat, int pos);
    void InitStack(int l, int first_vec);
    void GetWord(char *word, int stacklen);
    int NextSingleWordAllLettersStep(char *line);
    int NextStep(char *line);
    DictionaryNode GetNode(unsigned long n);
    void Reinitialise();
    virtual int MustStop();
#ifdef DUMP
    void RecursiveDump(int pos, long n, FILE *ofp);
#endif
  public:
    void InitDB();
    int StartConsult(char *pat, int type_in = USEALL,
    			int multi_in = 0,
    			int minlen_in=0, int maxlen_in=0, 
			int mincnt_in=0, int maxcnt_in=0,
			VariableSet *vars_in=0);
    int NextMatch(char *line, int len);
    void Reset();
    inline int Matches() const
    {
        return matches;
    }
    void ClearProgress()
    {
        progress = 0;
    }
#ifdef IS_FOR_PALM
    Boolean Open()
#else
    Boolean Open(const char *fname = 0)
#endif
    {
#ifdef IS_FOR_PALM
        return Database::Open(DictDBType, dmModeReadOnly);
#else
#ifdef UNIX
	fp = fopen((fname ? fname : "dawg"), "rb");
#else
	fp = fopen((fname ? fname : "dawg.pdb"), "rb");
#endif
	if (fp == 0) perror("fopen");
	return fp ? 1 : 0;
#endif
    }
#ifdef DUMP
    void RawDump(FILE *fp);
    void Dump(FILE *fp);
#endif
#ifdef IS_FOR_PALM
    DictionaryDatabase() : Database() {}
    void Init();
#else
    DictionaryDatabase() {}
    void Init(const char *fname = 0);
#endif
    virtual ~DictionaryDatabase();
};

#ifndef IS_FOR_PALM
void ShowVector(unsigned long *vec);
#endif

#endif

