#ifdef TEST
#include <stdio.h>

typedef unsigned short UInt;
typedef unsigned short Boolean;
#else
#ifdef __cplusplus
extern "C" {
#endif
#include <Pilot.h>
#include <Graffiti.h>
#include <SysEvtMgr.h>
#ifdef __cplusplus
}
#include "ptrsc.h"
#endif

#include "fw.h"
#include "dict.h"
#endif // TEST

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

#ifdef TEST
DictionaryDatabase::DictionaryDatabase() :
#else
DictionaryDatabase::DictionaryDatabase(class Form *owner_in) :
      Database(),
      owner(owner_in),
#endif
      current_record(0),
      current_recnum(-1),
      progpoint(0)
{
    // open the database
    (void)Open();
#ifdef TEST
    if (fp)
#else
    if (db)
#endif
    {
#ifdef TEST
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
#ifdef TEST
        delete [] current_record;
#else
        MemHandleUnlock(current_handle);
#endif
}

DictionaryDatabase::DictionaryNode
    DictionaryDatabase::GetNode(unsigned long n)
{
#ifdef TEST
    --n;
#endif
    int rn = Node2Record(n);
    if (current_recnum != rn)
    {
#ifdef TEST
	fseek(fp, rn*NodesPerRecord*4, SEEK_SET);
	if (fread(current_record, 4, NodesPerRecord, fp)>0)
	{
            current_recnum = rn;
	}
#else
        // unlock the current record
            
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
#endif
        else
        {
            current_record = 0;
            current_recnum = -1;
        }
    }
    unsigned long v;
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
            v = *((unsigned long*)(dp));
	}
    }
    return DictionaryNode(v);
}

char *DictionaryDatabase::NextPat(char *pat, int pos)
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
	    has_float_ranges = 1;
	}
	else if (*pat=='.' || *pat == '+' || *pat == '-')
	{
	    if (*pat=='.')
	    {
	        ++wildfloats;
		for (int i = 0; i < 26; i++)
		    ++maxpool[i];
	    }
	    else if (*pat=='+')
	    {
	        has_float_ranges = 1;
		varpool[plen++] = VOWELS;
		++maxpool['A'-'A'];
		++maxpool['E'-'A'];
		++maxpool['I'-'A'];
		++maxpool['O'-'A'];
		++maxpool['U'-'A'];
	    }
	    else if (*pat=='-')
	    {
	        has_float_ranges = 1;
		varpool[plen++] = CONSONANTS;
		for (int i = 0; i < 26; i++)
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
		if (v == ALL)
		    ++wildfloats;
		else
		{
	            varpool[plen++] = v;
	            has_float_ranges = 1;
		}
		for (int k = 0; k < 26; k++)
		    if (((1ul<<k) & v) != 0l)
			++maxpool[k];
	    }
	}
	pat++;
    }
    return (plen>32) ? 0 : pat; // only allow 32 floating ranges
    				// (i.e. unsigned long bitmask).
}

int DictionaryDatabase::FindPoolAllocation(int pos, int tot, int wilds,
						unsigned long pmask)
{
    if (tot<=wilds) return 0;
    while (need[pos]==0 && pos<26) ++pos;
    if (pos==26) return 0; // don't need anything
    for (int j = 0; j < plen; j++)
    {
	unsigned long ppmask = (1ul<<j);
	if ((pmask & ppmask) != 0l) // haven't used this one
	{
	    if (((1ul<<pos) & varpool[j]) != 0l) // letter is in range
	    {
		--need[pos];
		int rtn = FindPoolAllocation(pos, tot-1, wilds, (pmask & ~ppmask));
		++need[pos];
		if (rtn== 0) return 0;
	    }
	}
    }
    // No more range allocations; use a wild if we have
    if (wilds>0)
    {
	--need[pos];
	int rtn = FindPoolAllocation(pos, tot-1, wilds-1, pmask);
	++need[pos];
	if (rtn == 0) return 0;
    }
    return -1;
}

int DictionaryDatabase::RemoveFromPool(int c)
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

void DictionaryDatabase::ReplaceInPool(int c)
{
    if (--allocpool[c] >= fixedpool[c])
    {
        --vtot;
	--need[c];
    }
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
    Nstk[0] = GetNode(nstk[sp = start = 0] = FIRSTNODE);
    vecoff = first_vec;
    swcnt = 1;
    vtot = 0;
    for (int i = 0; i < 26; i++)
        need[i] = 0;
    starts[0] = starts[1] = 0;
}

int DictionaryDatabase::StartConsult(char *pat,
				      int type_in,
				      int multi_in,
				      int minlen_in, int maxlen_in,
				      int mincnt_in, int maxcnt_in)
{
    progpoint = 0;
#ifdef TEST
    if (fp == 0)
    {
        Open();
	if (fp == 0) return -1;
    }
#else
    if (db == 0)
    {
        Open();
	if (db == 0) return -1;
    }
#endif
    for (int i = 0; i < 26; i++)
        allocpool[i] = fixedpool[i] = maxpool[i] = 0;
    vlen = plen = wildfloats = 0;
    int px = 0;
    has_float_ranges = 0;
    while (pat && *pat) pat = NextPat(pat, px++);
    if (!has_float_ranges)
    {
        // convert any wildfloats into fixed ALLs
	if (wildfloats>0)
	{
	    for (int i = 0; i < vlen; i++)
	    {
	        if (anchormasks[i]==0)
		{
		    anchormasks[i] = ALL;
		    --wildfloats;
		}
	    }
	}
    }
    has_place_constraints = 0;
    for (int i = 0; i < vlen; i++)
    {
        if (anchormasks[i] != 0)
	{
	    has_place_constraints = 1;
	    break;
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
    // match=0;
    lastmulti[0] = 0;
    pmask = 0l;
    for (int i = 0; i < plen; i++)
        pmask |= (1ul << i);
    switch(matchtype)
    {
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
    lennow = progress = matches = 0;
    InitStack(wlen[0], vpos[0]);
#ifdef TEST
    printf("Mincnt %d Maxcnt %d\n", mincnt, maxcnt);
    printf("Minlen %d Maxlen %d\n", minlen, maxlen);
    printf("Wild Floats %d   Has Float Ranges %d\n", wildfloats, has_float_ranges);
    for (int i = 0; i < 26; i++)
    {
        printf("Letter %c  Fixed %d  Max %d\n", i+'A', fixedpool[i], maxpool[i]);
    }
    for (int i = 0; i < vlen; i++)
    {
        printf("Anchormask %d: %08X\n", i,  anchormasks[i]);
    }
    for (int i = 0; i < plen; i++)
    {
        printf("Var Pool %d: %08X\n", i,  varpool[i]);
    }
   
#endif
    return 0;
}

void DictionaryDatabase::GetWord(char *word, int stacklen)
{
    int j = 0;
    for (int i = 0; i<=stacklen; i++)
    {
	    if (i && nstk[i] <= 26)
	    {
	        word[j++] = ' ';
	        word[j++] = ' ';
	    }
        DictionaryNode n = Nstk[i];
	    word[j++] = n.Char();
    }
    word[j] = 0;
}


int DictionaryDatabase::MustStop()
{
#ifdef TEST
    return 0;
#else
    short x, y;
    Boolean pendown;
    EvtGetPen(&x, &y, &pendown);
    return (pendown && x>120 && y > 143);
#endif
}

// iTERATIVE IMPLEMENTATION OF RECURSIVE SEARCH

int DictionaryDatabase::NextSingleWordAllLettersStep(char *line)
{
    int rtn = 1; // 1 = got more; 0 = got word, -1 done
    DictionaryNode N = Nstk[sp];
#ifdef TEST
#if 0
    printf("sp %d  Node %d", sp, nstk[sp]);
    N.Print();
#endif
#else
    if ((++stepnum%100)==0 && MustStop())
        return -2;
#endif

    unsigned long vmask = anchormasks[sp];
    if ((N.Mask() & vmask) != 0ul)
    {
        if (sp == stop) // at the end?
        {
	    if (N.IsTerminal())
	    {
		GetWord(line, stop);
		rtn = 0;
	    }
        }
        else if (N.FirstChild() && sp < stop)
        {
	    // push on to next letter
	    ++sp;
	    Nstk[sp] = GetNode(nstk[sp] = N.FirstChild());
	    return rtn;
        }
	//ReplaceLetter(N);
    }
    for (;;)
    {
        if (!N.IsLastPeer()) // move to peer
	{
	    if (sp == (progpoint+1))
	    {
		DictionaryNode n = Nstk[progpoint];
#ifndef TEST
	        int p = ((n.Index()*26+N.Index()))/6;
		if (p != progress)
	            owner->ShowProgress(progress = p);
#endif
	    }
	    Nstk[sp] = GetNode(++nstk[sp]);
	    return rtn;
	}
	// no more peers; pop the stack
	if (sp==0) break;
	--sp;
	N = Nstk[sp];
	//ReplaceLetter(N);
    }
    return -1; // no more
}

int DictionaryDatabase::NextStep(char *line)
{
    int rtn = 1; // 1 = got more; 0 = got word, -1 done
    DictionaryNode N = Nstk[sp];

#ifdef TEST
#if 0
    printf("%d  sp %d  Node %d", stepnum++, sp, nstk[sp]);
    N.Print();
#endif
#else
    if ((++stepnum%100)==0 && MustStop())
        return -2;
#endif

    if (GrabLetter(N) == 0)
    {
        int wlen = sp-start+1;
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
	    starts[++swcnt] = start = ++sp;
	    if (has_place_constraints)
	    {
	        nstk[start] = FIRSTNODE;
	        Nstk[start] = GetNode(FIRSTNODE);
	    }
	    else
	    {
	        nstk[start] = nstk[laststart];
	        Nstk[start] = Nstk[laststart];
	    }
	    return rtn;
	}
        else if (N.FirstChild() && wlen<maxlen)
        {
	    // push on to next letter. If multi-word, enforce
	    // alphabetic ordering in subsequent words
	    if (start && !has_place_constraints)
	    {
	        int lwl = laststart+wlen;
	        if (nstk[sp] == nstk[lwl-1] && (start-laststart)>wlen) 
		{
		    ++sp;
		    nstk[sp] = nstk[lwl];
		    Nstk[sp] = Nstk[lwl];
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
	        int p;
		if (numlens == 1)
		    p = ((n.Index()*26+N.Index()))/6;
		else
		    p = ((lennow+1)*(n.Index()*26+N.Index()))/(6*numlens);
#ifndef TEST
		if (p != progress)
	            owner->ShowProgress(progress = p);
#endif
	    }
	    Nstk[sp] = GetNode(++nstk[sp]);
	    return rtn;
	}
	// no more peers; pop the stack
	if (sp==0) break;
	if (sp == start)
	{
	    // find last start point
	    // starts[1] = 0, starts[2] = 2nd wrd, starts[3] = 3rd wrd
	    // so 
	    start = laststart;
	    --swcnt;
#if 1
	    laststart = starts[swcnt-1];
#else

	    while (laststart > 0 && nstk[--laststart] > 26)
	        (void)0;
#endif
	    --sp;
	    N = Nstk[sp];
	    if (N.FirstChild() && (sp-start+1)<maxlen)
	    {
	        ++sp;
	        Nstk[sp] = GetNode(nstk[sp] = N.FirstChild());
	        return rtn;
	    }
	}
 	else
	{
	    --sp;
	    N = Nstk[sp];
	}
	ReplaceLetter(N);
    }
    return -1; // no more
}

int DictionaryDatabase::NextMatch(char *line)
{
#if 1
    // this could all be handled by one case, but for 
    // speed efficiency we handle the single word 
    // USEALL case separately...

    if (maxcnt <= 1 && matchtype==USEALL && !has_float_ranges)
    {
        for (;;)
        {
    	    int x = NextSingleWordAllLettersStep(line);
    	    if (x < 0)
    	        return x;
    	    else if (x == 0)
    	        return ++matches;
    	}
    }
    else
    {
        for (;;)
        {
    	    int x = NextStep(line);
    	    if ( x == -2) return -2;
	        else if (x < 0)
	        {
	            if (++lennow >= numlens)
	            return -1;
	            InitStack(wlen[lennow], vpos[lennow]);
	        }
	        else if (x == 0) 
	            return ++matches;
	    }
    }
#else
    DictionaryNode N = GetNode(++start);
#ifdef TEST
    sprintf(line, "%ld %c %c%c %ld", start, N.Char(),
		(N.IsTerminal()?'T':' '),
		(N.IsLastPeer()?'L':' '),
		N.FirstChild());
#else
    line[0] = N.IsTerminal() ? 'T' : ' ';
    line[1] = N.IsLastPeer() ? 'P' : ' ';
    line[2] = ' ';
    line[3] = N.Char();
    line[4] = ' ';
    strcpy(line+5, ltoa(N.FirstChild()));
#endif
    return 0;
#endif
}

void DictionaryDatabase::Reset()
{
    for (int i = 0; i < 26; i++)
        allocpool[i] = 0;
    lastmulti[0] = 0;
    pmask = 0l;
    for (int i = 0; i < plen; i++)
        pmask |= (1ul << i);
    lennow = stepnum = progress = matches = 0;
    InitStack(wlen[lennow], vpos[lennow]);
}

