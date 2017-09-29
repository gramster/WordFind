// TODO: help
//		 optimisations
//		 robustness with bad patterns, overruns
// Improve about dialog


#ifdef __cplusplus
extern "C" {
#endif
#include <Pilot.h>
#ifdef __cplusplus
}
#include "ptrsc.h"
#endif

#include "fw.h"

// derive the application

#define CLIENT_DB_TYPE		(0)

#define isspace(c)	((c)==' ' || (c)=='\t' || (c)=='\n')
#define isupper(c)	((c)>='A' && (c)<='Z')
#define islower(c)	((c)>='a' && (c)<='z')
#define toupper(c)	((char)(islower(c) ? ((c)-'a'+'A') : (c)))
#define isalpha(c)	(isupper(c) || islower(c))
#define InRange(v, mn, mx)	((v) >= (mn) && (v) <= (mx))

int strlen(char *s)
{
    char *b = s;
    while (*s) ++s;
    return (int)(s-b);
}

void strcpy(char *src, char *dst)
{
    while ((*src++ = *dst++) != 0)
        (void)0;
}

void strcat(char *src, char *dst)
{
    strcpy(src + strlen(src), dst);
}

int strcmp(char *s1, char *s2)
{
    while (*s1 && *s1 == *s2)
    {
        ++s1;
	++s2;
    }
    return (*s1-*s2);
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
    case PREFIX: return "Prefix";
    case SUFFIX: return "Suffix";
    case MULTI: return "Multiword";
    default:	return "Normal";
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
    return "None";
}

#define NodesPerRecord		1000
#define DictDBType		'dict'

class DictionaryDatabase : public Database
{
    int allocpool[26], fixedpool[26], varmax[26], need[26], request[26];
    // node stack for non-recursive matching
    unsigned long nstk[30];
    int sp, start, laststart, stop, vecoff, swcnt;
    unsigned long anchormasks[MAXPATLEN],
    				varpool[MAXPATLEN], pmask;
    int vlen, plen;
    int mincnt, maxcnt, minlen, maxlen, idx, matchtype;
    int numlens, lennow, wlen[32], vpos[32];
    char lastmulti[80];

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
            return (int)((v>>29)&1);
        }
        inline int IsLastPeer() const
        {
            return (int)((v>>30)&1);
        }
        inline int Index() const
        {
            return (int)((v>>24)&0x1F);
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
    };
  protected:
    struct DictionaryRecord
    {
        DictionaryNode nodes[NodesPerRecord];
    };
    int Node2Record(unsigned long n)
    {
        return (int)(n/NodesPerRecord);
    }
    
    DictionaryRecord *current_record;
    VoidHand current_handle;
    int current_recnum;

    int FindPoolAllocation(unsigned long avail);
    int RemoveFromPool(int c);
    void ReplaceInPool(int c);
    int GrabLetter(DictionaryNode &n);
    void ReplaceLetter(DictionaryNode &n);
    char *NextPat(char *pat);
    void InitStack(int l, int first_vec);
    void GetWord(char *word, int stacklen);
    int NextSWord(char *line);
    int NextMWord(char *line);
    DictionaryNode &GetNode(unsigned long n);
  public:
    int StartConsult(char *pat, int type_in = USEALL,
    			int minlen_in=0, int maxlen_in=0, 
			int mincnt_in=0, int maxcnt_in=0);
    int NextMatch(char *line);
    Boolean Open()
    {
        return Database::Open(DictDBType);
    }
    DictionaryDatabase();
    ~DictionaryDatabase();
};

DictionaryDatabase::DictionaryDatabase()
    : Database(), current_record(0), current_recnum(-1)
{
    // open the database
    (void)Open();
}

DictionaryDatabase::~DictionaryDatabase()
{
    if (current_record) 
        MemHandleUnlock(current_handle);
}

DictionaryDatabase::DictionaryNode
    &DictionaryDatabase::GetNode(unsigned long n)
{
    int rn = Node2Record(n);
    if (current_recnum != rn)
    {
        // unlock the current record
            
        if (current_record)
            MemHandleUnlock(current_handle);
                
        // Fetch and lock the `rn'th record
            
        current_handle = db ? DmQueryRecord(db, (UInt)rn) : 0;
        if (current_handle) // CHECK THIS!
        {
	    current_record = 
            	(DictionaryRecord*)MemHandleLock(current_handle);
            current_recnum = rn;
        }
        else
        {
            current_record = 0;
            current_recnum = -1;
        }
    }
    return current_record ?
        		current_record->nodes[n % NodesPerRecord] : 
        		DictionaryNode(0);
}

char *DictionaryDatabase::NextPat(char *pat)
{
    while (isspace(*pat)) pat++;
    if (*pat)
    {
	unsigned long v;
        if (isupper(*pat) || *pat==':' || *pat == '*' || *pat == '=')
        {
	    // fixed
	    if (*pat==':') v = ALL;
	    else if (*pat=='*') v = VOWELS; 
	    else if (*pat=='=') v = CONSONANTS;
	    else v = LetterMask(*pat);
	    anchormasks[vlen++] = v;
        }
        else if (islower(*pat))
	{
	    fixedpool[*pat-'a']++;
	    anchormasks[vlen++] = 0l;
	}
	else if (*pat=='.' || *pat == '+' || *pat == '-')
	{
	    if (*pat=='.')
	    {
		varpool[plen++] = ALL;
		for (int k = 0; k < 26; k++) ++varmax[k];
	    }
	    else if (*pat=='+')
	    {
		varpool[plen++] = VOWELS;
		varmax[0]++;
		varmax[4]++;
		varmax[8]++;
		varmax[14]++;
		varmax[20]++;
	    }
	    else if (*pat=='-')
	    {
		varpool[plen++] = CONSONANTS;
		for (int k = 0; k < 26; k++) ++varmax[k];
		varmax[0]--;
		varmax[4]--;
		varmax[8]--;
		varmax[14]--;
		varmax[20]--;
	    }
	    anchormasks[vlen++] = 0l;
	}
	else if (*pat == '[' || *pat == '{' || *pat == '(' || *pat == '>')
	{
	    unsigned long mask = 0ul;
	    int negate = 0;
	    pat++;
	    v = 0l;
	    if (*pat=='!' || *pat=='^')
	    {
		negate=1;
		pat++;
	    }
	    int upper = isupper(*pat);
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
	        varpool[plen++] = v;
		for (int k = 0; k < 26; k++)
		    if (((1l<<k) & v) != 0l)
			varmax[k]++;
	    }
	}
	pat++;
    }
    return pat;
}

int DictionaryDatabase::FindPoolAllocation(unsigned long avail)
{
    unsigned long lmask = 1ul;
    for (int i = 0; i < 26; i++, lmask <<= 1)
    {
	if (need[i]>0)
	{
	    if (avail == 0l) return -1;
	    unsigned long m = 1ul;
	    for (int j = 0; j < plen; j++, m<<=1)
	    {
		if ((avail & m) != 0 && (lmask & varpool[j]) != 0)
		{
		    need[i]--;
		    if (FindPoolAllocation((avail & ~m)) == 0)
		        return 0;
		    need[i]++;
		}
	    }
	    // no unused matching varpool patterns 8-(
	    return -1;
	}
    }
    // no need for any more allocations
    return 0;
}

int DictionaryDatabase::RemoveFromPool(int c)
{
    int f = fixedpool[c];

    if (allocpool[c] >= (f+varmax[c])) // no hope...
	    return -1;

    if (++allocpool[c] <= f) // no problem...
	    return 0;

    // the letter must come from the var pool.
    // Figure out how many letters we need...

#if 0
    for (int i = 0; i < 26; i++)
	if (allocpool[i] > fixedpool[i])
	    need[i] = allocpool[i] - fixedpool[i];
	else
	    need[i] = 0;
#else
    ++request[c];
    // we need an efficient memcpy 8-(
    for (int i = 0; i < 26; i++) need[i] = request[i];
#endif

    // now try to find an allocation of the var pool that matches the
    // list in need[]...

    if (FindPoolAllocation(pmask) < 0) // no possible allocation...
    {
	--request[c];
        --allocpool[c]; // put it back
	return -1;
    }
    return 0;
}

void DictionaryDatabase::ReplaceInPool(int c)
{
    allocpool[c]--;
}

int DictionaryDatabase::GrabLetter(DictionaryNode &n)
{
    unsigned long vmask = anchormasks[sp+vecoff];
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

void DictionaryDatabase::InitStack(int l, int first_vec)
{
    stop = l-1;
    nstk[sp = start = 0] = 1l;
    vecoff = first_vec;
    swcnt = 1;
}

int DictionaryDatabase::StartConsult(char *pat,
				      int type_in,
				      int minlen_in, int maxlen_in,
				      int mincnt_in, int maxcnt_in)
{
    if (db == 0)
    {
        Open();
	if (db == 0) return -1;
    }
    for (int i = 0; i < 26; i++)
        allocpool[i] = fixedpool[i] = varmax[i] = need[i] = request[i] = 0;
    vlen = plen = 0;
    while (pat && *pat) pat = NextPat(pat);
    minlen = minlen_in; maxlen = maxlen_in;
    mincnt = mincnt_in;	maxcnt = maxcnt_in;
    if (minlen < 1) minlen = 1;
    if (maxlen < 1 || maxlen > vlen) maxlen = vlen;
    if (minlen < 1) minlen = 1;
    if (mincnt < 1) mincnt = 1;
    matchtype = type_in;
    if (matchtype != MULTI) maxcnt = 1;
    else if (maxcnt < 1) maxcnt = vlen;
    pmask = 0l;
    // match=0;
    //lastmulti[0] = 0;
    for (int i = 0; i < plen; i++) pmask |= (1ul << i);
    switch(matchtype)
    {
    case MULTI:
    case USEALL:
	wlen[0] = vlen;
	vpos[0] = 0;
	numlens = 1;
	break;
    case PREFIX:
	for (int i = minlen; i<=maxlen; i++)
	{
	    wlen[i-minlen] = i;
	    vpos[i-minlen] = 0;
	}
	numlens = maxlen-minlen+1;
	break;
    case SUFFIX:
	for (int i = minlen; i<=maxlen; i++)
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

void DictionaryDatabase::GetWord(char *word, int stacklen)
{
    int j = 0;
    for (int i = 0; i<=stacklen; i++)
    {
	if (i && nstk[i] <= 26) word[j++] = ' ';
        DictionaryNode n = GetNode(nstk[i]);
	word[j++] = n.Char();
    }
    word[j] = 0;
}

int DictionaryDatabase::NextSWord(char *line)
{
    int rtn = 1; // 1 = got more; 0 = got word; -1 done
    DictionaryNode N = GetNode(nstk[sp]);

    // do we have the letter for the current node?

    if (GrabLetter(N) == 0)
    {
        int wlen = sp; // was sp-1 !
        if (sp == stop) // at the end?
        {
	        if (N.IsTerminal() && InRange(wlen, minlen, maxlen))
	        {
		        GetWord(line, stop);
		        rtn = 0;
	        }
        }
        else if (N.FirstChild() && wlen<maxlen)
        {
	        // push on to next letter
	        nstk[++sp] = N.FirstChild();
	        return rtn;
        }
	    ReplaceLetter(N);
    }

    // Determine whether to move to peer or bottom out

    for (;;)
    {
        if (!N.IsLastPeer()) // move to peer
	    {
	        nstk[sp]++;
	        return rtn;
	    }
	    // no more peers; pop the stack
	    if (sp==0) return -1;
        N = GetNode(nstk[--sp]);
	    ReplaceLetter(N);
    }
}


int DictionaryDatabase::NextMWord(char *line)
{
    int rtn = 1; // 1 = got more; 0 = got word or done
    DictionaryNode N = GetNode(nstk[sp]);

    int wlen = sp-start+1;
    if (GrabLetter(N) == 0)
    {
        if (sp == stop) // at the end?
        {
	    if (N.IsTerminal() && 
	        InRange(wlen, minlen, maxlen) &&
		InRange(swcnt, mincnt, maxcnt))
	    {
		    GetWord(line, stop);
		    rtn = 0;
	    }
        }
	else if (N.IsTerminal() && 
		 InRange(wlen, minlen, maxlen) &&
		 swcnt<=maxcnt)
	{
	    // push on to next word
	    laststart = start;
#if 0
	    nstk[start = ++sp] = 1l;
#else
	    nstk[start = ++sp] = nstk[laststart];
#endif
	    swcnt++;
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
		    return rtn;
		}
	    }
	    nstk[++sp] = N.FirstChild();
	    return rtn;
        }
	ReplaceLetter(N);
    }
    for (;;)
    {
        if (!N.IsLastPeer()) // move to peer
	{
	    nstk[sp]++;
	    return rtn;
	}
	// no more peers; pop the stack
	if (sp==0) break;
	int was_new_word;
	if (sp == start)
	{
	    was_new_word = 1;
	    // find last start point
#if 0
	    while (start > 0 && nstk[--start] > 26)
	        (void)0;
#else
	    start = laststart;
	    while (laststart > 0 && nstk[--laststart] > 26)
	        (void)0;
#endif
	    swcnt--;
	}
 	else was_new_word = 0;
	N = GetNode(nstk[--sp]);
	wlen = sp-start+1;
	if (was_new_word && N.FirstChild() && wlen<maxlen)
	{
	    nstk[++sp] = N.FirstChild();
	    return rtn;
	}
	else ReplaceLetter(N);
    }
    return -1; // no more
}

int DictionaryDatabase::NextMatch(char *line)
{
    for (;;)
    {
    	int x = (matchtype == MULTI) ? NextMWord(line) : NextSWord(line);
	if (x == -1)
	{
	    if (++lennow >= numlens) return -1;
	    InitStack(wlen[lennow], vpos[lennow]);
	}
	else if (x == 0) 
	    return 0;
    }
}

//---------------------------------------------------------

class RadioGroupDialog : public Dialog
{
    UInt firstcb;
    UInt numcbs;
    UInt val;
    CharPtr title;
  public:
    RadioGroupDialog(Form *parent_in,
    				UInt resid,
    				UInt firstcb_in,
    				UInt numcbs_in,
    				UInt val_in,
    				CharPtr title_in = 0)
        : Dialog(parent_in, resid),
          firstcb(firstcb_in),
          numcbs(numcbs_in),
          val(val_in),
          title(title_in) // must be static
    {}
    virtual void Init()
    {
    	Dialog::Init();
    	for (UInt i = 0; i < numcbs; i++)
    	{
            ControlPtr s = (ControlPtr)GetObject(firstcb+i);
			if (s)
	    		CtlSetValue(s, (Short)((i==val)?1:0));
	    }
        if (title) FrmSetTitle(frm, title);
    }
    UInt GetValue();
    virtual UInt Run()
    {
        (void)Dialog::Run();
		return GetValue();
    }
};

UInt RadioGroupDialog::GetValue()
{
    for (UInt i = 0; i < numcbs; i++)
    {
        ControlPtr s = (ControlPtr)GetObject(firstcb+i);
        if (s && CtlGetValue(s))
            return i;
    }
    return 0;
}

class MatchTypeDialog : public RadioGroupDialog
{
    UInt val;
  public:
    MatchTypeDialog(Form *parent_in, UInt val_in)
        : RadioGroupDialog(parent_in,
        					MatchTypeForm,
        					MatchTypeNormalCheckbox,
        					4,
        					val_in)
    {}
};

class WordLengthDialog : public RadioGroupDialog
{
  public:
    WordLengthDialog(Form *parent_in, UInt val_in, CharPtr title_in)
        : RadioGroupDialog(parent_in, 
        				WordLengthForm,
        				WordLengthNoLengthLimitCheckbox,
        				14,
        				val_in,
        				title_in)
    {}
};

class WordCountDialog : public RadioGroupDialog
{
  public:
    WordCountDialog(Form *parent_in, UInt val_in, CharPtr title_in)
        : RadioGroupDialog(parent_in,
        				 WordCountForm,
        				 WordCountNoCountLimitCheckbox,
        				 14,
        				 val_in,
        				 title_in)
    {}
};

class MainForm: public Form
{
  protected:
    UInt minlength, maxlength, mincount, maxcount;
    UInt matchtype;
    virtual Boolean Open();
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean HandleMenu(UInt menuID);
    void SetSelectorLabel(UInt resid, char *lbl);

  public:
    MainForm()
        : Form(MainFormForm),
          minlength(0), maxlength(0), mincount(0), maxcount(0),
          matchtype(USEALL)
    {
    }
};

class WordList : public List
{
  protected:
    class WordForm *owner;
    DictionaryDatabase &dict;
    char line[30];
    int nomore;
    virtual char *GetItem(UInt itemNum);
    virtual int NumItems();

  public:
    WordList(class WordForm *owner_in, DictionaryDatabase &dict_in)
       : List(WordFormWordsList),
         owner(owner_in),
         dict(dict_in)
    {}
    int Start(char *pat, UInt matchtype, UInt minlength,
    			 UInt maxlength, UInt mincount, UInt maxcount);
};

int WordList::Start(char *pat, UInt matchtype, UInt minlength,
    			 UInt maxlength, UInt mincount, UInt maxcount)
{
    if (dict.StartConsult(pat, (int)matchtype, 
        				(int)minlength, (int)maxlength,
        				(int)mincount, (int)maxcount) != 0)
    {
        nomore = 1;
        return -1;
    }
    nomore = 0;
    return 0;
}

int WordList::NumItems()
{
    return 10;
}

class WordForm : public Form
{
  protected:
  
    DictionaryDatabase dict;
    WordList list;
    
    virtual Boolean HandleSelect(UInt objID);
    virtual Boolean Open();
    virtual Boolean Update();
    virtual void Init();
    
  public:
  
    WordForm()
        : Form(WordFormForm),
          dict(),
          list(this, dict)
    { 
    }
    void EnableNextButton()
    {
        ControlPtr b = (ControlPtr)GetObject(WordFormNextButton);
        if (b) CtlSetEnabled(b, 1);
    }
    void DisableNextButton()
    {
        ControlPtr b = (ControlPtr)GetObject(WordFormNextButton);
        if (b) CtlSetEnabled(b, 0);
    }
    virtual List *GetList()
    {
        return &list;
    }
    int Start(CharPtr pat, UInt matchtype, UInt minlength,
    			UInt maxlength, UInt mincount, UInt maxcount)
    {
        return list.Start(pat, matchtype, minlength, maxlength,
      			mincount, maxcount);
    }
};


class PalmWordApplication : public Application
{
  protected:
    MainForm mainform;
    WordForm wordform;
    virtual Form *TopForm();
  public:
    PalmWordApplication();
    virtual Form *GetForm(UInt formID);
};

PalmWordApplication::PalmWordApplication()
  : Application(), mainform(), wordform()
{}

Form *PalmWordApplication::GetForm(UInt formID)
{
    switch (formID)
    {
    case MainFormForm:
        return &mainform;
    case WordFormForm:
	    return &wordform;
    }
    return 0;
}

void MainForm::SetSelectorLabel(UInt resid, char *lbl)
{
    ControlPtr sel = (ControlPtr)FrmGetObjectPtr(frm,
        				FrmGetObjectIndex(frm, resid));
    CtlSetLabel(sel, lbl);
}

Boolean MainForm::Open()
{
    Boolean rtn = Form::Open();
    FrmSetFocus(frm, FrmGetObjectIndex(frm, MainFormPatternField));
    SetSelectorLabel(MainFormMatchTypeSelTrigger, MatchTypeName(matchtype));
    SetSelectorLabel(MainFormMinLengthSelTrigger, LimitName(minlength));
    SetSelectorLabel(MainFormMaxLengthSelTrigger, LimitName(maxlength));
    SetSelectorLabel(MainFormMinCountSelTrigger, LimitName(mincount));
    SetSelectorLabel(MainFormMaxCountSelTrigger, LimitName(maxcount));
    return rtn;
}

Boolean MainForm::HandleSelect(UInt objID)
{
    switch (objID)
    {
    case MainFormMatchTypeSelTrigger:
    	{
    	MatchTypeDialog d(this, matchtype);
    	matchtype = d.Run();
	    SetSelectorLabel(MainFormMatchTypeSelTrigger, MatchTypeName(matchtype));
    	}
    	break;
    case MainFormMinLengthSelTrigger:
        {
	    WordLengthDialog d(this, minlength, "Min Word Length");
	    minlength = d.Run();
	    SetSelectorLabel(MainFormMinLengthSelTrigger, LimitName(minlength));
	    }
		break;
    case MainFormMaxLengthSelTrigger:
        {
	    WordLengthDialog d(this, maxlength, "Max Word Length");
	    maxlength = d.Run();
	    SetSelectorLabel(MainFormMaxLengthSelTrigger, LimitName(maxlength));
		}
		break;
    case MainFormMinCountSelTrigger:
        {
	    WordCountDialog d(this, mincount, "Min Word Count");
	    mincount = d.Run();
    	SetSelectorLabel(MainFormMinCountSelTrigger, LimitName(mincount));
	}
        break;
    case MainFormMaxCountSelTrigger:
        {
	    WordCountDialog d(this, maxcount, "Max Word Count");
	    maxcount = d.Run();
        SetSelectorLabel(MainFormMaxCountSelTrigger, LimitName(maxcount));

	}
        break;
    case MainFormGoButton:
        {
        FieldPtr fld = (FieldPtr)GetObject(MainFormPatternField);
	    CharPtr pat = fld ? FldGetTextPtr(fld) : 0;
	    if (pat && pat[0])
	    {
	        Application *a = Application::Instance();
            WordForm *wordform = a ? (WordForm*)a->GetForm(WordFormForm) : 0;
            if (wordform)
            {
                if (wordform->Start(pat, matchtype,
                				minlength, maxlength,
                				mincount, maxcount) == 0)
				    wordform->PostLoadEvent();
				else
					FrmAlert(NoDictBoxAlert); 
            }
        }
        break;
	    }
	default:
	    return False;
    }
	return True;
}

Boolean MainForm::HandleMenu(UInt menuID)
{
    switch(menuID)
    {
    case HelpAboutPalmWord:
        FrmAlert(AboutBoxAlert);
        return true;
    case EditUndo:
    case EditCut:
    case EditCopy:
    case EditPaste:
    case EditSelectAll:
      {
          Word focus = FrmGetFocus(frm);
          if (focus == noFocus)
              return false;
          FormObjectKind objtype = FrmGetObjectType(frm, focus);
          if (objtype != frmFieldObj)
              return false;
          FieldPtr fld = (FieldPtr)FrmGetObjectPtr(frm, focus);
		  if (!fld) return false;
	switch(menuID)
	{
    	case EditUndo:	FldUndo(fld);	break;
    	case EditCut:	FldCut(fld);	break;
    	case EditCopy:	FldCopy(fld);	break;
    	case EditPaste:	FldPaste(fld);	break;
    	case EditSelectAll:
		FldSetSelection(fld, 0, FldGetTextLength(fld));
		break;
      	}
      	return true;
      }
    case EditKeyboard:
        SysKeyboardDialog(kbdDefault);
	return true;
    case EditGraffiti:
        SysGraffitiReferenceDialog(referenceDefault);
	return true;
    }
    return false;
}

char *WordList::GetItem(UInt itemNum)
{
    (void)itemNum;
    line[0] = 0;
    if (!nomore && dict.NextMatch(line) < 0)
    {
        nomore = 1;
        ((WordForm*)owner)->DisableNextButton();
    }
    return line;
}

void WordForm::Init()
{
    Form::Init();
    EnableNextButton();
}

Boolean WordForm::HandleSelect(UInt objID)
{
    switch (objID)
    {
    case WordFormNewButton:
        {
	    Application *a = Application::Instance();
        MainForm *mainform = (MainForm*)a->GetForm(MainFormForm);
        if (mainform) mainform->PostLoadEvent();
	    return true;
	    }
    case WordFormNextButton:
//        list.Init();
        PostUpdateEvent();
	    return true;
    }
    return false;
}

Boolean WordForm::Open()
{
    list.Init();
    Boolean rtn = Form::Open();
    return rtn;
}

Boolean WordForm::Update()
{
    list.Erase();
    return false;
}

Form *PalmWordApplication::TopForm()
{
    return &mainform;
}

DWord RunApplication(Word cmd, Ptr cmdPBP, Word launchflags)
{
 	PalmWordApplication app;
	return app.Main(cmd, cmdPBP, launchflags);
}

