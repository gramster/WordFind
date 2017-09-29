// TODO: help
// Robustness with bad patterns, overruns
// Add a dict info dialog showing word length distributions 
//	and total number of words.
// Get progress bar to always compute to the same length at end

#include <stdio.h>
typedef short Int;
typedef unsigned short UInt;
typedef unsigned long ULong;

// derive the application

#define CLIENT_DB_TYPE		(0)

#define isspace(c)	((c)==' ' || (c)=='\t' || (c)=='\n')
#define isupper(c)	((c)>='A' && (c)<='Z')
#define islower(c)	((c)>='a' && (c)<='z')
#define toupper(c)	((char)(islower(c) ? ((c)-'a'+'A') : (c)))
#define isalpha(c)	(isupper(c) || islower(c))
#define InRange(v, mn, mx)	((v) >= (mn) && (v) <= (mx))

UInt strlen(char *s)
{
    char *b = s;
    while (*s) ++s;
    return (unsigned)(s-b);
}

void strcpy(char *dst, const char *src)
{
    while ((*dst++ = *src++) != 0)
        (void)0;
}

void strcat(char *dst, const char *src)
{
    strcpy(dst + strlen(dst), src);
}

Int strcmp(char *s1, char *s2)
{
    while (*s1 && *s1 == *s2)
    {
        ++s1;
	++s2;
    }
    return (*s1-*s2);
}

const char *ltoa(ULong l)
{
    if (l <= 0)
    	return "0";
    else
    {
        static char x[10];
    	Int p = 10;
    	while (l)
    	{
        	x[--p] = (char)((l%10)+'0');
        	l = l / 10;
    	}
    	return x+p;
    }
}

//---------------------------------------------------------

#define USEALL	0
#define PREFIX	1
#define SUFFIX	2
#define MULTI	3

#define ALL	    (0x3FFFFFFl)
#define VOWELS      ((1l<<0) | (1l<<4) | (1l<<8) | (1l<<14) | (1l<<20))
#define CONSONANTS  (ALL - VOWELS)
#define MAXPATLEN	(30)

char *MatchTypeName(UInt t)
{
    switch(t)
    {
    case PREFIX: return "Prefixes";
    case SUFFIX: return "Suffixes";
    default:	return "All";
    }
}

char *LimitName(UInt n)
{
    static char b[3];
    if (n > 0)
    {
        if (n<9)
        {
            b[0] = (char)(n+'0');
            b[1] = 0;
            return b;
        }
        else if (n < 100)
        {
            b[0] = (char)((n/10)+'0');
            b[1] = (char)((n%10)+'0');
            b[2] = 0;
            return b;
        }
    }
    return "Any";
}

#define NodesPerRecord		1000
#define DictDBType		'dict'

class DictionaryDatabase
{
    //class WordForm *owner;
    Int nodesize;
    Int allocpool[26], fixedpool[26], maxpool[26], need[26];
    // node stack for non-recursive matching
    ULong nstk[30];
    Int sp, start, laststart, stop, vecoff, swcnt;
    ULong anchormasks[MAXPATLEN],
    				varpool[MAXPATLEN], pmask;
    Int vlen, plen, wildfloats, vtot;
    Int matchtype, mincnt, maxcnt, minlen, maxlen, idx;
    Int numlens, lennow, wlen[32], vpos[32];
    Int starts[32];
    char lastmulti[80];
    Int progpoint;

  public:
    ULong LetterMask(char c)
    {
        return (1ul << (c-'A'));
    }
    struct DictionaryNode
    {
        ULong v;

	    DictionaryNode(ULong v_in = 0)
	    	: v(v_in)
	    { }
        inline ULong Raw() const
	{
	    return v;
	}
        inline Int IsTerminal() const
        {
            //return (int)((v>>30)&1);
            return (int)((v>>29)&1);
        }
        inline Int IsLastPeer() const
        {
            //return (int)((v>>31)&1);
            return (int)((v>>30)&1);
        }
        inline Int Index() const
        {
            //return (int)((v>>25)&0x1F);
            return (int)((v>>24)&0x1F)-1;
        }
        inline char Char() const
        {
            return (char)(Index()+'A');
        }
        inline ULong Mask() const
        {
            return (1ul << Index());
        }
        inline ULong FirstChild() const
        {
            //return (v&0x1FFFFFFul);
            return (v&0xFFFFFFul);
        }
    };
  protected:
    DictionaryNode Nstk[30];
    Int Node2Record(ULong n)
    {
        return (int)(n/NodesPerRecord);
    }
    FILE *fp;
    unsigned char *current_record;
    //VoidHand current_handle;
    Int current_recnum;

    Int FindPoolAllocation(Int pos, Int tot, Int wilds,
						ULong pmask = 0xfffffffful);
    Int RemoveFromPool(Int c);
    void ReplaceInPool(Int c);
    Int GrabLetter(DictionaryNode &n);
    void ReplaceLetter(DictionaryNode &n);
    char *NextPat(char *pat, Int pos);
    void InitStack(Int l, Int first_vec);
    void GetWord(char *word, Int stacklen);
    Int NextStep(char *line);
    DictionaryNode GetNode(ULong n);
  public:
    Int StartConsult(char *pat, Int type_in = USEALL,
    			Int multi_in = 0,
    			Int minlen_in=0, Int maxlen_in=0, 
			Int mincnt_in=0, Int maxcnt_in=0);
    Int NextMatch(char *line);
    void Reset();
    /* Boolean */ Int Open()
    {
        //return Database::Open(DictDBType);
	fp = fopen("dawg", "r");
	return fp ? 1 : 0;
    }
    DictionaryDatabase(/*class WordForm *owner_in*/);
    ~DictionaryDatabase();
};


class WordList //: public List
{
  protected:
    //class WordForm *owner;
    DictionaryDatabase &dict;
    char line[50];
    Int nomore;
    Int page;
  public:
    virtual char *GetItem(UInt itemNum);
    virtual Int NumItems();

  public:
    WordList(/*class WordForm *owner_in,*/ DictionaryDatabase &dict_in)
       : //List(WordFormWordsList),
         //owner(owner_in),
         dict(dict_in),
         nomore(0),
         page(0)
    {}
    Int NoMore() const
    {
        return nomore;
    }
    void Reset()
    {
        nomore = page = 0;
        dict.Reset();
    }
    void NextPage()
    {
        ++page;
    }
    Int Start(char *pat, UInt matchtype, UInt multi,
    			UInt minlength, UInt maxlength,
			UInt mincount, UInt maxcount);
};

DictionaryDatabase::DictionaryDatabase(/*class WordForm *owner_in*/)
    : //Database(),
      //owner(owner_in),
fp(0),
      current_record(0),
      current_recnum(-1),
      progpoint(0)
{
    // open the database
    (void)Open();
    if (fp)
    {
#if 1
	nodesize = 4;
        current_record = new unsigned char[NodesPerRecord*4];
#else
        VoidHand h = DmQueryRecord(db, (UInt)0);
        if (h)
        {
	    unsigned char *lp = (unsigned char*)MemHandleLock(h);
	    if (lp[2]==3)
	        nodesize = 3;
	    else
	        nodesize = 4;
            MemHandleUnlock(h);
        }
#endif
    }
}

DictionaryDatabase::~DictionaryDatabase()
{
    if (current_record) 
        /* MemHandleUnlock(current_handle); */ delete [] current_record;
}

DictionaryDatabase::DictionaryNode
    DictionaryDatabase::GetNode(ULong n)
{
--n;
    Int rn = Node2Record(n);
    if (current_recnum != rn)
    {
        // unlock the current record
            
#if 0
        if (current_record)
            MemHandleUnlock(current_handle);
                
        // Fetch and lock the `rn'th record
            
        current_handle = db ? DmQueryRecord(db, (UInt)rn) : 0;
        if (current_handle) // CHECK THIS!
        {
	    current_record = 
            	(unsigned char*)MemHandleLock(current_handle);
            current_recnum = rn;
        }
        else
        {
            current_record = 0;
            current_recnum = -1;
        }
#else
	fseek(fp, rn*NodesPerRecord*4, SEEK_SET);
	if (fread(current_record, 4, NodesPerRecord, fp)>0)
	{
            current_recnum = rn;
	}
        else
        {
            current_record = 0;
            current_recnum = -1;
        }
	
#endif
    }
    ULong v;
    if (current_record == 0)
        v = 0;
    else 
    {
        unsigned char *dp = current_record+(n%NodesPerRecord)*nodesize;
        if (nodesize == 3)
	{
            v = (dp[0]&0xFE);
            v = (v<<8) | (dp[0]&0x1);
            v = (v<<8) | dp[1];
            v = (v<<8) | dp[2];
	}
	else
	{
            v = *((ULong*)(dp));
	}
    }
    return DictionaryNode(v);
}

char *DictionaryDatabase::NextPat(char *pat, Int pos)
{
    while (isspace(*pat)) pat++;
    if (*pat)
    {
	ULong v;
        if (isupper(*pat) || *pat==':' || *pat == '*' || *pat == '=')
        {
	    // fixed
	    if (*pat==':') v = ALL;
	    else if (*pat=='*') v = VOWELS; 
	    else if (*pat=='=') v = CONSONANTS;
	    else
	    {
	        v = LetterMask(*pat);
	        if (pos==progpoint) ++progpoint;
	    }
	    anchormasks[vlen++] = v;
        }
        else if (islower(*pat))
	{
	    ++fixedpool[*pat-'a'];
	    ++maxpool[*pat-'a'];
	    anchormasks[vlen++] = 0l;
	}
	else if (*pat=='.' || *pat == '+' || *pat == '-')
	{
	    if (*pat=='.')
	    {
	        ++wildfloats;
		for (Int i = 0; i < 26; i++)
		    ++maxpool[i];
	    }
	    else if (*pat=='+')
	    {
		varpool[plen++] = VOWELS;
		++maxpool['A'-'A'];
		++maxpool['E'-'A'];
		++maxpool['I'-'A'];
		++maxpool['O'-'A'];
		++maxpool['U'-'A'];
	    }
	    else if (*pat=='-')
	    {
		varpool[plen++] = CONSONANTS;
		for (Int i = 0; i < 26; i++)
		    ++maxpool[i];
		--maxpool['A'-'A'];
		--maxpool['E'-'A'];
		--maxpool['I'-'A'];
		--maxpool['O'-'A'];
		--maxpool['U'-'A'];
	    }
	    anchormasks[vlen++] = 0l;
	}
	else if (*pat == '[' || *pat == '{' || *pat == '(' || *pat == '>')
	{
	    ULong mask = 0ul;
	    Int negate = 0;
	    pat++;
	    v = 0l;
	    if (*pat=='!' || *pat=='^')
	    {
		negate=1;
		pat++;
	    }
	    Int upper = isupper(*pat);
	    while (*pat!=']' && *pat != '}' && *pat != ')' && *pat != '>')
	    {
	    	if (*pat==0) return 0; /* syntax error */
		char start = *pat;
		if (isupper(start)) 
		    mask = LetterMask(start);
		else if (islower(start))
		{
		    start = toupper(start);
		    mask = LetterMask(start);
		}
		else return 0; // syntax error
		pat++;
		if (*pat=='-')
		{
		    pat++;
		    if (!isalpha(*pat)) return 0; // syntax error
		    char end = *pat;
		    pat++;
		    if (islower(end))
		        end = toupper(end);
		    while (start<=end)
		    {
			v |= mask;
			mask = LetterMask(++start);
		    }
		}
		else v |= mask;
	    }
	    if (negate) v ^= ALL;
	    if (upper)
	        anchormasks[vlen++] = v;
	    else
	    {
	        anchormasks[vlen++] = 0l;
		if (v == ALL)
		    ++wildfloats;
		else
	            varpool[plen++] = v;
		for (Int k = 0; k < 26; k++)
		    if (((1ul<<k) & v) != 0l)
			++maxpool[k];
	    }
	}
	pat++;
    }
    return (plen>32) ? 0 : pat; // only allow 32 floating ranges
    							// (i.e. ULong bitmask).
}

Int DictionaryDatabase::FindPoolAllocation(Int pos, Int tot, Int wilds,
						ULong pmask)
{
    if (tot<=wilds) return 0;
    while (need[pos]==0 && pos<26) ++pos;
    if (pos==26) return 0; // don't need anything
    for (Int j = 0; j < plen; j++)
    {
	ULong ppmask = (1ul<<j);
	if ((pmask & ppmask) != 0l) // haven't used this one
	{
	    if (((1ul<<pos) & varpool[j]) != 0l) // letter is in range
	    {
		--need[pos];
		Int rtn = FindPoolAllocation(pos, tot-1, wilds, (pmask & ~ppmask));
		++need[pos];
		if (rtn== 0) return 0;
	    }
	}
    }
    // No more range allocations; use a wild if we have
    if (wilds>0)
    {
	--need[pos];
	Int rtn = FindPoolAllocation(pos, tot-1, wilds-1, pmask);
	++need[pos];
	if (rtn == 0) return 0;
    }
    return -1;
}

Int DictionaryDatabase::RemoveFromPool(Int c)
{
    if (allocpool[c] == maxpool[c]) // no can do...
        return -1;
    else if (allocpool[c] < fixedpool[c]) // no problem...
    {
        ++allocpool[c];
	return 0;
    }

    // The letter must come from the var pool of floating ranges.
    // Try to find an allocation of the var pool that matches the
    // list in need[]...

    ++vtot;
    ++need[c];

    if (FindPoolAllocation(0, vtot, wildfloats) < 0) // no possible allocation...
    {
	--need[c];
	--vtot;
	return -1;
    }
    ++allocpool[c];
    return 0;
}

void DictionaryDatabase::ReplaceInPool(Int c)
{
    allocpool[c]--;
    if (fixedpool[c]==0)
        --vtot;
}

Int DictionaryDatabase::GrabLetter(DictionaryNode &n)
{
    ULong vmask = anchormasks[sp+vecoff];
    if (vmask)
    {
        if ((n.Mask() & vmask) != 0ul)
            return 0;
    }
    else if (RemoveFromPool(n.Index()) >= 0)
        return 0;
    return -1;
}

void DictionaryDatabase::ReplaceLetter(DictionaryNode &n)
{
    if (anchormasks[sp+vecoff]==0)
	 ReplaceInPool(n.Index());
}

void DictionaryDatabase::InitStack(Int l, Int first_vec)
{
    stop = l-1;
    nstk[sp = start = 0] = 1l;
    Nstk[0] = GetNode(1l);
    starts[1] = 0;
    vecoff = first_vec;
    swcnt = 1;
    vtot = 0;
    for (Int i = 0; i < 26; i++)
        need[i] = 0;
}

Int DictionaryDatabase::StartConsult(char *pat,
				      Int type_in,
				      Int multi_in,
				      Int minlen_in, Int maxlen_in,
				      Int mincnt_in, Int maxcnt_in)
{
    progpoint = 0;
    if (fp == 0)
    {
        Open();
	if (fp == 0) return -1;
    }
    for (Int i = 0; i < 26; i++)
        allocpool[i] = fixedpool[i] = maxpool[i] = 0;
    vlen = plen = wildfloats = 0;
    Int px = 0;
    while (pat && *pat) pat = NextPat(pat, px++);
    if (plen==0) // no floating ranges
    {
        // convert any wildfloats into fixed ALLs
	if (wildfloats>0)
	{
	    for (Int i = 0; i < vlen; i++)
	    {
	        if (anchormasks[i]==0)
		{
		    anchormasks[i] = ALL;
		    --wildfloats;
		}
	    }
	}
    }
    minlen = minlen_in; maxlen = maxlen_in;
    mincnt = mincnt_in;	maxcnt = maxcnt_in;
    if (minlen < 1) minlen = 1;
    if (maxlen < 1 || maxlen > vlen) maxlen = vlen;
    if (minlen < 1) minlen = 1;
    if (mincnt < 1) mincnt = 1;
    matchtype = type_in;
    if (!multi_in)
    {
        mincnt = maxcnt = 1;
    }
    else if (maxcnt < 1)
        maxcnt = vlen;
    pmask = 0l;
    // match=0;
    lastmulti[0] = 0;
    for (Int i = 0; i < plen; i++)
        pmask |= (1ul << i);
    switch(matchtype)
    {
    case USEALL:
	wlen[0] = vlen;
	vpos[0] = 0;
	numlens = 1;
	break;
    case PREFIX:
	for (Int i = minlen; i<=maxlen; i++)
	{
	    wlen[i-minlen] = i;
	    vpos[i-minlen] = 0;
	}
	numlens = maxlen-minlen+1;
	break;
    case SUFFIX:
	for (Int i = minlen; i<=maxlen; i++)
	{
	    wlen[i-minlen] = i;
	    vpos[i-minlen] = vlen-i;
	}
	numlens = maxlen-minlen+1;
    }
    lennow = 0;
    InitStack(wlen[0], vpos[0]);
    return 0;
}

void DictionaryDatabase::GetWord(char *word, Int stacklen)
{
    Int j = 0;
    for (Int i = 0; i<=stacklen; i++)
    {
	    if (i && nstk[i] <= 26)
	    {
	        word[j++] = ' ';
	        word[j++] = ' ';
	    }
	    word[j++] = Nstk[i].Char();
    }
    word[j] = 0;
}

// iTERATIVE IMPLEMENTATION OF RECURSIVE SEARCH

Int DictionaryDatabase::NextStep(char *line)
{
    Int rtn = 1; // 1 = got more; 0 = got word, -1 done
    DictionaryNode N = Nstk[sp];

    //((WordForm*)owner)->ShowDebug(sp, nstk[sp]);
    if (GrabLetter(N) == 0)
    {
        Int wlen = sp-start+1;
        if (sp == stop) // at the end?
        {
	    if (N.IsTerminal() && wlen>=minlen && swcnt>=mincnt)
	    {
		GetWord(line, stop);
		rtn = 0;
	    }
        }
	else if (N.IsTerminal() && wlen>=minlen && swcnt<maxcnt)
	{
	    // push on to next word
	    laststart = start;
#if 0
	    start = ++sp;
	    swcnt++;
#else
	    starts[swcnt++] = start = ++sp;
#endif
	    nstk[start] = nstk[laststart];
	    Nstk[start] = Nstk[laststart];
	    return rtn;
	}
        else if (N.FirstChild() && wlen<maxlen)
        {
	    // push on to next letter. If multi-word, enforce
	    // alphabetic ordering in subsequent words
	    if (start)
	    {
	        if (nstk[sp] == nstk[laststart+wlen-1] &&
		    (start-laststart)>wlen) 
		{
		    nstk[++sp] = nstk[laststart+wlen];
		    Nstk[sp] = Nstk[laststart+wlen];
		    return rtn;
		}
	    }
	    ++sp;
	    Nstk[sp] = GetNode(nstk[sp] = N.FirstChild());
	    return rtn;
        }
	ReplaceLetter(N);
    }
    for (;;)
    {
        if (!N.IsLastPeer()) // move to peer
	{
	    if (sp == (progpoint+1))
	    {
		    DictionaryNode n = Nstk[progpoint];
	        //((WordForm*)owner)->ShowProgress(((lennow+1)*(n.Index()*26+N.Index()))/(6*numlens)+1);
	    }
	    Nstk[sp] = GetNode(++nstk[sp]);
	    return rtn;
	}
	// no more peers; pop the stack
	if (sp==0) break;
	Int was_new_word;
	if (sp == start)
	{
	    was_new_word = 1;
	    swcnt--;
#if 0
	    // find last start point
	    start = laststart;
	    while (laststart > 0 && nstk[--laststart] > 26)
	        (void)0;
#else
	    start = laststart;
	    laststart = starts[swcnt-1];
#endif
	}
 	else was_new_word = 0;
	N = Nstk[--sp];
	if (was_new_word && N.FirstChild() && (sp-start+1)<maxlen)
	{
	    ++sp;
	    Nstk[sp] = GetNode(nstk[sp] = N.FirstChild());
	    return rtn;
	}
	else ReplaceLetter(N);
    }
    return -1; // no more
}

Int DictionaryDatabase::NextMatch(char *line)
{
#if 1
    for (;;)
    {
    	Int x = NextStep(line);
	if (x == -1)
	{
	    if (++lennow >= numlens) return -1;
	    InitStack(wlen[lennow], vpos[lennow]);
	}
	else if (x == 0) 
	    return 0;
    }
#else
    static ULong n = 1;
    DictionaryNode N = GetNode(n);
    sprintf(line, "%6ld %c %c %c %08lX %lu", n++,
    		(N.IsTerminal() ? 'T' : ' '),
    		(N.IsLastPeer() ? 'P' : ' '),
    		N.Char(), N.Raw(), N.FirstChild());
    return 0;
#endif
}

void DictionaryDatabase::Reset()
{
    lennow = 0;
    InitStack(wlen[lennow], vpos[lennow]);
}

Int WordList::Start(char *pat, UInt matchtype, UInt multi,
			UInt minlength, UInt maxlength, 
			UInt mincount, UInt maxcount)
{
    if (dict.StartConsult(pat, (int)matchtype, (int)multi,
        				(int)minlength, (int)maxlength,
        				(int)mincount, (int)maxcount) != 0)
    {
        nomore = 1;
        return -1;
    }
    nomore = page = 0;
    return 0;
}

Int WordList::NumItems()
{
    return 10;
}

char *WordList::GetItem(UInt itemNum)
{
    (void)itemNum;
    line[0] = 0;
    if (!nomore && dict.NextMatch(line) < 0)
    {
        nomore = 1;
        //((WordForm*)owner)->DisableNextButton();
    }
    return line;
}

int main(int argc, char **argv)
{
    (void)argc; (void) argv;
    DictionaryDatabase dict;
    WordList list(dict);
    list.Start((argc==2 ? argv[1] : ":.:"), USEALL, 0, 0, 0, 0, 0);
    int i = 0;
    while (!list.NoMore())
    {
        puts(list.GetItem(i++));
    }
}


