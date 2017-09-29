// Same as old way, but converted to C++

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
//#include <search.h>

//int debug = 0;

#define DUMP
#define DICTMGR
#define USE_REGEXP
#define USE_GLOB

typedef unsigned short UInt16;
typedef short Int16;
typedef unsigned long UInt32;
typedef long Int32;
#define StrCopy(d,s)	strcpy(d,s)
#define StrNCopy(d,s,n)	strncpy(d,s,n)

#include "regex.cc"
#include "dict.cc"

void StrUpr(char *s)
{
    while (*s) { if (islower(*s)) *s = toupper(*s); s++; }
}

/* compare two differing strings, return position of first difference*/

int cmpstr(char *s1, char *s2) // s2 should come before in sort
{
    register int pos = 0;
    while ((s1[pos]==s2[pos]) && s1[pos]) pos++;
    if (s1[pos] && s1[pos]<s2[pos])
    {
	fprintf(stderr,"ERROR: input must be sorted - unlike %s and %s\n",
			s2,s1);
	exit(0);
    }
    return pos;	
}

//----------------------------------------------------------------------

// The self member has a FREE bit, a TERMINAL bit, and 5 bits
// for the character. That leaves 25 bits. To speed up node
// compares (by reducing memcmps of the 26 children) we map the
// set of child letters into 22 of these bits, and the height of
// the subtree into 3 (we store (height&7)).
// To speed up the mapping of the 26 letters into 20 bits,
// we use this table:

static unsigned long lmasks[26] = 
{
    0x00000020ul, // A
    0x00000040ul, // B
    0x00000080ul, // C
    0x00000100ul, // D
    0x00000200ul, // E
    0x00000400ul, // F
    0x00000800ul, // G
    0x00001000ul, // H
    0x00002000ul, // I
    0x00000040ul, // J shares with B
    0x00004000ul, // K
    0x00008000ul, // L
    0x00010000ul, // M
    0x00020000ul, // N
    0x00040000ul, // O
    0x00080000ul, // P
    0x00004000ul, // Q shares with K
    0x00100000ul, // R
    0x00200000ul, // S
    0x00400000ul, // T
    0x00800000ul, // U
    0x01000000ul, // V
    0x02000000ul, // W
    0x04000000ul, // X shares with Y and Z
    0x04000000ul, // Y
    0x04000000ul  // Z
};

class Node
{
  protected:
    friend class NodePool;
#ifdef UNIX
    const unsigned long FREE = 0x80000000u;
    const unsigned long TERMINAL = 0x40000000u;
#else
#define FREE		0x80000000u
#define TERMINAL	0x40000000u
#endif
#ifdef USE_BITFIELDS
    unsigned free:1;
    unsigned terminal:1;
    unsigned depth:3;
    unsigned chmask:22;
    unsigned chidx:5;
#else
    unsigned long self;
#endif

    int children[26];	// indices of child nodes
  public:
    inline void Clear(int chidx_in = 0)
    {
#ifdef USE_BITFIELDS
        chidx = chidx_in;
#else
	self = (chidx_in&0x1f);
#endif
	memset(children, 0, 26 * sizeof(children[0]));
    }
    inline Node()
    {
	Clear();
    }
    inline ~Node()
    {}
    inline int IsFree() const
    {
#ifdef USE_BITFIELDS
	return free;
#else
	return (self & FREE) != 0;
#endif
    }
    inline int IsUsed() const
    {
#ifdef USE_BITFIELDS
	return (free == 0);
#else
	return (self & FREE) == 0;
#endif
    }
    inline void SetFree()
    {
#ifdef USE_BITFIELDS
	free = 1;
#else
	self |= FREE;
#endif
    }
    inline void SetUsed()
    {
#ifdef USE_BITFIELDS
	free = 0;
#else
	self &= ~FREE;
#endif
    }
    inline int IsTerminal() const
    {
#ifdef USE_BITFIELDS
	return terminal;
#else
	return (self & TERMINAL) != 0;
#endif
    }
    inline void SetTerminal()
    {
#ifdef USE_BITFIELDS
	terminal = 1;
#else
	self |= TERMINAL;
#endif
    }
    inline int Index() const
    {
#ifdef USE_BITFIELDS
	return chidx;
#else
	return self&0x1f;
#endif
    }
    inline unsigned long ChildMask() const
    {
#ifdef USE_BITFIELDS
	return chmask;
#else
        return (self & 0x07ffffe0ul);
#endif
    }
    inline unsigned long LengthMask() const
    {
#ifdef USE_BITFIELDS
	return depth;
#else
        return (self & 0x38000000ul);
#endif
    }
    inline void NoteDepth(int depth_in)
    {
#ifdef USE_BITFIELDS
	depth = depth_in&7;
#else
	self |= (((unsigned long)depth_in)&7)<<27;
#endif
    }
    inline void SetChild(int chidx, int childnode)
    {
#ifdef USE_BITFIELDS
	chmask |= 1ul<<chidx;
#else
	self |= lmasks[chidx];
        children[chidx] = childnode;
#endif
    }
    inline void ChainFree(unsigned nextfree)
    {
#ifdef USE_BITFIELDS
	free = 1;
	chmask = nextfree;
#else
	self = FREE|nextfree;
#endif
    }
    inline int NextFree() const
    {
#ifdef USE_BITFIELDS
	return chmask;
#else
        return self & ~(FREE|TERMINAL);
#endif
    }
    void Print(FILE *fp);
    int LastChild() const;
    int NumChildren() const;
};

void Node::Print(FILE *fp)
{
    if (IsTerminal()) fprintf(fp, "Terminal ");
    fprintf(fp, "Node %c (chmask %lX, dmask %lX)\n",
		Index()+'A', ChildMask(), LengthMask());
    for (int j=0; j<26; j++)
    {
	if (children[j])
	    fprintf(fp, "   Child %d (%c)\n", children[j], (char)(j+'A'));
    }
}

int Node::LastChild() const
{
    for (int j=26; --j >= 0; )
    {
        if (children[j]) return j;
    }
    return -1;
}

int Node::NumChildren() const
{
    int chld = 0;
    for (int j=0; j<26; j++)
        if (children[j])
    	chld++;
    return chld;
}

//----------------------------------------------------------------------

class NodePool
{
  public:
#ifdef UNIX
    const int MAXNODES = 250000;	/* max nodes in pool	*/
   const int MAXLEN = 30;		/* max word length	*/
   const int MAXLINELEN = 64;		/* max input line length */
#else
#define MAXNODES	250000
#define MAXLEN		30
#define MAXLINELEN	64
#endif

  private:
    Node node[MAXNODES];
    int index[MAXNODES];
    int botnode, topnode, freenodes, usednodes;
    int wcnt;

    char word[MAXLEN+1],lastword[MAXLEN+1];
    int	nodestack[MAXLEN];
    int stacktop;

  public:
    void Init(); // was Setup
    NodePool()
    {
	Init();
    }
    int AllocateNode(int chidx);
    void MarkGraph(int n);
    void MarkGraph();
    unsigned long IndexNodes();
    void DumpGraph(FILE *fp, int n, int pos, int ch);
    inline void DumpGraph(FILE *fp)
    {
	DumpGraph(fp, 0, 0, ' ');
    }
    void PrintGraph(FILE *fp);
    void WriteNode(FILE *fp, int n);
    void CheckOptimality();
    void WriteDawg(FILE *fp);
    void WritePalmDB(FILE *fp, unsigned long nnodes,
    			const char *name = 0,
			const char *srcname = 0,
			int progid = 0);
    void AddWord(char *word, int pos);
    void FreeNode(int n);
    int MergeNodes(int first, int last, int n, int parent);
    inline unsigned int NodeCompare(int n1, int n2) const
    {
#ifdef USE_BITFIELDS
	return (node[n1].chmask == node[n2].chmask &&
	        node[n1].chidx == node[n2].chidx &&
	        node[n1].terminal == node[n2].terminal &&
#else
	return (node[n1].self == node[n2].self &&
#endif
		memcmp(node[n1].children,node[n2].children, 26*sizeof(int))==0);
    }
    inline int MergeNodes(int i)
    {
	/* This funny way of doing it is (hopefully!) a performance improvement
	   as we no longer have to check for equality in NodeCompare */
	int n = nodestack[i], parent = nodestack[i-1];
	if (MergeNodes(1, n-1, n, parent))
	    return 1;
	else
	    return MergeNodes(n+1, topnode, n, parent);
    }
    void Merge(int pos = 0);
    void ShowStats() const
    {
#if 0
	printf("Max space used: %%%5.2f,  Final space used %%%5.2f\n",
		(double)topnode*100./(double)MAXNODES,
		(double)usednodes*100./(double)MAXNODES);
#else
	printf("Max space used: %%%5.2f\n",
		(double)topnode*100./(double)MAXNODES);
#endif
    }
};

void NodePool::Init() // was Setup
{
    node[0].Clear();
    /* set up free list */
    for (int i=1; i<MAXNODES; i++)
        node[i].ChainFree(i+1);
    node[MAXNODES-1].ChainFree(0);
    botnode = 1;
    topnode = usednodes = 0;
    freenodes = MAXNODES-1;
    wcnt = 0;
    stacktop = 0;
}

int NodePool::AllocateNode(int chidx)
{
    if (botnode == 0)
    {
        fprintf(stderr,"Out of nodes!\n");
        exit(0);
    }
    int i = botnode;
    if (i>topnode) topnode = i;
    botnode = node[i].NextFree();
    node[i].Clear(chidx);
    --freenodes;
    return i;
}

void NodePool::MarkGraph(int n)
{
    if (node[n].IsFree())
    {
        node[n].SetUsed();
        usednodes++;
        for (int *start = node[n].children, *end = node[n].children+26;
	     start < end;
    	     start++)
        {
            if (*start) MarkGraph(*start);
        }
    }
}

void NodePool::MarkGraph()
{
    for (int n=0; n<=topnode; n++)
        node[n].SetFree();
    usednodes = 0;
    MarkGraph(0);
}

void NodePool::DumpGraph(FILE *fp, int n, int pos, int ch)
{
    fprintf(fp, "%6d%*c%c\n", n, pos, ' ',
		(node[n].IsTerminal() ? toupper(ch) : ch));
    for (int j=0; j<26; j++)
        if (node[n].children[j])
   	    DumpGraph(fp, node[n].children[j], pos+2, j+'a');
}

void NodePool::PrintGraph(FILE *fp)
{
    for (int i=0; i<=topnode; i++)
    {
        if (node[i].IsUsed())
        {
    	printf("%7d ", i);
    	node[i].Print(fp);
        }
    }
}

void NodePool::WriteNode(FILE *fp, int n)
{
    int last = node[n].LastChild();
    for (int j=0; j<=last; j++)
    {
        int ch = node[n].children[j];
        if (ch)
        {
    	    unsigned  long v = (unsigned long)index[ch];
    	    if (node[ch].IsTerminal())
    	        v |= 0x20000000l;
    	    if (j==last)
    	        v |= 0x40000000l;
    	    v += ((unsigned long)(j+1))<<24;
    	    fwrite(&v, 1, sizeof(unsigned long), fp);
#ifdef DEBUG
    	    if (debug)
	        printf("  Writing edge for %c (%08lX)\n",(char)(j+'A'-1),v);
#endif
        }
    }
}

void NodePool::CheckOptimality()
{
    unsigned mask = 0;
    for (int i=0; i<=topnode; i++)
    {
        if (node[i].IsUsed() && node[i].ChildMask()==0)
	{
	    int chidx = node[i].Index();
	    unsigned long v = 1ul << chidx;
	    if ((mask & v) != 0)
	        fprintf(stderr, "Duplicate leaf for %c\n", 'A'+chidx);
	    mask |= v; 
	}
    }
    fprintf(stderr, "Optimality mask %lX (expect 3FFFFFF)\n", mask);
}

unsigned long NodePool::IndexNodes()
{
    long idx = 1;
    for (int i=0; i<=topnode; i++)
    {
        index[i] = 0;
        if (node[i].IsUsed())
        {
    	    int chld = node[i].NumChildren();
    	    if (chld)
    	    {
    	        index[i] = idx;
    	        idx += chld;
    	    }
        }
    }
    return idx;
}

void NodePool::WriteDawg(FILE *fp)
{
    for (int i=0; i<=topnode; i++)
    {
        if (index[i])
        {
            WriteNode(fp, i);
        }
    }
}

void NodePool::WritePalmDB(FILE *dawg,
				unsigned long nnodes,
				const char *name,
				const char *srcname,
				int progid)
{
    char nbuf[32];
    if (name==0) name = srcname;
    sprintf(nbuf, "%.16s (%d words)",
		((name&&name[0])?name:"unknown"), wcnt);
    nbuf[31] = 0;

    // write out the headers

    unsigned char shortbuf[2], longbuf[4];

    // name

    fwrite(nbuf, sizeof(char), 32, dawg);

    // attributes

    memset(shortbuf, 0, 2);
    fwrite(shortbuf, sizeof(char), 2, dawg);

    // version

    memset(shortbuf, 0, 2);
    fwrite(shortbuf, sizeof(char), 2, dawg);

    // creation and modification date

    unsigned long now = time(0);
    unsigned long delta = 49ul * 365ul * 24ul * 3600ul +
     	17ul * 366ul * 24ul * 3600ul; // 1904 -> 1970
    now += delta;
    longbuf[0] = (now>>24)&0xFF;
    longbuf[1] = (now>>16)&0xFF;
    longbuf[2] = (now>>8)&0xFF;
    longbuf[3] = (now)&0xFF;
    fwrite(longbuf, sizeof(char), 4, dawg);
    fwrite(longbuf, sizeof(char), 4, dawg);

    // last backup date

    memset(longbuf, 0, 4);
    fwrite(longbuf, sizeof(char), 4, dawg);

    // modification number

    memset(longbuf, 0, 4);
    fwrite(longbuf, sizeof(char), 4, dawg);

    // appinfo area

    memset(longbuf, 0, 4);
    fwrite(longbuf, sizeof(char), 4, dawg);

    // sortinfo area

    memset(longbuf, 0, 4);
    fwrite(longbuf, sizeof(char), 4, dawg);

    // database type

    strcpy((char*)nbuf, "dict");
    fwrite(nbuf, sizeof(char), 4, dawg);

    // creator ID

    strcpy((char*)nbuf, "xWrd");
    fwrite(nbuf, sizeof(char), 4, dawg);

    // unique ID seed

    memset(longbuf, 0, 4);
    fwrite(longbuf, sizeof(char), 4, dawg);

    // NextRecordList ID

    memset(longbuf, 0, 4);
    fwrite(longbuf, sizeof(char), 4, dawg);

    // number of records

    if (debug) printf("Nodes: %lu\n", nnodes);

    nnodes++; // add one for the node 0
    unsigned short nrecs = (nnodes+NodesPerRecord-1) / NodesPerRecord;
    if (debug) printf("Records: %u\n", nrecs);
    shortbuf[0] = nrecs/256;
    shortbuf[1] = nrecs%256;
    fwrite(shortbuf, sizeof(char), 2, dawg);

    // write out the record list of record offsets.
    // each of these is 8 bytes. Thus the actual
    // records are at offsets (78+8*nrecs),
    // (78+8*nrecs+NodesPerRecord*4), ...

    int nodesize = (nnodes>=0x1FFFFul) ? 4 : 3; // 3 = packed demo dict
//fprintf(stderr, "nodesize %d \n", nodesize);
    unsigned long record_offset = ftell(dawg) + 8*nrecs;
    for (int i = 0; i < nrecs; i++)
    {
	// record offset
	longbuf[0] = (record_offset>>24)&0xFF;
	longbuf[1] = (record_offset>>16)&0xFF;
	longbuf[2] = (record_offset>>8)&0xFF;
	longbuf[3] = (record_offset)&0xFF;
	fwrite(longbuf, sizeof(char), 4, dawg);
	record_offset += NodesPerRecord*nodesize;

	// attributes and ID

	memset(longbuf, 0, 4);
	fwrite(longbuf, sizeof(char), 4, dawg);
    }

    // now write out the nodes

    longbuf[0] = (nodesize)&0xFF;
    longbuf[1] = progid; //0=demo; 1=lite;2=++;3=pro
    fwrite(longbuf, sizeof(char), nodesize, dawg);

    unsigned long n, nn = 1;

    for (n = 0; n <= topnode; n++)
    {
        if (index[n]==0) continue;
        int last = node[n].LastChild();
        for (int j=0; j<=last; j++)
        {
            int ch = node[n].children[j];
            if (ch)
            {
    	        unsigned  long v = (unsigned long)index[ch];
    	        if (node[ch].IsTerminal())
    	            v |= (1ul << TVSH);
    	        if (j==last)
    	            v |= (1ul << PVSH);
    	        v |= ((unsigned long)(j+IDLT))<<IVSH;
		longbuf[0] = (v>>24)&0xFF;
		if (nodesize == 3)
		{
		    longbuf[0] |= (v>>16)&0x01;
		    longbuf[1] = (v>>8)&0xFF;
		    longbuf[2] = (v)&0xFF;
		    if (debug)
			printf("%ld : %c%c '%c' %08lx [%02x %02x %02x] %d\n",
				nn++,
				(node[ch].IsTerminal()?'T':' '),
				((j==last)?'L':' '),
				(j+'A'),
				v, 
				longbuf[0],
				longbuf[1],
				longbuf[2],
				index[ch]);
		}
		else
		{
		    longbuf[1] = (v>>16)&0xFF;
		    longbuf[2] = (v>>8)&0xFF;
		    longbuf[3] = (v)&0xFF;
		    if (debug)
			printf("%ld : %c%c '%c' %08lx [%02x %02x %02x %02x]\n",
				nn++,
				(node[ch].IsTerminal()?'T':' '),
				((j==last)?'L':' '),
				(j+'A'),
				v, 
				longbuf[0],
				longbuf[1],
				longbuf[2],
				longbuf[3]);
		}
		fwrite(longbuf, sizeof(char), nodesize, dawg);
	    }
	}
    }
#if 0
    // pad the last record
    memset(longbuf, 0, 4);
    n = nnodes;
    while ((++n)%NodesPerRecord)
    {
	if (debug) printf("%ld PAD\n", n);
	fwrite(longbuf, sizeof(char), nodesize, dawg);
    }
#endif
}

void NodePool::AddWord(char *word, int pos)
{
    int n = nodestack[pos];
    if (debug)
        printf("%d: %-16s Free %d\n", ++wcnt, word, freenodes);
    else
        printf("%d: %-16s\n", ++wcnt, word);
    int l = strlen(word);
    while (word[pos])
    {
        int j = word[pos] - 'A';
        int nd = AllocateNode(j);
	node[n].SetChild(j, nd);
	node[n].NoteDepth(l - pos);
        nodestack[++pos] = n = nd;
    }
    node[n].SetTerminal();
    stacktop = pos;
}

void NodePool::FreeNode(int n)
{
    node[n].ChainFree(botnode);
    botnode = n;
    if (topnode == n)
        while (topnode > 0 && node[--topnode].IsFree());
    freenodes++;
}

int NodePool::MergeNodes(int first, int last, int n, int parent)
{
    for (int i=first; i<=last; i++)
    {
        if (NodeCompare(i, n))
        {
    	    int chidx = node[n].Index();
    	    node[parent].children[chidx] = i;
    	    FreeNode(n);
    	    return 1;
        }
    }
    return 0; /* no match */
}

void NodePool::Merge(int pos)
{
    int i = stacktop;
    while (i>pos)
    {
	MergeNodes(i);
	i--;
    }
}

static void useage(void)
{
    fprintf(stderr,"Usage: dictmgr [-S] [-n <name>] [-m <minlen>] [-M <maxlen>] [-o <OPDB>] [<IWL> ...] (build dictionary)\n");
    fprintf(stderr,"or:    dictmgr -d [-o <OWL>] <IPDB>  (dump dictionary)\n");
    fprintf(stderr,"or:    dictmgr -? <pattern> [-t All|Prefix|Suffix|RegExp|GlobPat] [-l <minlen>] [-L <maxlen>] [-Multi] [-W <maxwords>] [-w <minwords>] [<PDB>] (search)\n");
    fprintf(stderr,"\n\nIPDB and OPDB represent input and output .pdb dictionary databases.\n");
    fprintf(stderr,"IWL and OWL represent input and output word list files.\n");
    fprintf(stderr,"\n\nIf no word list file is specified for building, standard input is used.\n");
    fprintf(stderr,"If no output file is specified for building, pw.pdb is used.\n");
    fprintf(stderr,"If no output file is specified for listing, standard output is used.\n");
    fprintf(stderr,"Input word lists must be free of punctuation.\n");
    fprintf(stderr,"If they are already sorted, use -S to speed up the loading of the words.\n");
    fprintf(stderr,"Multiple word lists can be specified when building, and they will be\n");
    fprintf(stderr,"merged. The -m and -M flags allow you to select which subset of words\n");
    fprintf(stderr,"should be included based on word length restrictions.\n");
    fprintf(stderr,"At most 16 input word lists can be merged into one\n");
    exit(0);
}

void DumpHeaders(FILE *dawg, FILE *ofp)
{
    char nbuf[32];
    unsigned char shortbuf[2], longbuf[4];
    //frewind(fp);
    fseek(dawg, 0l, SEEK_SET);
    fread(nbuf, sizeof(char), 32, dawg);
    fprintf(ofp, "Name: %.32s\n", nbuf);
    // Attributes
    fread(shortbuf, sizeof(char), 2, dawg);
    fprintf(ofp, "Attributes: %d\n", shortbuf[0]*256+shortbuf[1]);
    // version
    fread(shortbuf, sizeof(char), 2, dawg);
    fprintf(ofp, "Version: %d\n", shortbuf[0]*256+shortbuf[1]);
    // creation and modification date
    fread(longbuf, sizeof(char), 4, dawg);
    fread(longbuf, sizeof(char), 4, dawg);
    // last backup date
    fread(longbuf, sizeof(char), 4, dawg);
    // modification number
    fread(longbuf, sizeof(char), 4, dawg);
    fprintf(ofp, "Modification number: %lu\n",
		(longbuf[0]<<24)|(longbuf[1]<<16)|(longbuf[2]<<8)|longbuf[3]);
    // appinfo area
    fread(longbuf, sizeof(char), 4, dawg);
    // sortinfo area
    fread(longbuf, sizeof(char), 4, dawg);
    // database type
    fread(longbuf, sizeof(char), 4, dawg);
    fprintf(ofp, "Type: %.4s\n", longbuf);
    // creator ID
    fread(longbuf, sizeof(char), 4, dawg);
    fprintf(ofp, "Creator ID: %.4s\n", longbuf);
    // unique ID seed
    fread(longbuf, sizeof(char), 4, dawg);
    // last record list
    fread(longbuf, sizeof(char), 4, dawg);
    // number of records
    fread(shortbuf, sizeof(char), 2, dawg);
    fprintf(ofp, "NumRecs: %d\n", shortbuf[0]*256+shortbuf[1]);
}

void Dump(int dumplevel, const char *ofname, const char *fname = 0)
{
    FILE *fp = 0;
    if (ofname) fp = fopen(ofname, "w");
    DictionaryDatabase dict(fname);
//    dict.Init(fname);
    dict.InitDB();
    if (dumplevel>1)
    {
	DumpHeaders(dict.FP(), (fp ? fp : stdout));
        dict.RawDump(fp ? fp : stdout);
    }
    else
        dict.Dump(fp ? fp : stdout);
    if (fp) fclose(fp);
    if (gncnt && debug)
        printf("GetNode %lu  Cache Hits: %02d%%\n", gncnt, ((gncnt-dbcnt)*100ul)/gncnt);
}

void Consult(char *pattern, 
			int type_in,
			int multi_in = 0,
			int minlen_in = 0,
			int maxlen_in = 0,
			int mincnt_in = 0,
			int maxcnt_in = 0,
			const char *ofname = 0,
			const char *fname = 0)
{
    DictionaryDatabase dict(fname);
    if (dict.InitDB() == 0)
    {
        const char *err = dict.StartConsult(pattern, type_in, multi_in, minlen_in, maxlen_in, mincnt_in, maxcnt_in);
	if (err == 0)
        {
            FILE *fp = ofname ? fopen(ofname, "w") : 0;
            char line[80];
            while (dict.NextMatch(line, sizeof(line)) >= 0)
	    {
                if (fp) fprintf(fp, "%s\n", line);
	        else puts(line);
	    }
            if (fp) fclose(fp);
	}
	else fprintf(stderr, "Invalid search pattern %s: %s\n", pattern, err);
    }
    else fprintf(stderr, "Cannot initialise dictionary %s\n", fname);
}

// Get the next line in alpha order from a set of open files
//
// CHANGE! Add a separate routine that given a single FP, filename,
//	last word, and category set, will read the next word that is
//	in the category set and comes after the last word (or is the
//	same as the last word). If it reaches eof it should fclose
//	and return -1. Then build GetLine around this.
//      This must also be able to build merged word lists and
//	merge the categories, which may change things.

static int first = 1;
static char wordbuffers[16][MAXLINELEN] = { { 0 } };
static int wordlines[16] = { 0 };
int load_from_disk = 0, must_sort = 0;

void ResetFiles(char **filename, FILE **lex)
{
    first = 1;
    for (int i = 0; i < 16; i++)
    {
       if (lex[i])
           rewind(lex[i]);
	else if (filename[i])
	    lex[i] = fopen(filename[i], "r");
	wordbuffers[i][0] = 0;
	wordlines[i] = 0;
    }
}

int GetLine(char *wordbuf, FILE **lex, char **filename, const char *categories,
			int minlength, int maxlength)
{
    (void)categories; // not implemented yet
    int best, skip, rtn = -1;
    if (first)
    {
        for (int i = 0; i < 16; i++)
	{
	    if (lex[i] == 0)
	        wordbuffers[i][0] = 0;
	    else if (feof(lex[i]) || fgets(wordbuffers[i], MAXLINELEN, lex[i])==0)
	    {
	        wordbuffers[i][0] = 0;
	        fclose(lex[i]);
	        lex[i] = 0;
	    }
	    else
	    {
		++wordlines[i];
		StrUpr(wordbuffers[i]);
	    }
	}
	first = 0;
    }
//wordbuf[0] = 0;
  retry:
    best = skip = 0;
    while (wordbuffers[best][0]==0) ++best;
    if (best >= 16)
    {
      wordbuf[0] = 0;
	return -1;
    }
    for (int i = best+1; i < 16; i++)
    {    
//if (wordbuffers[i][0])
    //printf("\t%s\n", wordbuffers[i]);
        if (wordbuffers[i][0] && strcmp(wordbuffers[best], wordbuffers[i])>0)
	    best = i;
    }
#ifdef CATEGORIES
    char *cat = wordbuffers[best];
    while (*cat && isalpha(*cat)) ++cat;
    int l = cat-wordbuffers[best], x = 0;
    if (isspace(*cat))
    {
        while (isspace(*cat)) ++cat;
    }
    else cat = 0;
#else
    int l = strlen(wordbuffers[best]), x = 0;
#endif
    while (l>0 && (wordbuffers[best][l-1]<'A' || wordbuffers[best][l-1]>'Z'))
        --l;
    wordbuffers[best][l] = 0;
    if (l) x = strcmp(wordbuf, wordbuffers[best]);
    if (x == 0) skip = 1; // duplicate or empty
    else if (x > 0)
    {
        if (load_from_disk)
	{
            printf("Skipping out-of-order word `%s' in file %s, line %d\n",
      		wordbuffers[best], filename[best], wordlines[best]);
	    skip = 1;
	}
	else
	{
	    must_sort = 1;
            //strcpy(wordbuf, wordbuffers[best]);
	}
    }
    //else strcpy(wordbuf, wordbuffers[best]);
    if (!skip) strcpy(wordbuf, wordbuffers[best]);
    if (feof(lex[best]) || fgets(wordbuffers[best], MAXLINELEN, lex[best])==0)
    {
	wordbuffers[best][0] = 0;
	fclose(lex[best]);
	lex[best] = 0;
	if (wordbuf[0])
	    rtn = strlen(wordbuf); /* rtn=length */
    }
    else
    {
	++wordlines[best];
        StrUpr(wordbuffers[best]);
	rtn = strlen(wordbuf); /* rtn=length */
	if (skip) ; 
	else if (rtn > maxlength)
	{
	    fprintf(stderr, "Word %s (file %s line %d) is too long (>%d); skipping!\n",
			wordbuf, filename[best], wordlines[best], maxlength);
	    skip = 1;
	}
	else if (rtn < 0 || rtn<minlength || isspace(wordbuf[0]))
	{
	    if (rtn) fprintf(stderr,"Word %s (file %s line %d ) is too short (<%d); skipping\n",
			wordbuf, filename[best], wordlines[best], minlength);
	    skip = 1;
	}
    }
    if (skip) goto retry;
    return rtn;
}

void CheckSort(const char *fname)
{
    FILE *fp = fopen(fname, "r");
    if (fp)
    {
        char buff[80], last[80];
	last[0]=0;
	long lnum = 0;
        while (!feof(fp))
	{
	    if (fgets(buff, sizeof(buff), fp) == 0) break;
	    ++lnum;
	    StrUpr(buff);
	    buff[strlen(buff)-1] = 0;
	    if (buff[0] && strcmp(buff, last)<0)
	        printf("Line %ld %s\n", lnum, buff);
	    strcpy(last, buff);
	}
        fclose(fp);
    }
}

inline void swap(char **tbl, unsigned long i, unsigned long j)
{
    char *tmp = tbl[i];
    tbl[i] = tbl[j];
    tbl[j] = tmp;
}

void quicksort(char **tbl, unsigned long l, unsigned long r)
{
    unsigned long i, j;
    if (r > l)
    {
        char *tmp = tbl[r], *tmp2; i = l-1; j = r;
	for (;;)
	{
	    while (strcmp(tbl[++i], tmp) < 0);
	    while (strcmp(tbl[--j], tmp) > 0);
	    if (i >= j) break;
	    swap(tbl, i, j);
	}
	swap(tbl, i, r);
	quicksort(tbl, l, i-1);
	quicksort(tbl, i+1, r);
    }
}

int SetProgID(FILE *fp, int progID)
{
    if (fseek(fp, 76, SEEK_SET) == 0)
    {
        unsigned char v[4];
        if (fread(v, 2, 1, fp)>0)
	{
	    if (fseek(fp, 78 + 8 * ((v[0]<<8)|v[1]), SEEK_SET)==0)
	    {
	        if (fread(v, 4, 1, fp)>0)
		{
		    v[1] = progID;
		    if (fseek(fp, -4l, SEEK_CUR) == 0)
		        if (fwrite(v, 4, 1, fp) > 0)
			    return 0;
		}
	    }
	}
    }
    return -1;
}

int SetProgID(const char *fname, int progID)
{
    FILE *fp = fopen(fname, "r+b");
    if (fp)
    {
        int rtn = SetProgID(fp, progID);
        fclose(fp);
	return rtn;
    }
    return -1;
}

int ShowProgID(FILE *fp)
{
    if (fseek(fp, 76, SEEK_SET) == 0)
    {
        unsigned char v[4];
        if (fread(v, 2, 1, fp)>0)
	{
	    unsigned long off = 78 + 8 * ((v[0]<<8)|v[1]);
	    if (fseek(fp, off, SEEK_SET)==0)
	    {
	        if (fread(v, 4, 1, fp)>0)
		{
		    printf("Offset %lu Nodesize %d\n", off, (int)v[0]);
		    printf("Offset %lu ID %d\n", off+1, (int)v[1]);
		    printf("Old n3size %d\n", (int)v[2]);
		    printf("Old n4size %d\n", (int)v[3]);
		}
	    }
	}
    }
    return -1;
}

int ShowProgID(const char *fname)
{
    FILE *fp = fopen(fname, "rb");
    if (fp)
    {
        int rtn = ShowProgID(fp);
        fclose(fp);
	return rtn;
    }
    return -1;
}

int main(int argc, char **argv)
{
    FILE *lex[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    char *filename[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	char *categories = 0;
    const char *name = 0;

    unsigned short l;
    int pos, n;
    char word[80], lastword[80], *dname = 0, *pattern = 0;
    int doraw = 0, dump = 0, progid = 0, type=USEALL;
    int minlength = 1, maxlength = 32, mincnt = 0, maxcnt = 0, multi = 0;
    time_t start_time = time(0);

    fprintf(stderr, "Welcome to DictMgr for WordFindPro!\n");
    fprintf(stderr, "\nUse \"dictmgr -h\" for usage instructions\n\n");

    for (int i=1; i<argc; i++)
    {
	switch(argv[i][0])
	{
	case '-':
	    switch(argv[i][1])
	    {
		case 'C':
		    categories = argv[++i];
		    if (i>=argc) useage();
		    break;
		case 'n':
		    name = argv[++i];
		    if (i>=argc) useage();
		    break;
		case 'o':
		    dname = argv[++i];
		    if (i>=argc) useage();
		    break;
		case '?':
		    pattern = argv[++i];
		    if (i>=argc) useage();
		    break;
		case 'h':
		case 'H':
		    useage();
		    break;
		case 'd':
		    ++dump;
		    break;
		case 'l':
		    minlength = atoi(argv[++i]);
		    if (i>=argc) useage();
		    break;
		case 'L':
		    maxlength = atoi(argv[++i]);
		    if (i>=argc) useage();
		    break;
		case 'M':
		    multi = 1;
		    break;
		case 'r':
		    if (dname == 0) dname = "pw.dic";
		    doraw = 1;
		    break;
		case 'D':
		    ++debug;
		    break;
		case 'F':
		    progid = atoi(argv[++i]);
		    if (i>=argc) useage();
		    break;
		case 'S': 
		    load_from_disk = 1;
		    break;
		case 'P': 
		    progid = atoi(argv[++i]);
		    if (i>=argc) useage();
		    dname = argv[++i];
		    if (i>=argc) useage();
		    SetProgID(dname, progid);
		    exit(0);
		case 'I': 
		    dname = argv[++i];
		    if (i>=argc) useage();
		    ShowProgID(dname);
		    exit(0);
		case 'w':
		    mincnt = atoi(argv[++i]);
		    if (i>=argc) useage();
		    break;
		case 'W':
		    maxcnt = atoi(argv[++i]);
		    if (i>=argc) useage();
		    break;
		case 't':
		    if (++i>=argc) useage();
		    if (argv[i][0]=='p' || argv[i][0]=='P')
		        type = PREFIX;
		    else if (argv[i][0]=='s' || argv[i][0]=='S')
		        type = SUFFIX;
		    else if (argv[i][0]=='m' || argv[i][0]=='M')
		        type = MULTI;
		    else if (argv[i][0]=='a' || argv[i][0]=='A')
		        type = USEALL;
		    else if (argv[i][0]=='r' || argv[i][0]=='R')
		        type = REGEXP;
		    else if (argv[i][0]=='g' || argv[i][0]=='G')
		        type = GLOB;
		    break;
		default:
		    useage();
	    }
	    break;
	default:
	    if (dump)
	        Dump(dump, dname, argv[i]);
	    else if (pattern)
		Consult(pattern, type, multi, minlength, maxlength,
				mincnt, maxcnt, dname, argv[i]);
	    else for (int x = 0; x < 16; x++)
	    {
 		if (lex[x] == 0)
		{
	            if ((lex[x] = fopen(argv[i], "r")) == 0)
		    {
		        fprintf(stderr,"Cannot open input file %s\n",
					argv[i]);
		        exit(-1);
		    }
		    filename[x] = argv[i];
		    break;
		}
	    }
	    break;
	}
    }
    if (dump || pattern)
    {
        fprintf(stderr, "Time: %d secs\n", time(0)-start_time);
        exit(0);
    }
    if (lex[0] == 0) lex[0] = stdin;
    if (dname == 0) dname = "pw.pdb";
    FILE *dawg = fopen(dname,"wb");
    if (dawg==0)
    {
	fprintf(stderr,"Cannot open output file %s!\n",dname);
	exit(-2);
    }
    fprintf(stderr, "Initialising...\n");
    NodePool *p = new NodePool;

    /* clear the stack containing last word */

    lastword[0] = word[0] = 0;
    //for (i=0;i<=NodePool::MAXLEN;i++) nodeStk[i]=0;
    
    if (maxlength >= MAXLEN) maxlength = MAXLEN-1;

    if (load_from_disk)
    {
        for (;;)
        {
            /*int l = */GetLine(word, lex, filename, categories, minlength, maxlength);
	    if (word[0] == 0) break;

	    /* Find the point of departure from last word */

	    pos = cmpstr(word,lastword);
	    strcpy(lastword,word);

	    /* `bottom-out' of tree to point of departure, merging
			any completed subtrees */

	    p->Merge(pos);
	    p->AddWord(word, pos);
#ifdef DEBUG
	    if (debug>2) p->DumpGraph(stdout);
#endif
	}
    }
    else
    {
        /* count the number of words */

        long wcnt = 0;
        for (;;)
        {
            int l = GetLine(word, lex, filename, categories, minlength, maxlength);
	    if (l <= 0) break;
	    ++wcnt;
        }
        printf("Found %ld words\n", wcnt);

        /* allocate space */

        char **words = new char*[wcnt];
        if (words == 0)
        {
            fprintf(stderr, "Allocation failure\n");
	    exit(1);
        }

        /* load the words */

        ResetFiles(filename, lex);
        word[0] = 0;
        long wrd;
        for (wrd = 0; wrd<wcnt;wrd++)
        {
            int l = GetLine(word, lex, filename, categories, minlength, maxlength);
	    if (l <= 0) break;
	    words[wrd] = new char[l+1];
	    if (words[wrd]==0)
	    {
                fprintf(stderr, "Allocation failure\n");
	        exit(1);
	    }
	    strncpy(words[wrd], word, l);
	    words[wrd][l]=0;
        }
        printf("Loaded %ld words\n", wcnt = wrd);

	if (must_sort)
	{
            printf("Sorting...");
            fflush(stdout);
	    quicksort(words, 0, wcnt-1);
            printf("Done\n");
	}
	else printf("Input already sorted; excellent!\n");

        /* add words */

        for (wrd = 0; wrd < wcnt; wrd++)
        {
            if (wrd && strcmp(words[wrd], /*words[wrd-1]*/word) == 0)
	    {
		  delete []words[wrd];
	        continue; // dup
	    }

            strcpy(word, words[wrd]);
	    delete [] words[wrd];

	    /* Find the point of departure from last word */

	    pos = cmpstr(word,lastword);
	    strcpy(lastword,word);

	    /* `bottom-out' of tree to point of departure, merging
			any completed subtrees */

	    p->Merge(pos);
	    p->AddWord(word, pos);
#ifdef DEBUG
	    if (debug>2) p->DumpGraph(stdout);
#endif
        }
        delete [] words;
    }
    for (int x = 0; x < 16; x++)
    {
        if (lex[x] && lex[x] != stdin)
	    fclose(lex[x]);
	lex[x] = 0;
    }

    p->Merge(); /* Make sure we've bottomed out... */
//p->PrintGraph(stdout);
    //p->MarkGraph();
#ifdef DEBUG
    if (debug>1) p->DumpGraph(stdout);
#endif
    if (debug) p->CheckOptimality();
    /* We now have to convert the DAWG into an array of edges
    // in a file.  */
    unsigned long nn = p->IndexNodes();
    if (doraw) p->WriteDawg(dawg);
    else p->WritePalmDB(dawg, nn, name, filename[0], progid); //((filename[1]==0)?filename[0]:0));
    fclose(dawg);
#ifdef DEBUG
    if (debug) p->DumpGraph(stdout);
#endif
    p->ShowStats();
    fprintf(stderr, "Time: %d secs\n", time(0)-start_time);
    delete p;
    return 0;
}

