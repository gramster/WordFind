// Same as old way, but converted to C++

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <ctype.h>

void strupr(char *s)
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
    const unsigned long FREE = 0x80000000u;
    const unsigned long TERMINAL = 0x40000000u;
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
    void Print(FILE *fp)
    {
        if (IsTerminal()) fprintf(fp, "Terminal ");
	fprintf(fp, "Node\n");
	for (int j=0; j<26; j++)
	{
	    if (children[j])
		fprintf(fp, "   Child %d (%c)\n", children[j], (char)(j+'A'));
	}
    }
    int LastChild() const
    {
	for (int j=26; --j >= 0; )
	{
	    if (children[j]) return j;
	}
	return -1;
    }
    int NumChildren() const
    {
	int chld = 0;
	for (int j=0; j<26; j++)
	    if (children[j])
		chld++;
 	return chld;
    }
};

//----------------------------------------------------------------------

class NodePool
{
  public:
    const int MAXNODES = 250000;	/* max nodes in pool	*/
    const int MAXLEN = 30;		/* max word length	*/
  private:
    Node node[MAXNODES];
    int index[MAXNODES];
    int botnode, topnode, freenodes, usednodes;
    int wcnt;

    char word[MAXLEN+1],lastword[MAXLEN+1];
    int	nodestack[MAXLEN];
    int stacktop;

  public:
    void Init() // was Setup
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
    NodePool()
    {
	Init();
    }
    int AllocateNode(int chidx)
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
    void MarkGraph(int n)
    {
	if (node[n].IsFree())
	{
	    node[n].SetUsed();
	    usednodes++;
	    int *start = node[n].children, *end = node[n].children+26;
	    while (start < end)
	    {
	        if (*start) MarkGraph(*start);
		start++;
	    }
	}
    }
    void MarkGraph()
    {
	for (int n=0; n<=topnode; n++)
	    node[n].SetFree();
	usednodes = 0;
	MarkGraph(0);
    }
    void DumpGraph(FILE *fp, int n, int pos, int ch)
    {
	fprintf(fp, "%6d%*c%c\n", n, pos, ' ',
			(node[n].IsTerminal() ? toupper(ch) : ch));
	for (int j=0; j<26; j++)
	    if (node[n].children[j])
		DumpGraph(fp, node[n].children[j], pos+2, j+'a');
    }
    inline void DumpGraph(FILE *fp)
    {
	DumpGraph(fp, 0, 0, ' ');
    }
    void PrintGraph(FILE *fp)
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
    void WriteNode(FILE *fp, int n)
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
		printf("  Writing edge for %c (%08lX)\n",(char)(j+'A'-1),v);
#endif
	    }
	}
    }
    void WriteDawg(FILE *fp)
    {
        int idx = 1;
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
	for (int i=0; i<=topnode; i++)
	{
	    if (index[i])
	    {
#ifdef DEBUG
		printf("Writing node %d, index %d\n", i, index[i]);
#endif
	        WriteNode(fp, i);
	    }
	}
    }
    void AddWord(char *word, int pos)
    {
	int n = nodestack[pos];
	printf("%d: %-16s Free %d\n", ++wcnt, word, freenodes);
	while (word[pos])
	{
	    int j = word[pos] - 'A';
	    nodestack[++pos] = n = (node[n].children[j] = AllocateNode(j));
	}
	node[n].SetTerminal();
	stacktop = pos;
    }
    void FreeNode(int n)
    {
	node[n].ChainFree(botnode);
	botnode = n;
	if (topnode == n)
	    while (topnode > 0 && node[--topnode].IsFree());
	freenodes++;
    }
    inline unsigned int NodeCompare(int n1, int n2) const
    {
	return (node[n1].self == node[n2].self &&
		memcmp(node[n1].children,node[n2].children, 26*sizeof(int))==0);
    }
    int MergeNodes(int first, int last, int n, int parent)
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
    int MergeNodes(int i)
    {
	/* This funny way of doing it is (hopefully!) a performance improvement
	   as we no longer have to check for equality in NodeCompare */
	int n = nodestack[i], parent = nodestack[i-1];
	if (MergeNodes(1, n-1, n, parent))
	    return 1;
	else
	    return MergeNodes(n+1, topnode, n, parent);
    }
    void Merge(int pos = 0)
    {
	int i = stacktop;
	while (i>pos) MergeNodes(i--);
    }
    void ShowStats() const
    {
	printf("Max space used: %%%5.2f,  Final space used %%%5.2f\n",
		(double)topnode*100./(double)MAXNODES,
		(double)usednodes*100./(double)MAXNODES);
    }

    void SaveState(char *word, char *lastword)
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
    int RestoreState(char *word, char *lastword)
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
};

static void useage(void)
{
    fprintf(stderr,"Useage: buildict [-R] [-o <output file>] [ <input file> ]\n");
    fprintf(stderr,"\n\tIf no input file is specified, standard input is used\n");
    fprintf(stderr,"\tIf no output file is specified, DAWG is used\n");
    exit(0);
}

static void HandleSignal(int);
typedef void (*handler)(int);

int must_abort = 0;

int main(int argc, char **argv)
{
    FILE *lex = stdin;

    unsigned short l;
    int pos, n;
    char word[80], lastword[80], *dname = "dawg";
    int restore = 0;

    fprintf(stderr, "Welcome to buildict!\n");

    for (int i=1; i<argc; i++)
    {
	switch(argv[i][0])
	{
	case '-':
	    switch(argv[i][1])
	    {
		case 'o':
		    dname = argv[++i];
		    if (i>=argc) useage();
		    break;
		case 'R':
		    restore = 1;
		    break;
		default:
		    useage();
	    }
	    break;
	default:
	    lex = fopen(argv[i],"r");
	    if (lex==0)
	    {
		fprintf(stderr,"Cannot open input file %s\n",
					argv[i]);
		exit(-1);
	    }
	    else if (++i != argc)
		useage();
	    break;
	}
    }
    FILE *dawg = fopen(dname,"wb");
    if (dawg==0)
    {
	fprintf(stderr,"Cannot open output file %s!\n",dname);
	exit(-2);
    }
    fprintf(stderr, "Initialising...\n");
    NodePool *p = new NodePool;

    /* clear the stack containing last word */

    lastword[0]=0;
    //for (i=0;i<=NodePool::MAXLEN;i++) nodeStk[i]=0;

    /* add words */

    if (restore)
    {
	int wcnt = p->RestoreState(word, lastword);
	/* Now skip forward in lex file to next word */
	for (int i = wcnt; i--; )
	    fgets(word, NodePool::MAXLEN, lex);
	fprintf(stderr, "Resuming at %s\n", word);
    }

    (void)signal(SIGINT, (handler)HandleSignal);
#ifndef __MSDOS__
    (void)signal(SIGQUIT, (handler)HandleSignal);
#endif

    while (!must_abort && !feof(lex))
    {
	if (fgets(word,80,lex) == 0) break;
	strupr(word);

	int l = strlen(word)-1;
	while (l>0 && (word[l]<'A' || word[l]>'Z')) l--;
	word[l+1] = 0;

	/* l now indexes the last letter */

	if (l >= NodePool::MAXLEN)
	{
	    fprintf(stderr, "Word is too long! - %s\n", word);
	    continue;
	}
	else if (l<1) /* less than 2 letters */
	{
	    if (l) fprintf(stderr,"WARNING: word %s is too short; skipping\n",word);
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
	p->DumpGraph(stdout);
#endif
    }
    if (must_abort)
    {
	p->SaveState(word, lastword);
	delete p;
	exit(0);
    }
    fclose(lex);

    p->Merge(); /* Make sure we've bottomed out... */
    //p->MarkGraph();
#ifdef DEBUG
    p->DumpGraph(stdout);
#endif
    /* We now have to convert the DAWG into an array of edges
    // in a file.  */
    printf("Writing dawg to file\n");
    p->WriteDawg(dawg);
    fclose(dawg);
    p->DumpGraph(stdout);
    p->ShowStats();
    delete p;
    return 0;
}

static void HandleSignal(int signo)
{
    if (++must_abort>3) signal(SIGINT, SIG_DFL);
    fprintf(stderr, "Interrupted!\n");
}

