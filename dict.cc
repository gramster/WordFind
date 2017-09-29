#include "mypilot.h"

#define VARVECT

#ifndef TEST
#include "ptrsc.h"
#include "fw.h"
#endif

#include "dict.h"

#ifdef TEST
extern int debug;

void ShowVector(unsigned long *vec)
{
    printf("\nVar assignments:\n\n");
    for (int v = 0; v < 27; v++)
    {
        printf("%02d ", v);
	for (int c = 0; c < 26; c++)
	    if ((vec[v] & (1ul<<c)) != 0)
		printf("%c", c+'A');
	printf("\n");
    }
}

#endif

const char *ltoa(unsigned long l)
{
    if (l == 0)
    	return "0";
    else
    {
        static char x[10];
    	int p = 9;
	x[9]=0;
    	while (l)
    	{
            x[--p] = (char)((l%10)+'0');
            l = l / 10;
    	}
    	return x+p;
    }
}

#ifdef TEST
DictionaryDatabase::DictionaryDatabase() :
#else
DictionaryDatabase::DictionaryDatabase(class Form *owner_in) :
      Database(),
      owner(owner_in),
#endif
      varvector_out(0),
      current_record(0),
      current_recnum(-1),
      sp(0),
      start(0),
      laststart(0),
      stop(0),
      vecoff(0),
      swcnt(0),
      stepnum(0),
      matches(0),
      progress(0),
      pmask(0),
      vlen(0),
      plen(0),
      wildfloats(0),
      vtot(0),
      matchtype(0),
      mincnt(0),
      maxcnt(0),
      minlen(0),
      maxlen(0),
      //idx;
      numlens(0),
      lennow(0),
      progpoint(0),
      has_float_ranges(0),
      simple_search(1),
      has_place_constraints(0)

{
    memset((char*)allocpool, 0, sizeof(allocpool[0])*26);
    memset((char*)fixedpool, 0, sizeof(fixedpool[0])*26);
    memset((char*)maxpool, 0, sizeof(maxpool[0])*26);
    memset((char*)need, 0, sizeof(need[0])*26);
#ifdef VARVECT
    memset((char*)varvector_in, (char)0xff, sizeof(varvector_in[0])*27);
#endif
    memset((char*)nstk, 0, sizeof(nstk[0])*30);
    memset((char*)starts, 0, sizeof(starts[0])*32);
    memset((char*)anchormasks, 0, sizeof(anchormasks[0])*MAXPATLEN);
    memset((char*)varpool, 0, sizeof(varpool[0])*MAXPATLEN);
    memset((char*)wlen, 0, sizeof(wlen[0])*32);
    memset((char*)vpos, 0, sizeof(vpos[0])*32);
    memset((char*)lastmulti, 0, sizeof(lastmulti[0])*80);

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
        VoidHand h = QueryRecord(0);
        if (h)
        {
	    unsigned char *lp = (unsigned char*)MemHandleLock(h);
	    if (lp)
	    {
	        if (lp[2]==3)
	            nodesize = 3;
	        else
	            nodesize = 4;
	    }
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

const char *DictionaryDatabase::MatchTypeName(UInt t)
{
    switch(t)
    {
    case PREFIX: return "Prefixes";
    case SUFFIX: return "Suffixes";
    default:	return "All";
    }
}

const char *DictionaryDatabase::LimitName(UInt n)
{
    // these get used for labels and must be static for PalmOS
    switch(n)
    {
    case 0: return "Any";
    case 1: return "1";
    case 2: return "2";
    case 3: return "3";
    case 4: return "4";
    case 5: return "5";
    case 6: return "6";
    case 7: return "7";
    case 8: return "8";
    case 9: return "9";
    case 10:return "10";
    case 11:return "11";
    case 12:return "12";
    case 13:return "13";
    case 14:return "14";
    case 15:return "15";
    case 16:return "16";
    case 17:return "17";
    case 18:return "18";
    case 19:return "19";
    }
    return "";
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
            
        current_handle = db ? QueryRecord((UInt)rn) : 0;
        if (current_handle)
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
        if (nodesize == 3)
	{
	    unsigned char *dp = current_record+(n%NodesPerRecord)*3;
            v = (dp[0]&0xFE);
            v = (v<<8) | (dp[0]&0x1);
            v = (v<<8) | dp[1];
            v = (v<<8) | dp[2];
	}
	else
	{
	    unsigned char *dp = current_record+(n%NodesPerRecord)*4;
            v = *((unsigned long*)(dp));
	}
    }
    return DictionaryNode(v);
}

unsigned long DictionaryDatabase::NextPatternElt(char *&pat,
					int &is_float, int &is_single)
{
    unsigned long rtn = 0;
    if (isupper(*pat))
    {
	is_single = 1;
	is_float = 0;
	rtn = LetterMask(*pat);
    }
    else if (*pat==':' || *pat == '*' || *pat == '=')
    {
	is_single = 1;
	is_float = 0;
	// fixed
	if (*pat==':') rtn = ALL;
	else if (*pat=='*') rtn = VOWELS; 
	else if (*pat=='=') rtn = CONSONANTS;
    }
    else if (islower(*pat))
    {
	is_single = 1;
	is_float = 1;
	rtn = LetterMask((char)(*pat-('a'-'A')));
    }
    else if (*pat=='.' || *pat == '+' || *pat == '-')
    {
	is_single = 0;
	is_float = 1;
	if (*pat=='.') rtn = ALL;
	else if (*pat=='+') rtn = VOWELS; 
	else if (*pat=='-') rtn = CONSONANTS;
    }
    else if (*pat == '[' || *pat == '{' || *pat == '(' || *pat == '>')
    {
        unsigned long tmpmask = 0ul;
	int negate = 0;
	rtn = 0ul;
	pat++;
	rtn = 0l;
	if (*pat=='!' || *pat=='^')
	{
	    negate=1;
	    pat++;
	}
	is_float = islower(*pat);
	while (*pat!=']' && *pat != '}' && *pat != ')' && *pat != '>')
	{
	    if (*pat==0) return 0; /* syntax error */
	    char start = *pat;
	    if (isupper(start)) 
		tmpmask = LetterMask(start);
	    else if (islower(start))
	    {
		start = toupper(start);
		tmpmask = LetterMask(start);
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
		    rtn |= tmpmask;
		    tmpmask = LetterMask(++start);
		}
	    }
	    else rtn |= tmpmask;
	}
	if (negate) rtn ^= ALL;
    }
    return rtn;
}

char *DictionaryDatabase::NextPat(char *pat, int pos)
{
    while (isspace(*pat)) pat++;
    if (*pat)
    {
	unsigned long v;
#ifdef WORDEND
        if (*pat == '|')
	{
	    if (vlen>0)
	    {
		anchormasks[vlen-1] |= WORDEND;
	    	simple_search = 0;
		has_wordend_constraints = 1;
	    }
	}
	else
#endif
	if (isdigit(*pat)) // variable placeholder
	{
	    v = (*pat)-'0';
	    if (isdigit(pat[1]))
	    {
 		v = v*10 + (pat[1]-'0');
	    	++pat;
	    }
	    if (v > 26) return 0; // must be in range 0..26
	    anchormasks[vlen++] = VARNUM | v;
	    simple_search = 0;
	    if (pat[1]=='/') // associated letter is explicitly given
	    {
	        pat += 2;
#ifdef VARVECT
	        int is_float, is_single;
		if (varvector_out &&
		    (varvector_in[v] = NextPatternElt(pat, is_float, is_single)) == 0)
		    return 0; // syntax error
#endif
	    }
	}
        else
	{
	    int is_float, is_single;
	    v = NextPatternElt(pat, is_float, is_single);
	    if (v == 0) return 0; // syntax error
	    if (is_float)
	    {
	        anchormasks[vlen++] = 0l;
		if (is_single)
		{
	    	    ++fixedpool[*pat-'a'];
	    	    has_float_ranges = 1;
	    	    simple_search = 0;
		}
		else if ((v&ALL)==ALL)
		    ++wildfloats;
		else
		{
		    varpool[plen++] = v;
	            has_float_ranges = 1;
	            simple_search = 0;
		}
		for (int i = 0; i < 26; i++)
		    if ((v & (1ul<<i)) != 0)
		    	++maxpool[i];
	    }
	    else
	    {
		anchormasks[vlen++] = v;
		if (is_single && pos==progpoint) ++progpoint;
	    }
	}
	++pat;
    }
    return (plen>32) ? 0 : pat; // only allow 32 floating ranges
    				// (i.e. unsigned long bitmask).
}

int DictionaryDatabase::FindPoolAllocation(unsigned pos, int tot, int wilds,
						unsigned long pmask)
{
    if (tot<=wilds) return 0; // trivial to satisfy 
    while (need[pos]==0)
        if (++pos == 26)
	    return 0;
    unsigned long ppmask = 1, posmask = (1ul<<pos);
    for (int j = 0; j < plen; j++)
    {
	if ((pmask & ppmask) != 0l) // haven't used this one
	{
	    if ((posmask & varpool[j]) != 0l) // letter is in range
	    {
		--need[pos];
		int rtn = FindPoolAllocation(pos, tot-1, wilds, (pmask & ~ppmask));
		++need[pos];
		if (rtn== 0) return 0;
	    }
	    ppmask <<= 1;
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
    if (--allocpool[c] >= fixedpool[c]) // return to float?
    {
        --vtot;
	--need[c];
    }
}

int DictionaryDatabase::GrabLetter(DictionaryNode &n)
{
    unsigned long vmask = anchormasks[sp+vecoff];
    int idx = n.Index();
    if (vmask & VARNUM) // variable
    {
	int vnum = (int)(vmask & 0xff);
	if (vnum < 0 || vnum > 26) return -1;
	if (refcount[vnum]==0) // first use
	{
	    if (varset[idx]==0)	// mustn't be in use by a different var
#ifdef VARVECT
	    	if (varvector_out==0 ||
		   (varvector_in[vnum] & (1ul<<idx)) != 0) // must be allowed
#endif
	    {
		refcount[vnum] = 1;
		varset[idx] = vnum+1;
	    	return 0;
	    }
	}
	else if (varset[idx] == (vnum+1)) // must match earlier use
	{
	    ++refcount[vnum];
	    return 0;
	}
    }
    else if ((vmask & ALL) == 0) // floatiing
    {
        if (RemoveFromPool(idx) >= 0)
            return 0;
    }
    else // anchored
    {
	if (((1ul<<idx) & vmask) != 0ul)
            return 0;
    }
    return -1;
}

void DictionaryDatabase::ReplaceLetter(DictionaryNode &n)
{
    unsigned long vmask = anchormasks[sp+vecoff];
    if (vmask & VARNUM) // variable
    {
	if (--refcount[vmask&0xff] == 0)
	    varset[n.Index()] = 0;
    }
    else if ((vmask & ALL) == 0) // floating
	 ReplaceInPool(n.Index());
}

void DictionaryDatabase::InitStack(int l, int first_vec)
{
    stop = l-1;
    Nstk[0] = GetNode(nstk[sp = start = 0] = FIRSTNODE);
    vecoff = first_vec;
    swcnt = 1;
    vtot = 0;
    memset((char*)need, 0, sizeof(need[0])*26);
    memset((char*)starts, 0, sizeof(starts[0])*32);
    memset((char*)refcount, 0, sizeof(refcount[0])*27);
    memset((char*)varset, 0, sizeof(varset[0])*26);
}

int DictionaryDatabase::StartConsult(char *pat,
				      int type_in,
				      int multi_in,
				      int minlen_in, int maxlen_in,
				      int mincnt_in, int maxcnt_in,
				      unsigned long *varvector)
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
#ifdef VARVECT
    varvector_out = varvector;
    if (varvector)
    {
	for (int i = 0; i < 27; i++)
	{
	    varvector_in[i] = varvector[i];
	    if (varvector_in[i]&0x80000000ul)
	        varvector_in[i] = 0x3ffffffful;
	    varvector_out[i] = 0x80000000ul; // mark as unused
	}
    }
#endif
    memset((char*)allocpool, 0, sizeof(allocpool[0])*26);
    memset((char*)fixedpool, 0, sizeof(fixedpool[0])*26);
    memset((char*)maxpool, 0, sizeof(maxpool[0])*26);
    vlen = plen = wildfloats = has_float_ranges = 0;
    simple_search = 1;
    int px = 0;
    has_place_constraints = 0;
    has_wordend_constraints = 0;
    while (pat && *pat) pat = NextPat(pat, px++);
#ifdef WORDEND 
    if (has_wordend_constraints)
    {
	has_place_constraints = 1;
	anchormasks[vlen-1] |= WORDEND;
	multi_in = 1;
    }
#endif
    if (!has_float_ranges)
    {
        // convert any wildfloats into fixed ALLs
	if (wildfloats>0)
	{
	    for (int i = 0; i < vlen; i++)
	    {
	        if ((anchormasks[i]&ALL) == 0ul)
		{
		    anchormasks[i] |= ALL;
		    --wildfloats;
		}
	    }
	}
    }
    if (!has_place_constraints)
    {
        for (int i = 0; i < vlen; i++)
        {
            if ((anchormasks[i]&ALL) != 0ul)
	    {
	        has_place_constraints = 1;
	        break;
	    }
	}
    }
    minlen = minlen_in; maxlen = maxlen_in;
    mincnt = mincnt_in;	maxcnt = maxcnt_in;
    if (minlen < 1) minlen = 1;
    if (maxlen < 1 || maxlen > vlen) maxlen = vlen;
    matchtype = type_in;
    if (!multi_in)
    {
        mincnt = maxcnt = 1;
    }
    else
    {
        if (mincnt < 1) mincnt = 1;
        if (maxcnt < 1) maxcnt = vlen;
    }
    // match=0;
    lastmulti[0] = 0;
    pmask = 0ul;
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
        printf("Letter %c  Avail in Floating Pool: Min %d  Max %d\n", i+'A',
			fixedpool[i], maxpool[i]);
    }
    for (int i = 0; i < vlen; i++)
    {
        printf("Anchormask %d: %08X\n", i,  anchormasks[i]);
    }
    for (int i = 0; i < plen; i++)
    {
        printf("Var Pool %d: %08X\n", i,  varpool[i]);
    }
    ShowVector(varvector_in);
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
    return 0;
}

void DictionaryDatabase::ShowProgress(DictionaryNode &N)
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
}
// iTERATIVE IMPLEMENTATION OF RECURSIVE SEARCH

int DictionaryDatabase::NextStep(char *line)
{
    int rtn = 1; // 1 = got more; 0 = got word, -1 done
    DictionaryNode N = Nstk[sp];

#ifdef TEST
    if (debug)
    {
        GetWord(line, stop);
        printf("%d  sp %d  Node %d Line %s ", stepnum++, sp, nstk[sp], line);
        N.Print();
    }
#else
    ShowProgress(N);
    if ((++stepnum%100)==0 && MustStop())
        return -2;
#endif

    if (GrabLetter(N) == 0)
    {
        int wlen = sp-start+1;
#if 0
printf("sp %d vecoff %d anchormasks[sp+vecoff] %lX WORDEND %d is_term %d start %d wlen %d\n",
	sp, vecoff, anchormasks[sp+vecoff],
	(anchormasks[sp+vecoff]&WORDEND),
	N.IsTerminal(), start, wlen);
#endif
        if (sp == stop) // at the end?
        {
	    if (N.IsTerminal() && wlen>=minlen && swcnt>=mincnt)
	    {
		GetWord(line, stop);
#ifdef VARVECT
		// Update the set of possible variables
		if (varvector_out)
		{
		    for (int i = 0; i < 26; i++)
		    {
		        int var = varset[i];
		        if (var < 1 || var>26) continue;
		        if (refcount[--var]>0)
			{
			    varvector_out[var] &= ~0x80000000ul;
			    varvector_out[var] |= (1ul << i);
			}
		    }
		}
#endif
		rtn = 0;
	    }
        }
	else if (N.IsTerminal() && wlen>=minlen && swcnt<maxcnt
#ifdef WORDEND
		&& (!has_wordend_constraints || (anchormasks[sp+vecoff]&WORDEND))
#endif
		)
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
        else if (N.FirstChild() && wlen<maxlen
#ifdef WORDEND
		&& (!has_wordend_constraints ||
			(anchormasks[sp+vecoff]&WORDEND) == 0l)
#endif
		)
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
	    if (N.FirstChild() && (sp-start+1)<maxlen
#ifdef WORDEND
		&& (!has_wordend_constraints ||
			(anchormasks[sp+vecoff]&WORDEND) == 0l)
#endif
		)
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

extern void DrawDebugText(int, int, const char*);
long cnt = 0;

int DictionaryDatabase::NextMatch(char *line, int len)
{
    if (maxlen >= len) maxlen = len-1; // avoid overruns
#if 1 // set to 0 for low level dict testing
    for (;;)
    {
//	::DrawDebugText(100, 2, ltoa(++cnt));
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
    memset((char*)allocpool, 0, sizeof(allocpool[0])*26);
    lastmulti[0] = 0;
    pmask = 0l;
    for (int i = 0; i < plen; i++)
        pmask |= (1ul << i);
    lennow = stepnum = progress = matches = 0;
    InitStack(wlen[lennow], vpos[lennow]);
}

