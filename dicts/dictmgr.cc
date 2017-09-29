// Same as old way, but converted to C++

#define OLD_GETNODE // for now

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>

//int debug = 0;

#define DUMP
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
    unsigned long self;
    int children[26];	// indices of child nodes
  public:
    inline void Clear(int chidx = 0)
    {
	self = chidx;
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
	return (self & FREE) != 0;
    }
    inline int IsUsed() const
    {
	return (self & FREE) == 0;
    }
    inline void SetFree()
    {
	self |= FREE;
    }
    inline void SetUsed()
    {
	self &= ~FREE;
    }
    inline int IsTerminal() const
    {
	return (self & TERMINAL) != 0;
    }
    inline void SetTerminal()
    {
	self |= TERMINAL;
    }
    inline int Value() const
    {
	return (self & (~(FREE|TERMINAL)));
    }
    //inline void SetValue(unsigned v)
    //{
	//self = (self&(FREE|TERMINAL)) | (v & (~(FREE|TERMINAL)));
    //}
    inline void ChainFree(unsigned nextfree)
    {
	self = FREE|nextfree;
    }
    void Print(FILE *fp);
    int LastChild() const;
    int NumChildren() const;
};

void Node::Print(FILE *fp)
{
    if (IsTerminal()) fprintf(fp, "Terminal ");
    fprintf(fp, "Node\n");
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
#else
#define MAXNODES	250000
#define MAXLEN		30
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
    void WriteDawg(FILE *fp);
    void WritePalmDB(FILE *fp, unsigned long nnodes,
    			const char *name = 0,
			const char *srcname = 0);
    void AddWord(char *word, int pos);
    void FreeNode(int n);
    int MergeNodes(int first, int last, int n, int parent);
    inline unsigned int NodeCompare(int n1, int n2) const
    {
	return (node[n1].self == node[n2].self &&
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
    void SaveState(char *word, char *lastword);
    int RestoreState(char *word, char *lastword);

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
    botnode = node[i].Value();
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
				const char *srcname)
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

    longbuf[0] = 0;
    longbuf[1] = 0;
    longbuf[2] = 0;
    longbuf[3] = (nodesize)&0xFF;
    fwrite(longbuf+4-nodesize, sizeof(char), nodesize, dawg);

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
    	            v |= 0x40000000l;
    	        if (j==last)
    	            v |= 0x80000000l;
    	        v |= ((unsigned long)j)<<25;
		longbuf[0] = (v>>24)&0xFF;
		if (nnodes < (128l*1024l))
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
#if 1
#endif
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
    while (word[pos])
    {
        int j = word[pos] - 'A';
        nodestack[++pos] = n = (node[n].children[j] = AllocateNode(j));
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
    	    int chidx = node[n].Value();
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
    while (i>pos) MergeNodes(i--);
}

void NodePool::SaveState(char *word, char *lastword)
{
    int i;
    FILE *stf = fopen("buildict.tmp", "w");
    if (stf == 0)
    {
        fprintf(stderr, "Can't open state file\n");
        return;
    }
    fwrite(&botnode, sizeof(botnode), 1, stf);
    fwrite(&topnode, sizeof(topnode), 1, stf);
    fwrite(&freenodes, sizeof(freenodes), 1, stf);
    fwrite(&usednodes, sizeof(usednodes), 1, stf);
    fwrite(word, sizeof(char), MAXLEN+1, stf);
    fwrite(lastword, sizeof(char), MAXLEN+1, stf);
    fwrite(&wcnt, sizeof(wcnt), 1, stf);
    fwrite(&stacktop, sizeof(stacktop), 1, stf);
    fwrite(nodestack, sizeof(int), stacktop+1, stf);
    for (i = 0; i <= topnode; i++)
        fwrite(node+i, sizeof(Node), 1, stf);
    fwrite(index, sizeof(int), topnode+1, stf);
    fclose(stf);
    fprintf(stderr, "Saved state; resume with the -R flag\n");
}

int NodePool::RestoreState(char *word, char *lastword)
{
    int i;
    FILE *stf = fopen("buildict.tmp", "r");
    fprintf(stderr, "Restoring state...\n");
    if (stf == 0)
    {
        fprintf(stderr, "Can't open state file\n");
        exit(-1);
    }
    fread(&botnode, sizeof(botnode), 1, stf);
    fread(&topnode, sizeof(topnode), 1, stf);
    fread(&freenodes, sizeof(freenodes), 1, stf);
    fread(&usednodes, sizeof(usednodes), 1, stf);
    fread(word, sizeof(char), MAXLEN+1, stf);
    fread(lastword, sizeof(char), MAXLEN+1, stf);
    fread(&wcnt, sizeof(wcnt), 1, stf);
    fread(&stacktop, sizeof(stacktop), 1, stf);
    fprintf(stderr, "Bot %d Top %d Free %d Used %d Cnt %d Stk %d Wrd %s\n",
    	botnode, topnode, freenodes, usednodes,
    	wcnt, stacktop, lastword);
    fread(nodestack, sizeof(int), stacktop+1, stf);
    for (i = 0; i <= topnode; i++)
        fread(node+i, sizeof(Node), 1, stf);
    fread(index, sizeof(int), topnode+1, stf);
    fclose(stf);
    return wcnt;
}

static void useage(void)
{
    fprintf(stderr,"Usage: dictmgr [-n <name>] [-m <minlen>] [-M <maxlen>] [-o <OPDB>] [<IWL> ...] (build dictionary)\n");
    fprintf(stderr,"or:    dictmgr -l [-o <OWL>] <IPDB>  (list dictionary)\n");
//    fprintf(stderr,"or:    dictmgr -R <EmailAddress>] <HotSyncName>  (register programs)\n");
    fprintf(stderr,"\n\nIPDB and OPDB represent input and output .pdb dictionary databases.\n");
    fprintf(stderr,"IWL and OWL represent input and output word list files.\n");
    fprintf(stderr,"\n\nIf no word list file is specified for building, standard input is used.\n");
    fprintf(stderr,"If no output file is specified for building, pw.pdb is used.\n");
    fprintf(stderr,"If no output file is specified for listing, standard output is used.\n");
    fprintf(stderr,"Input word lists must be sorted and be free of punctuation.\n");
    fprintf(stderr,"Multiple word lists can be specified when building, and they will be\n");
    fprintf(stderr,"merged. The -m and -M flags allow you to select which subset of words\n");
    fprintf(stderr,"should be included based on word length restrictions.\n");
    fprintf(stderr,"At most 16 input word lists can be merged into one\n");
//    fprintf(stderr,"\nWhen registering, you will be prompted for a key. A pwreg.pdb file\n");
//    fprintf(stderr,"will be created, which you should install on your Palm device.\n");
//    fprintf(stderr,"Warning: invalid pwreg.pdb files will cause PalmWord++ to crash!\n");
    exit(0);
}

static void HandleSignal(int);
typedef void (*handler)(int);

int must_abort = 0;

void Dump(int dumplevel, const char *ofname, const char *fname = 0)
{
    FILE *fp = 0;
    if (ofname) fp = fopen(ofname, "w");
    DictionaryDatabase dict;
    dict.Init(fname);
    dict.InitDB();
    if (dumplevel>1)
        dict.RawDump(fp ? fp : stdout);
    else
        dict.Dump(fp ? fp : stdout);
    if (fp) fclose(fp);
    if (gncnt && debug)
        printf("GetNode %lu  Cache Hits: %02d%%\n", gncnt, ((gncnt-dbcnt)*100ul)/gncnt);
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

int GetLine(char *wordbuf, FILE **lex, char **filename, const char *categories)
{
    (void)categories; // not implemented yet
    int best, dup;
    static int first = 1;
    static char wordbuffers[16][MAXLEN] = { { 0 } };
    if (first)
    {
        for (int i = 0; i < 16; i++)
	{
	    if (lex[i] == 0)
	        wordbuffers[i][0] = 0;
	    else if (feof(lex[i]) || fgets(wordbuffers[i], MAXLEN, lex[i])==0)
	    {
	        wordbuffers[i][0] = 0;
	        fclose(lex[i]);
	        lex[i] = 0;
	    }
	    else StrUpr(wordbuffers[i]);
	}
	first = 0;
    }
  retry:
    best = dup = 0;
    while (wordbuffers[best][0]==0) ++best;
    if (best >= 16) return -1;
//printf("Choosing from:\n\t%s\n", wordbuffers[best]);
    for (int i = best+1; i < 16; i++)
    {    
//if (wordbuffers[i][0])
    //printf("\t%s\n", wordbuffers[i]);
        if (wordbuffers[i][0] && strcmp(wordbuffers[best], wordbuffers[i])>0)
	    best = i;
    }
//printf("Best: %s\n\n", wordbuffers[best]);
#ifdef CATEGORIES
    char *cat = wordbuffers[best];
    while (*cat && isalpha(*cat)) ++cat;
    int l = cat-wordbuffers[best]-1;
    if (isspace(*cat))
    {
        while (isspace(*cat)) ++cat;
    }
    else cat = 0;
#else
    int l = strlen(wordbuffers[best])-1;
#endif
    while (l>0 && (wordbuffers[best][l]<'A' || wordbuffers[best][l]>'Z'))
        l--;
    wordbuffers[best][l+1] = 0;
    int x = strcmp(wordbuf, wordbuffers[best]);
    if (x == 0) dup = 1;
    else if (x > 0)
    {
        printf("Skipping out-of-order word %s in file %s\n",
		wordbuffers[best], filename[best]);
	dup = 1;
    }
    else strcpy(wordbuf, wordbuffers[best]);
    if (feof(lex[best]) || fgets(wordbuffers[best], MAXLEN, lex[best])==0)
    {
	wordbuffers[best][0] = 0;
	fclose(lex[best]);
	lex[best] = 0;
    }
    if (dup) goto retry;
    return 0;
}

int main(int argc, char **argv)
{
    FILE *lex[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    char *filename[16], *categories;
    const char *name = 0;

    unsigned short l;
    int pos, n;
    char word[80], lastword[80], *dname = 0;
    int restore = 0;
    int doraw = 0, dump = 0;
    int minlength = 1, maxlength = 32;

    fprintf(stderr, "Welcome to DictMgr for PalmWord++ Pro!\n");
    fprintf(stderr, "\nUse \"dictmgr -?\" for usage instructions\n\n");

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
		    useage();
		    break;
		case 'l':
		    ++dump;
		    break;
		case 'm':
		    minlength = atoi(argv[++i]);
		    if (i>=argc) useage();
		    break;
		case 'M':
		    maxlength = atoi(argv[++i]);
		    if (i>=argc) useage();
		    break;
		case 'r':
		    if (dname == 0) dname = "pw.dic";
		    doraw = 1;
		    break;
		case 'R':
		    restore = 1;
		    break;
		case 'D':
		    ++debug;
		    break;
		default:
		    useage();
	    }
	    break;
	default:
	    if (dump)
	        Dump(dump, dname, argv[i]);
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
    if (dump) exit(0);
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

    /* add words */

    if (restore && lex[1]==0)
    {
	int wcnt = p->RestoreState(word, lastword);
	/* Now skip forward in lex file to next word */
	for (int i = wcnt; i--; )
	    //fgets(word, NodePool::MAXLEN, lex);
	    fgets(word, MAXLEN, lex[0]);
	fprintf(stderr, "Resuming at %s\n", word);
    }
    if (maxlength >= MAXLEN) maxlength = MAXLEN-1;

    (void)signal(SIGINT, (handler)HandleSignal);
#ifdef UNIX
    (void)signal(SIGQUIT, (handler)HandleSignal);
#endif

    while (!must_abort)
    {
        if (GetLine(word, lex, filename, categories) < 0)
	    break;
	int l = strlen(word)-1;	/* l now indexes the last letter */

	if (l >= maxlength)
	{
	    fprintf(stderr, "Word %s is too long; skipping!\n", word);
	    continue;
	}
	else if (l<(minlength-1))
	{
	    if (l) fprintf(stderr,"Word %s is too short; skipping\n",word);
	    continue;
	}

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
    if (must_abort)
    {
	p->SaveState(word, lastword);
	delete p;
	exit(0);
    }
    for (int x = 0; x < 16; x++)
    {
        if (lex[x] && lex[x] != stdin)
	    fclose(lex[x]);
	lex[x] = 0;
    }

    p->Merge(); /* Make sure we've bottomed out... */
    //p->MarkGraph();
#ifdef DEBUG
    if (debug>1) p->DumpGraph(stdout);
#endif
    /* We now have to convert the DAWG into an array of edges
    // in a file.  */
    unsigned long nn = p->IndexNodes();
    if (doraw) p->WriteDawg(dawg);
    else p->WritePalmDB(dawg, nn, name, filename[0]); //((filename[1]==0)?filename[0]:0));
    fclose(dawg);
#ifdef DEBUG
    if (debug) p->DumpGraph(stdout);
#endif
    p->ShowStats();
    delete p;
    return 0;
}

static void HandleSignal(int signo)
{
    (void)signo;
    if (++must_abort>3) signal(SIGINT, SIG_DFL);
    fprintf(stderr, "Interrupted!\n");
}

