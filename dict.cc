#include "mypilot.h"

#define VARVECT

#ifdef IS_FOR_PALM
#include "ptrsc.h"
#include "fw.h"
#endif // IS_FOR_PALM

#include "dict.h"

#ifdef DUMP
unsigned long gncnt = 0, dbcnt = 0;
int debug = 0;
#endif // DUMP

extern int debug;

void VariableSet::ComputeConstraints()
{
    for (int i = 0; i < 26; i++)
        if (used[i])
	    constraints[i] &= used[i];
}

void VariableSet::Print(char *buf, int v) const
{
    int dv = v+1; // display values start from 1 not 0
    buf[0] = (char)('0'+dv/10);
    buf[1] = (char)('0'+dv%10);
    if (vars[v])
    {
	buf[2] = '=';
	buf[3] = vars[v];
	buf[4] = 0;
    }
    else
    {
	buf[2] = ' ';
	int pos = 3;
	for (char c = 'A'; c < 'Z'; c++)
	    if (IsChInConstraint(v, c))
		buf[pos++] = c;
	buf[pos] = 0;
    }
}

void VariableSet::Show() const
{
#ifndef IS_FOR_PALM
    printf("\nVar assignments:\n\n");
    for (int v = 0; v < 26; v++)
    {
        char buf[40];
	Print(buf, v);
	puts(buf);
    }
#endif // IS_FOR_PALM
}

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

#ifdef IS_FOR_PALM
void DictionaryDatabase::Init()
#else
void DictionaryDatabase::Init(const char *fname)
#endif // IS_FOR_PALM
{
#ifdef IS_FOR_PALM
    owner = 0;
#endif
    vars = 0;
    current_record = 0;
    current_recnum = -1;
    sp = 0;
    start = 0;
    laststart = 0;
    stop = 0;
    vecoff = 0;
    swcnt = 0;
    stepnum = 0;
    matches = 0;
    progress = 0;
    pmask = 0;
    vlen = 0;
    plen = 0;
    wildfloats = 0;
    vtot = 0;
    matchtype = 0;
    mincnt = 0;
    maxcnt = 0;
    minlen = 0;
    maxlen = 0;
    numlens = 0;
    lennow = 0;
    progpoint = 0;
    has_float_ranges = 0;
    simple_search = 1;
    has_place_constraints = 0;
    memset((char*)allocpool, 0, sizeof(allocpool[0])*26);
    memset((char*)fixedpool, 0, sizeof(fixedpool[0])*26);
    memset((char*)maxpool, 0, sizeof(maxpool[0])*26);
    memset((char*)need, 0, sizeof(need[0])*26);
    memset((char*)nstk, 0, sizeof(nstk[0])*30);
    memset((char*)starts, 0, sizeof(starts[0])*32);
    memset((char*)anchormasks, 0, sizeof(anchormasks[0])*MAXPATLEN);
    memset((char*)varpool, 0, sizeof(varpool[0])*MAXPATLEN);
    memset((char*)wlen, 0, sizeof(wlen[0])*32);
    memset((char*)vpos, 0, sizeof(vpos[0])*32);
    memset((char*)lastmulti, 0, sizeof(lastmulti[0])*80);

    // open the database
#ifdef IS_FOR_PALM
    (void)Open();
    if (db)
#else
    (void)Open(fname);
    if (fp)
#endif // IS_FOR_PALM
    {
#ifdef UNIX
	nodesize = 4;
        current_record = new unsigned char[NodesPerRecord*4];
#else
#ifdef IS_FOR_PALM
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
#else
	// find the nodesize in the .pdb file
	unsigned char v[4];
	fseek(fp, 76, SEEK_SET);
	if (fread(v, 2, 1, fp)>0)
	    numrecords = (v[0]<<8) | v[1];
	nodeStart = 78 + 8 * numrecords;
	fseek(fp, nodeStart, SEEK_SET);
	if (fread(v, 4, 1, fp)>0)
	    if (v[2]==3)
	        nodesize = 3;
	    else
	        nodesize = 4;
        current_record = new unsigned char[NodesPerRecord*nodesize];
#endif // IS_FOR_PALM
#endif // UNIX
    }
}

DictionaryDatabase::~DictionaryDatabase()
{
    if (current_record) 
#ifdef IS_FOR_PALM
        MemHandleUnlock(current_handle);
#else
        delete [] current_record;
#endif // IS_FOR_PALM
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
#ifdef DUMP
    gncnt++;
#endif
#ifdef UNIX
    --n;
#endif // UNIX
    int rn = Node2Record(n);
    if (current_recnum != rn)
    {
#ifdef DUMP
        dbcnt++;
#endif
#ifdef IS_FOR_PALM
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
#else
        // !! must make nodeStart the offset in the pdb of the nodes,
	// !! and nodesize set to 3 or 4 as determined from pdb
	// nodeStart = 78 + 8 * nrecs
	// nrecs is stored as a short at offset 76
	// 
	fseek(fp, nodeStart + rn*NodesPerRecord*nodesize, SEEK_SET);
	if (fread(current_record, nodesize, NodesPerRecord, fp)>0)
	{
            current_recnum = rn;
	}
#endif // IS_FOR_PALM
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
	pat++;
	rtn = 0ul;
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
#endif // WORDEND
	if (isdigit(*pat)) // variable placeholder
	{
	    int var = (*pat)-'0';
	    if (isdigit(pat[1]))
	    {
 		var = var*10 + (pat[1]-'0');
	    	++pat;
	    }
	    if (var<1 || var > 26) return 0; // must be in range 1..26
	    --var;
	    anchormasks[vlen++] = VARNUM | var;
	    simple_search = 0;
	    if (pat[1]=='/') // associated letter is explicitly given
	    {
	        pat += 2;
#ifdef VARVECT
	        int is_float, is_single;
		if (vars)
		    if ((v = NextPatternElt(pat, is_float, is_single)) == 0)
		        return 0; // syntax error
		    else
		        vars->SetConstraint(var, v);
#endif // VARVECT
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
	int vnum = (int)(vmask & 0xff); // var #
	if (vnum < 0 || vnum > 26) return -1;
	if (refcount[vnum]==0) // first use of this var
	{
	    if (varset[idx]==0)	// letter mustn't be in use by a different var
#ifdef VARVECT
	    	if (vars==0 || vars->CanUseIdx(vnum, idx)) // must be allowed
#endif // VARVECT
	    {
		refcount[vnum] = 1; // variable in use
		varset[idx] = vnum+1; // bind letter to variable #
	    	return 0;
	    }
	}
	else if (varset[idx] == (vnum+1)) // is letter bound to var?
	{
	    ++refcount[vnum]; // up the reference count
	    return 0;
	}
	// else failure
    }
    else if ((vmask & ALL) == 0) // floatiing
    {
        if (RemoveFromPool(idx) >= 0)
            return 0;
	// else failure
    }
    else // anchored
    {
	if (((1ul<<idx) & vmask) != 0ul)
            return 0;
	// else failure
    }
    return -1;
}

void DictionaryDatabase::ReplaceLetter(DictionaryNode &n)
{
    unsigned long vmask = anchormasks[sp+vecoff];
#ifdef VARVECT
    if (vmask & VARNUM) // variable
    {
	if (--refcount[vmask&0xff] == 0) // variiable out of use?
	    varset[n.Index()] = 0; // unbind the letter
    }
    else
#endif // VARVECT
      if ((vmask & ALL) == 0) // floating
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
				      VariableSet *vars_in)
{
    progpoint = 0;
#ifdef IS_FOR_PALM
    if (db == 0)
    {
        Open();
	if (db == 0) return -1;
    }
#else
    if (fp == 0)
    {
        Open();
	if (fp == 0) return -1;
    }
#endif // IS_FOR_PALM
#ifdef VARVECT
    vars = vars_in;
    vars->ClearUsed();
#endif // VARVECT
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
#endif // WORDEND
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
    if (matchtype==USEALL)
    {
	wlen[0] = vlen;
	vpos[0] = 0;
	numlens = 1;
    }
    else if (matchtype==PREFIX)
    {
	for (int i = minlen; i<=maxlen; i++)
	{
	    wlen[i-minlen] = i;
	    vpos[i-minlen] = 0;
	}
	numlens = maxlen-minlen+1;
    }
    else if (matchtype==SUFFIX)
    {
	for (int i = minlen; i<=maxlen; i++)
	{
	    wlen[i-minlen] = i;
	    vpos[i-minlen] = vlen-i;
	}
	numlens = maxlen-minlen+1;
    }
    lennow = progress = matches = 0;
    InitStack(wlen[0], vpos[0]);
#ifndef IS_FOR_PALM
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
#endif // IS_FOR_PALM
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
#ifdef IS_FOR_PALM
    if (owner && sp == (progpoint+1))
    {
	DictionaryNode n = Nstk[progpoint];
        int p;
	if (numlens == 1)
	    p = ((n.Index()*26+N.Index()))/6;
	else
	    p = ((lennow+1)*(n.Index()*26+N.Index()))/(6*numlens);
	if (p != progress)
            owner->ShowProgress(progress = p);
    }
#else
    (void)N;
#endif // IS_FOR_PALM
}
// iTERATIVE IMPLEMENTATION OF RECURSIVE SEARCH

int DictionaryDatabase::NextStep(char *line)
{
    int rtn = 1; // 1 = got more; 0 = got word, -1 done
    DictionaryNode N = Nstk[sp];

#ifdef IS_FOR_PALM
    ShowProgress(N);
    if ((++stepnum%100)==0 && MustStop())
        return -2;
#else
    if (debug)
    {
        GetWord(line, stop);
        printf("%d  sp %d  Node %d Line %s ", stepnum++, sp, nstk[sp], line);
        N.Print();
    }
#endif // IS_FOR_PALM

    if (GrabLetter(N) == 0)
    {
        int wlen = sp-start+1;
#if 0
printf("sp %d vecoff %d anchormasks[sp+vecoff] %lX WORDEND %d is_term %d start %d wlen %d\n",
	sp, vecoff, anchormasks[sp+vecoff],
	(anchormasks[sp+vecoff]&WORDEND),
	N.IsTerminal(), start, wlen);
#endif //0
        if (sp == stop) // at the end?
        {
	    if (N.IsTerminal() && wlen>=minlen && swcnt>=mincnt)
	    {
		GetWord(line, stop);
#ifdef VARVECT
		// Update the set of possible variables
		if (vars)
		{
		    for (int i = 0; i < 26; i++) // letters
		    {
		        int var = varset[i]; // get var# used by letter
		        if (var<1 || var>26) continue;
//		        if (refcount[var-1]>0) // should be true
			   vars->UseIdx(var-1, i);
		    }
		}
#endif // VARVECT
		rtn = 0;
	    }
        }
	else if (N.IsTerminal() && wlen>=minlen && swcnt<maxcnt
#ifdef WORDEND
		&& (!has_wordend_constraints || (anchormasks[sp+vecoff]&WORDEND))
#endif // WORDEND
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
#endif // WORDEND
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
#endif // 1
	    --sp;
	    N = Nstk[sp];
	    if (N.FirstChild() && (sp-start+1)<maxlen
#ifdef WORDEND
		&& (!has_wordend_constraints ||
			(anchormasks[sp+vecoff]&WORDEND) == 0l)
#endif // WORDEND
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
	    {
#ifdef VARVECT
		// Update the set of variable constraints
		if (vars) vars->ComputeConstraints();
#endif // VARVECT
	        return -1;
	    }
	    InitStack(wlen[lennow], vpos[lennow]);
	}
	else if (x == 0) 
	    return ++matches;
    }
#else // !1
    DictionaryNode N = GetNode(++start);
#ifdef IS_FOR_PALM
    line[0] = N.IsTerminal() ? 'T' : ' ';
    line[1] = N.IsLastPeer() ? 'P' : ' ';
    line[2] = ' ';
    line[3] = N.Char();
    line[4] = ' ';
    strcpy(line+5, ltoa(N.FirstChild()));
#else
    sprintf(line, "%ld %c %c%c %ld", start, N.Char(),
		(N.IsTerminal()?'T':' '),
		(N.IsLastPeer()?'L':' '),
		N.FirstChild());
#endif // IS_FOR_PALM
    return 0;
#endif // 1
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


#ifdef DUMP

char newWord[32];

void DictionaryDatabase::RecursiveDump(int pos, long n, FILE *ofp)
{
    int c;
    if (n==(long)0) return;
    for (;;)
    {
    	DictionaryNode N = GetNode(n);
	newWord[pos] = N.Char();
	/* If the node is terminal, we have a valid matched word. */
	if (N.IsTerminal())
	{
	    newWord[pos+1]=0;
	    fprintf(ofp, "%s\n", newWord);
	}
	RecursiveDump(pos+1, N.FirstChild(), ofp);
	/* Move to next edge */
	if (!N.IsLastPeer()) n++;
	else break;
    }
}

void DictionaryDatabase::Dump(FILE *fp)
{
    RecursiveDump(0, 1l, fp);
}

void DictionaryDatabase::RawDump(FILE *fp)
{
    unsigned long i;
    long numnodes = numrecords * NodesPerRecord;
    fprintf(fp, "Nodes: %ld\n", numnodes);
    for (i=1;i<=numnodes;i++)
    {
	DictionaryNode N = GetNode(i);
	fprintf(fp, "%-6ld %-6ld %c%c %2d (%c)\n", (long)i,
			(long)N.FirstChild(), N.IsTerminal()?'T':' ',
			N.IsLastPeer()?'L':' ',
			(int)N.Index(), N.Char());
    }
}

#endif // DUMP


