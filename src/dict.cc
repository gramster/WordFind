#include "mypilot.h"

#ifdef IS_FOR_PALM
#include "fw.h"
#else // !IS_FOR_PALM
#define USE_REGEXP
#include "regex.h"
#endif // IS_FOR_PALM

#include "dict.h"

#ifdef DUMP
unsigned long gncnt = 0, dbcnt = 0;
int debug = 0;
#endif // DUMP

extern int debug;

#ifdef VARVECT

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
#endif

#ifdef IS_FOR_PALM
DictionaryDatabase::DictionaryDatabase()
    : Database()
#else
DictionaryDatabase::DictionaryDatabase(const char *fname)
#endif // IS_FOR_PALM
{
#ifdef IS_FOR_PALM
    current_record = 0;
#else
    strncpy(dbname, fname, 31);
    dbname[31]=0;
    current_record = new unsigned char[NodesPerRecord*4];
    fp = 0;
#endif
#ifdef VARVECT
    vars = 0;
#endif
#ifdef IS_FOR_PALM
    handles = 0;
    records = 0;
#endif
    current_recnum = -1;
    matchtype = 0;
    mincnt = 0;
    maxcnt = 0;
    minlen = 0;
    maxlen = 0;
    numlens = 0;
    re = 0;
}

DictionaryDatabase::~DictionaryDatabase()
{
#ifdef IS_FOR_PALM
    FreeRecords();
#else
    delete [] current_record;
#endif
#ifndef WORDFINDLITE
    delete re;
#endif
}

const char *DictionaryDatabase::MatchTypeName(UInt16 t)
{
    switch(t)
    {
    case PREFIX: return "Prefixes";
    case SUFFIX: return "Suffixes";
    default:	return "All";
    }
}

const char *DictionaryDatabase::LimitName(UInt16 n)
{
    return "Any\000"
           "1\000\000\000"
           "2\000\000\000"
           "3\000\000\000"
           "4\000\000\000"
           "5\000\000\000"
           "6\000\000\000"
           "7\000\000\000"
           "8\000\000\000"
           "9\000\000\000"
           "10\000\000"
           "11\000\000"
           "12\000\000"
           "13\000\000"
           "14\000\000"
           "15"+n*4;
#if 0    
    // these get used for labels and must be static for PalmOS
    static char rtn[5];
    char *p = rtn;
    if (n>9) *p++ = (char)(n/10+48);
    if (n>0) { *p++ = (char)(n%10+48); *p = 0; }
    else StrCopy(rtn, "Any");
    return rtn;
#endif
}

//----------------------------------------------------------------------------------
// Get a node from the dictionary database

DictionaryDatabase::DictionaryNode
    DictionaryDatabase::GetNode(unsigned long n)
{
#ifdef DUMP
    gncnt++;
#endif
#ifdef RAW_DAWG // nodes begin at 0 in dawgs but at 1 in .pdb files
    --n;
#endif // RAW_DAWG

#ifdef IS_FOR_PALM
    current_recnum = Node2Record(n);
    current_record = records[current_recnum];
#else
    int r = Node2Record(n);
    if (r != current_recnum)
    {
        current_recnum = r;
        fseek(fp, nodeStart+r*NodesPerRecord*nodesize, SEEK_SET);
        fread(current_record, nodesize, NodesPerRecord, fp);
    }
#endif
    
    unsigned long v;
    if (current_record == 0)
        v = 0;
    else 
    {
        if (nodesize == 3)
	{
	    // this should perhaps be written in assembly?
	    unsigned char *dp = current_record+(n%NodesPerRecord)*3;
            v = (dp[0]&0xFE);
            v = (v<<8) | (dp[0]&0x1);
            v = (v<<8) | dp[1];
            v = (v<<8) | dp[2];
	}
	else
	{
	    unsigned char *dp = current_record+(n%NodesPerRecord)*4;
#ifdef IS_FOR_PALM
            v = *((unsigned long*)(dp));
#else
            v = (dp[0]<<24) | (dp[1]<<16) | (dp[2]<<8) | dp[3];
#endif
	}
    }
    return DictionaryNode(v);
}

//------------------------------------------------------------------------------
// Compile a consultation template pattern

const char *DictionaryDatabase::NextPatternElt(char *&pat,
					int &is_float, int &is_single,
					unsigned long &rtn)
{
    rtn = 0;
    if (isupper(*pat))
    {
		is_single = 1;
		is_float = 0;
		rtn = LetterMask(*pat);
    }
    else if (*pat==FIX_ANY || *pat == FIX_VOWEL || *pat == FIX_CONSONANT)
    {
		is_single = 0;
		is_float = 0;
		// fixed
		if (*pat==FIX_ANY) rtn = ALL;
		else if (*pat==FIX_VOWEL) rtn = VOWELS; 
		else if (*pat==FIX_CONSONANT) rtn = CONSONANTS;
    }
    else if (islower(*pat))
    {
		is_single = 1;
		is_float = 1;
		rtn = LetterMask((char)(*pat-('a'-'A')));
    }
    else if (*pat==FLT_ANY || *pat == FLT_VOWEL || *pat == FLT_CONSONANT)
    {
		is_single = 0;
		is_float = 1;
		if (*pat==FLT_ANY) rtn = ALL;
		else if (*pat==FLT_VOWEL) rtn = VOWELS; 
		else if (*pat==FLT_CONSONANT) rtn = CONSONANTS;
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
		is_single = 0;
		while (*pat!=']' && *pat != '}' && *pat != ')' && *pat != '>')
		{
	    	if (*pat==0) return "Unterminated character range"; /* syntax error */
	    	char start = *pat;
	    	if (isupper(start)) 
	    	{
				tmpmask = LetterMask(start);
			}
	    	else if (islower(start))
	    	{
				start = toupper(start);
				tmpmask = LetterMask(start);
	    	}
	    	else
	    	{
	    		return "Invalid character in range"; // syntax error
	    	}
	    	pat++;
	    	if (*pat=='-')
	    	{
				pat++;
				if (!isalpha(*pat)) return "Invalid character following `-'"; // syntax error
				char end = *pat;
				pat++;
				if (islower(end))
				{
		    		end = toupper(end);
		    	}
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
    return 0;
}

const char *DictionaryDatabase::NextPat(char *&pat, int pos)
{
    const char *err;
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
#if defined(WORDFINDPP) || defined(WORDFINDLITE)
	    	return 0;
#else
	    	int var = (*pat)-'0';
	    	if (isdigit(pat[1]))
	    	{
 				var = var*10 + (pat[1]-'0');
	    		++pat;
	    	}
	    	if (var<1 || var > 26) return "Variable number out of range"; // must be in range 1..26
	    	--var;
	    	anchormasks[vlen++] = VARNUM | var;
	    	simple_search = 0;
#ifdef VARVECT
	    	if (pat[1]=='/') // associated letter is explicitly given
	    	{
	        	pat += 2;
	        	int is_float, is_single;
				if (vars)
				{
		    		if ((err = NextPatternElt(pat, is_float, is_single, v)) != 0)
		        		return err; // syntax error
		    		else
		        		vars->SetConstraint(var, v);
		        }
	    	}
#endif VARVECT
#endif
		}
        else // letter or letter range
		{
	    	int is_float, is_single;
	    	err = NextPatternElt(pat, is_float, is_single, v);
	    	if (err != 0) return err; // syntax error
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
				{
		    		++wildfloats;
		    	}
				else
				{
		    		varpool[plen++] = v;
	            	has_float_ranges = 1;
	            	simple_search = 0;
				}
				for (int i = 0; i < 26; i++)
				{
		    		if ((v & (1ul<<i)) != 0)
		    		{
		    			++maxpool[i];
		    		}
		    	}
	    	}
	    	else // anchored letter or letter range
	    	{
				anchormasks[vlen++] = v;
				if (is_single && pos==progpoint) ++progpoint;
	    	}
		}
		++pat;
    }
    return (plen>32) ? "Pattern has too many floats" : 0; // only allow 32 floating ranges
    							  // (i.e. unsigned long bitmask).
}

//--------------------------------------------------------------------
// Find an allocation of letters from the pool that matches our needs

int DictionaryDatabase::FindPoolAllocation(unsigned pos, int tot, int wilds,
						unsigned long pmask)
{
    if (tot<=wilds) return 0; // trivial to satisfy 
    while (need[pos]==0)
        if (++pos == 26)
	    return 0;
    unsigned long ppmask = 1, needmask = (1ul<<pos);
    for (int j = 0; j < plen; j++)
    {
	if ((pmask & ppmask) != 0l) // haven't used this one
	{
	    if ((needmask & varpool[j]) != 0l) // letter is in range
	    {
		--need[pos];
		int rtn = FindPoolAllocation(pos, tot-1, wilds, (pmask & ~ppmask));
		++need[pos];
		if (rtn== 0) return 0;
	    }
	}
	ppmask <<= 1;
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

//------------------------------------------------------------------------
// Remove a letter from the tile pool

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

//--------------------------------------------------------------------
// Replace a letter in the tile pool

void DictionaryDatabase::ReplaceInPool(int c)
{
    if (--allocpool[c] >= fixedpool[c]) // return to float?
    {
        --vtot;
	--need[c];
    }
}

//--------------------------------------------------------------------

int DictionaryDatabase::GrabLetter(DictionaryNode &n)
{
    unsigned long vmask = anchormasks[sp+vecoff];
    int idx = n.Index(); // get the letter index 0..25
    if (vmask & VARNUM) // variable
    {
	int vnum = (int)(vmask & 0xff); // Get the var # for this position
	if (vnum < 0 || vnum >= 26) return -1;
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

//---------------------------------------------------------------------
// Prepare for a new search of a specific length

void DictionaryDatabase::InitStack(int l, int first_vec)
{
    stop = l-1;
    if (stop>=totlen) stop=totlen-1;
    Nstk[0] = GetNode(nstk[sp = start = 0] = FIRSTNODE);
    vecoff = first_vec;
    swcnt = 1;
    vtot = 0;
    memset((char*)need, 0, sizeof(need[0])*26);
    memset((char*)starts, 0, sizeof(starts[0])*32);
    memset((char*)refcount, 0, sizeof(refcount[0])*27);
    memset((char*)varset, 0, sizeof(varset[0])*26);
}

//---------------------------------------------------------------------
// Clear everything ready for a new search

void DictionaryDatabase::Reinitialise()
{
    progpoint = 0;
    memset((char*)allocpool, 0, sizeof(allocpool[0])*26);
    memset((char*)fixedpool, 0, sizeof(fixedpool[0])*26);
    memset((char*)maxpool, 0, sizeof(maxpool[0])*26);
    vlen = plen = wildfloats = has_float_ranges = 0;
    simple_search = 1;
    has_place_constraints = 0;
    has_wordend_constraints = 0;
    pmask = 0ul;
    lennow = progress = matches = 0;
    memset((char*)nstk, 0, sizeof(nstk[0])*30);
    memset((char*)anchormasks, 0, sizeof(anchormasks[0])*MAXPATLEN);
    memset((char*)varpool, 0, sizeof(varpool[0])*MAXPATLEN);
    memset((char*)word_lengths, 0, sizeof(word_lengths[0])*32);
    memset((char*)start_offset_in_pat, 0, sizeof(start_offset_in_pat[0])*32);
    memset((char*)lastmulti, 0, sizeof(lastmulti[0])*80);
    sp = 0;
    start = 0;
    laststart = 0;
    stop = 0;
    vecoff = 0;
    swcnt = 0;
    stepnum = 0;
    vtot = 0;

    // open the database
#ifdef IS_FOR_PALM
    if (db)
#else
    if (fp)
#endif // IS_FOR_PALM
    {
#ifdef RAW_DAWG
	    nodesize = 4;
#else
#ifdef IS_FOR_PALM
        unsigned char *v = records[0];
        {
#else
	// find the nodesize in the .pdb file
	unsigned char v[4];
	fseek(fp, 76, SEEK_SET);
	if (fread(v, 2, 1, fp)>0)
	        numrecords = (v[0]<<8) | v[1];
	nodeStart = 78 + 8 * numrecords;
	fseek(fp, nodeStart, SEEK_SET);
	if (fread(v, 1, 4, fp)>0)
	{
#endif // IS_FOR_PALM
#ifdef DICTMGR
        if (v[1] < 4)
#endif
#ifdef WORDFINDLITE
        if (v[1] < 2)
#endif
#ifdef WORDFINDPP
		if ((v[1]%2) == 0) 
#endif
#ifdef WORDFINDPRO
        if ((v[1]%3) == 0)
#endif
	        {
	            nodesize = v[0];
#if 0 // backward compat with pre-Lite dicts; deprecated
		        if (nodesize==0)
		        {
		            // backward compatability with pre-Lite dicts
		            if (v[2]==3)
		               nodesize = 3;
		            else if (v[3]==4)
		                nodesize = 4;
		        }
#endif
	        }
	        else 
	            nodesize = 0;
        }
#endif // RAW_DAWG
    }
}

//---------------------------------------------------------------------------
// Set up the records pointers to the database

#ifdef IS_FOR_PALM

void DictionaryDatabase::InitRecords()
{
    handles = new MemHandle[numrecords];
    records = new unsigned char*[numrecords];
    for (int i = 0; i < numrecords; i++)
    {
        handles[i] = db ? QueryRecord((UInt16)i) : 0;
	if (handles[i])
	    records[i] = (unsigned char*)MemHandleLock(handles[i]);
	else
	    records[i] = 0;
    }
}

// and release them...

void DictionaryDatabase::FreeRecords()
{
    if (handles)
        for (int i = 0; i < numrecords; i++)
            MemHandleUnlock(handles[i]);
    delete [] handles;
    handles = 0;
    delete [] records;
    records = 0;
}

#endif IS_FOR_PALM

//-----------------------------------------------------------------------------
// Initialise the dictionary database

int DictionaryDatabase::InitDB()
{
#ifdef IS_FOR_PALM
    if (db == 0)
    {
        if (id) Database::OpenByID((LocalID)id, card, dmModeReadOnly);
        else Database::OpenByType(DictDBType, dmModeReadOnly);
	if (db == 0) return -1;
    }
    if (records == 0)
        InitRecords();
#else
    if (fp == 0)
    {
        Open(dbname);
    }
#endif // IS_FOR_PALM
    Reinitialise();
    if (nodesize == 0)
    {
#ifdef IS_FOR_PALM
        Close();
        FreeRecords();
	db = 0;
#else
	fclose(fp);
	fp = 0;
#endif
	return -1;
    }
    return 0;
}

//---------------------------------------------------------------------------
// Begin a template-based consultation

const char *DictionaryDatabase::StartConsult(char *pat,
				      int type_in,
				      int multi_in,
				      int minlen_in, int maxlen_in,
				      int mincnt_in, int maxcnt_in,
#ifdef VARVECT
				      VariableSet *vars_in)
#else
				      void *vars_in)
#endif
{
    if (pat == 0 || pat[0] == 0)
        return "No search pattern";
    if (InitDB() < 0)
        return "No dictionary";
#ifndef WORDFINDLITE
    delete re;
    re = 0;
    if (type_in == GLOB) // if no * ranges, convert to a template, else treat as glob
    {
        int l;
        for (l = 0; pat[l]; ++l)
        {
            if (pat[l] == '*')
            {
                re = new GlobPattern();
                break;
            }
        }
        if (re == 0) // convert to template
        {
            type_in = USEALL;
            while (--l>=0)
            {
                if (pat[l] == '?')
                    pat[l] = ':';
                else if (islower(pat[l]))
                    pat[l] = toupper(pat[l]);
            }
        }
    }
#ifdef USE_REGEXP
    else if (type_in == REGEXP) // XXXif no closure operators, and bounded by ^$, convert to a template, else treat as RE
    {
        re = new WFRegExp();
    }
#endif
    if (re)
    {
        int minl, maxl;
	const char *err = re->Compile(pat, remask, minmask, minl, maxl);
	if (err)
        {
            delete re;
            re = 0;
            return err;
        }
        if (minlen < minl) minlen = minl;
        if (maxlen==0 || maxlen > maxl) maxlen = maxl;
        numlens = 1;
        progpoint = 0;
        matches = 0;
        Nstk[0] = GetNode(nstk[sp = 0] = FIRSTNODE);
        // for efficiency, we decrement minlen as sp starts from 0 but
        // corresponds to lengths of 1 or more.
        --minlen;
        matchtype = type_in;
        return 0;
    }
#endif // !defined(WORDFINDLITE)
    if (type_in == MULTI)
    {
        type_in = USEALL;
	multi_in = 1;
    }
#ifdef VARVECT
    vars = vars_in;
    if (vars) vars->ClearUsed();
#else
    (void)vars_in;
#endif // VARVECT
    int px = 0;
    totlen = -1;
    
    while (pat && *pat)
    {
	if (*pat =='$')
	{
	    totlen = px;
	    ++pat;
	}
	else 
	{
	    const char *err = NextPat(pat, px++);
	    if (err) return err;
	}
    }
    if (pat == 0) return "Syntax error in pattern"; // syntax error
    if (totlen<0) totlen = px;
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
    if (minlen > maxlen && maxlen>0) minlen = maxlen;
    if (maxlen < 1 || maxlen > vlen) maxlen = vlen;
    if (minlen > vlen) minlen = vlen;
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
    for (int i = 0; i < plen; i++)
        pmask |= (1ul << i);
    if (matchtype==USEALL)
    {
	word_lengths[0] = vlen;
	start_offset_in_pat[0] = 0;
	numlens = 1;
    }
    else
    {
	int minmlen = mincnt*minlen;
	int maxmlen = maxcnt*maxlen;
	if (minmlen>vlen) minmlen = vlen;
	if (maxmlen>vlen) maxmlen = vlen;
	if (matchtype==PREFIX)
    	{
	    for (int i = minmlen; i<=maxmlen; i++)
	    {
	        word_lengths[i-minmlen] = i;
	        start_offset_in_pat[i-minmlen] = 0;
	    }
        }
        else if (matchtype==SUFFIX)
        {
	    for (int i = minmlen; i<=maxmlen; i++)
	    {
	        word_lengths[i-minmlen] = i;
	        start_offset_in_pat[i-minmlen] = vlen-i;
	    }
        }
	numlens = maxmlen-minmlen+1;
    }
    InitStack(word_lengths[0], start_offset_in_pat[0]);
#ifndef IS_FOR_PALM
    if (debug)
    {
        int i;
        printf("Mincnt %d Maxcnt %d\n", mincnt, maxcnt);
        printf("Minlen %d Maxlen %d\n", minlen, maxlen);
        printf("Wild Floats %d   Has Float Ranges %d\n", wildfloats, has_float_ranges);
        for (i = 0; i < 26; i++)
        {
            printf("Letter %c  Avail in Floating Pool: Min %d  Max %d\n", i+'A',
			fixedpool[i], maxpool[i]);
        }
        for (i = 0; i < vlen; i++)
        {
            printf("Anchormask %d: %08X\n", i,  anchormasks[i]);
        }
        for (i = 0; i < plen; i++)
        {
            printf("Var Pool %d: %08X\n", i,  varpool[i]);
        }
    }
#endif // IS_FOR_PALM
    return 0;
}

//---------------------------------------------------------------------------------
// Begin a WordFindLite-style consulatation - basically convert to a WordFindPro
// one and then call the StartConsult routine for WordFindPro.

#ifdef WORDFINDLITE
const char *DictionaryDatabase::StartConsult(const char *tmplate, const char *anagram)
{
    char pat[64];
    unsigned long anag_used = 0l;
    UInt16 l = (UInt16)strlen(tmplate), anag_l = 0, wcnt = 1;
    if (l >= sizeof(pat)) return "Pattern too long";
    if (anagram)
    {
        anag_l = (UInt16)strlen(anagram);
	if (anag_l > 32) return "Pattern too long";
    }
    for (int i = 0; i < l; i++)
    {
	char tch = tmplate[i];
	if (islower(tch)) tch = toupper(tch);
	pat[i] = tch;
	if (pat[i] == '|')
	    wcnt++;
        else if (anag_l && tmplate[i]!='?')
	{
	    // mark the template letter as used
	    // if it occurs in the anagram
	    for (int j = 0; j < anag_l; j++)
	    {
		char ach = anagram[j];
		if (ach=='|' || ((1l<<j)&anag_used)!=0) continue;
		if (islower(ach)) ach = toupper(ach);
		if (ach == tch)
		{
		    anag_used |= 1ul << j;
		    break;
		}
	    }
	}
    }
    if (anag_l)
    {
        // if anagram was longer, append some '?' chars. This allows us
        // to enter just an anagram 
        if (l == 0) // use '|' if in anagram
        {
            for (int i = 0 ; i < anag_l; i++)
	        if (anagram[i] == '|')
                    pat[l++] = '|';
	        else
                    pat[l++] = '?';
        }
        else // non-empty template; ignore '|'s in anagram and extend pat
        {
            int xtra = (int)(anag_l - l + wcnt - 1);
            for (int i = 0 ; i < anag_l; i++)
	        if (anagram[i] == '|') --xtra;
            while (xtra-->0)
                pat[l++] = '?';
	}
    }
    // convert '?' to ':' or a lower case unused letter from the anagram
    for (int i = 0; i < l; i++)
    {
        if (pat[i]=='?')
	{
	    pat[i] = FIX_ANY;
	    if (anag_l) // if any letters left in anag, use them
	    {
		for (int j = 0; j < anag_l; j++)
		{
		    char ach = anagram[j];
		    if (ach == '|' || ((1l<<j)&anag_used)!=0) continue;
		    if (isupper(ach)) ach = tolower(ach);
		    pat[i] = ach;
		    anag_used |= 1ul << j;
		    break;
		}
	    }
	}
    }
    pat[l] = 0;
    return StartConsult(pat);
}
#endif

//---------------------------------------------------------------------------
// Use the stack to reconstruct the current match as a printable string

unsigned long DictionaryDatabase::GetWord(char *word, int stacklen)
{
    int j = 0;
    unsigned long rtn = 0ul;
    for (int i = 0; i<=stacklen; i++)
    {
	if (i && nstk[i] <= 26)
	{
	    word[j++] = ' ';
	    word[j++] = ' ';
	}
        DictionaryNode n = Nstk[i];
	word[j++] = n.Char();
	rtn |= 1ul << n.Index();
    }
    word[j] = 0;
    return rtn;
}

//----------------------------------------------------------------------------
// Subclasses can override this to abort a search

int DictionaryDatabase::MustStop()
{
    return 0;
}

//----------------------------------------------------------------------------
// Give an indication of the search progress percentage

extern void ShowProgress(int p);

void DictionaryDatabase::ShowProgress()
{
#ifdef IS_FOR_PALM
    if (sp >= (progpoint+1))
    {
	DictionaryNode n1 = Nstk[progpoint];
	DictionaryNode n2 = Nstk[progpoint+1];
        int p;
	if (numlens == 1)
	    p = ((n1.Index()*26+n2.Index()))/6;
	else
	    p = ((lennow+1)*(n1.Index()*26+n2.Index()))/(6*numlens);
	if (p != progress)
	    ::ShowProgress(progress = p);
    }
#endif // IS_FOR_PALM
}

//-----------------------------------------------------------------------------
// Iterative implementation of a recursive search

int DictionaryDatabase::NextStep(char *line)
{
    int rtn = 1; // 1 = got more; 0 = got word, -1 done

#ifdef IS_FOR_PALM
    if ((++stepnum%100)==0)
    {
        ShowProgress();
        if (MustStop())
            return -2;
    }
#endif // IS_FOR_PALM
    DictionaryNode N = Nstk[sp];
#ifdef WORDFINDLITE
    if (1)
#else
    if (matchtype >= REGEXP)
    {
        if (sp || (remask & (1ul << N.Index())) != 0)
        {
            if (N.IsTerminal() && sp>=minlen)
            {
                unsigned long m = GetWord(line, sp);
                if ((m & minmask) == minmask && re->Match(line))
                    rtn = 0;
            }
            // move to next child, if there is one
            if (N.FirstChild() && sp < maxlen)
            {
	        ++sp;
	        Nstk[sp] = GetNode(nstk[sp] = N.FirstChild());
	        return rtn;
            }
        }
    }
    else // template
#endif
    {
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
		            int var = varset[i]-1; // get var# bound to letter i
		            if (var<0 || var>25) continue;
			    vars->UseIdx(var, i);
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
    }
    for (;;)
    {
        if (!N.IsLastPeer()) // move to peer
	{
	    Nstk[sp] = GetNode(++nstk[sp]);
	    break;
	}
	// no more peers; pop the stack
	else if (sp==0) 
	{
	    rtn = -1;
	    break;
	}
	else if (matchtype < REGEXP) // template?
	{
	    if (sp == start)
	    {
	        // find last start point
	        // starts[1] = 0, starts[2] = 2nd wrd, starts[3] = 3rd wrd
	        // so 
	        start = laststart;
	        --swcnt;
	        laststart = starts[swcnt-1];
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
	            break;
	        }
	    }
 	    else
	    {
	        N = Nstk[--sp];
	    }
	    ReplaceLetter(N);
	}
	else // glob or regexp
	{
	    N = Nstk[--sp];
	}
    }
    return rtn;
}

//extern void DrawDebugText(int, int, const char*);
//long cnt = 0;

int DictionaryDatabase::NextMatch(char *line, int len)
{
    if (matchtype>=REGEXP || maxlen>=len) maxlen = len-1; // avoid overruns
    for (;;)
    {
    	int x = NextStep(line);
    	if ( x == -2) return -2;
	else if (x < 0)
	{
	    if (matchtype>=REGEXP) return x;
	    if (++lennow >= numlens)
	    {
#ifdef VARVECT
		// Update the set of variable constraints
		if (vars) vars->ComputeConstraints();
#endif // VARVECT
	        return -1;
	    }
	    InitStack(word_lengths[lennow], start_offset_in_pat[lennow]);
	}
	else if (x == 0) 
	    return ++matches;
    }
}

//--------------------------------------------------------------------------
    
void DictionaryDatabase::Reset()
{
    memset((char*)allocpool, 0, sizeof(allocpool[0])*26);
    lastmulti[0] = 0;
    pmask = 0l;
    for (int i = 0; i < plen; i++)
        pmask |= (1ul << i);
    lennow = stepnum = progress = matches = 0;
    InitStack(word_lengths[lennow], start_offset_in_pat[lennow]);
}

//--------------------------------------------------------------------------
// Dump the database

#ifdef DUMP

char newUInt16[32];

void DictionaryDatabase::RecursiveDump(int pos, long n, FILE *ofp)
{
    int c;
    if (n==(long)0) return;
    for (;;)
    {
    	DictionaryNode N = GetNode(n);
	newUInt16[pos] = N.Char()+'a'-'A';
	/* If the node is terminal, we have a valid matched word. */
	if (N.IsTerminal())
	{
	    newUInt16[pos+1]=0;
	    fprintf(ofp, "%s\n", newUInt16);
	}
	RecursiveDump(pos+1, N.FirstChild(), ofp);
	/* Move to next edge */
	if (!N.IsLastPeer()) n++;
	else break;
    }
}

void DictionaryDatabase::Dump(FILE *fp)
{
#ifdef IS_FOR_PALM
    InitRecords();
#endif
    RecursiveDump(0, 1l, fp);
#ifdef IS_FOR_PALM
    FreeRecords();
#endif
}

void DictionaryDatabase::RawDump(FILE *fp)
{
    unsigned long i;
    long numnodes = numrecords * NodesPerRecord;
#ifdef IS_FOR_PALM
    InitRecords();
#endif
    fprintf(fp, "Nodes: %ld\n", numnodes);
    for (i=1;i<=numnodes;i++)
    {
	DictionaryNode N = GetNode(i);
	fprintf(fp, "%-6ld %08lX %-6ld %c%c %2d (%c)\n", (long)i,
			N.Raw(),
			(long)N.FirstChild(), N.IsTerminal()?'T':' ',
			N.IsLastPeer()?'L':' ',
			(int)N.Index(), N.Char());
    }
#ifdef IS_FOR_PALM
    FreeRecords();
#endif
}

#endif // DUMP
