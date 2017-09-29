// dic2pdb - converts a master dictionary file to a Pilot
// database, converrting the endianness of the nodes in the
// process, and normalising letter indices to start at 0.

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define TERMINAL	0x40000000u
#define NodesPerRecord	(1000)

main(int argc, char **argv)
{
    if (argc==2)
    {
        struct stat st;
	if (stat(argv[argc-1], &st) == 0)
	{
            FILE *ifp = fopen(argv[argc-1], "rb");
	    if (ifp)
	    {
	        char oname[128];
	        strcpy(oname, argv[argc-1]);
	        char *s = strrchr(oname, '.');
	        if (s) strcpy(s, ".pdb");
		 else strcat(oname, ".pdb");
	        FILE *ofp = fopen(oname, "wb");
	        if (ofp)
	        {
	            // write out the headers

		    unsigned char nm[32], shortbuf[2], longbuf[4];

		    // name

		    strcpy(nm, "xwdc");
		    fwrite(nm, sizeof(char), 32, ofp);
		    printf("Name: xwdc\n");

		    // attributes

		    memset(shortbuf, 0, 2);
		    fwrite(shortbuf, sizeof(char), 2, ofp);

		    // version

		    memset(shortbuf, 0, 2);
		    fwrite(shortbuf, sizeof(char), 2, ofp);

		    // creation and modification date

		    unsigned long now = time(0);
		    unsigned long delta = 49ul * 365ul * 24ul * 3600ul +
			    	17ul * 366ul * 24ul * 3600ul; // 1904 -> 1970
		    now += delta;
		    longbuf[0] = (now>>24)&0xFF;
		    longbuf[1] = (now>>16)&0xFF;
		    longbuf[2] = (now>>8)&0xFF;
		    longbuf[3] = (now)&0xFF;
		    fwrite(longbuf, sizeof(char), 4, ofp);
		    fwrite(longbuf, sizeof(char), 4, ofp);
		    printf("Created: %lu (delta %lu)\n", now, delta);

		    // last backup date

		    memset(longbuf, 0, 4);
		    fwrite(longbuf, sizeof(char), 4, ofp);

		    // modification number

		    memset(longbuf, 0, 4);
		    fwrite(longbuf, sizeof(char), 4, ofp);

		    // appinfo area

		    memset(longbuf, 0, 4);
		    fwrite(longbuf, sizeof(char), 4, ofp);

		    // sortinfo area

		    memset(longbuf, 0, 4);
		    fwrite(longbuf, sizeof(char), 4, ofp);

		    // database type

		    strcpy(nm, "dict");
		    fwrite(nm, sizeof(char), 4, ofp);
		    printf("Type: dict\n");

		    // creator ID

		    strcpy(nm, "xWrd");
		    fwrite(nm, sizeof(char), 4, ofp);
		    printf("Creator: xWrd\n");

		    // unique ID seed

		    memset(longbuf, 0, 4);
		    fwrite(longbuf, sizeof(char), 4, ofp);

		    // NextRecordList ID

		    memset(longbuf, 0, 4);
		    fwrite(longbuf, sizeof(char), 4, ofp);

		    // number of records

		    unsigned long nnodes = st.st_size / 4l;
		    printf("Nodes: %lu\n", nnodes);
		    nnodes++; // add one for the node 0
		    unsigned short nrecs = (nnodes+NodesPerRecord-1) / NodesPerRecord;
		    printf("Records: %u\n", nrecs);
		    shortbuf[0] = nrecs/256;
		    shortbuf[1] = nrecs%256;
		    fwrite(shortbuf, sizeof(char), 2, ofp);

		    // write out the record list of record offsets.
		    // each of these is 8 bytes. Thus the actual
		    // records are at offsets (78+8*nrecs),
		    // (78+8*nrecs+NodesPerRecord*4), ...

		    printf("Writing record list:  0%"); fflush(stdout);
		    int nodesize = (nnodes>=0x1FFFFul) ? 4 : 3; // 3 = packed demo dict
		    unsigned long record_offset = ftell(ofp) + 8*nrecs;
		    for (int i = 0; i < nrecs; i++)
		    {
			// record offset

			printf("\b\b\b%2d%%", ((i+1)*100)/nrecs); fflush(stdout);
			longbuf[0] = (record_offset>>24)&0xFF;
			longbuf[1] = (record_offset>>16)&0xFF;
			longbuf[2] = (record_offset>>8)&0xFF;
			longbuf[3] = (record_offset)&0xFF;
			fwrite(longbuf, sizeof(char), 4, ofp);
		        record_offset += NodesPerRecord*nodesize;

			// attributes and ID

			memset(longbuf, 0, 4);
			fwrite(longbuf, sizeof(char), 4, ofp);
		    }

		    // now write out the nodes

		    longbuf[0] = 0;
		    longbuf[1] = 0;
		    longbuf[2] = 0;
		    longbuf[3] = (nodesize)&0xFF;
		    fwrite(longbuf+4-nodesize, sizeof(char), nodesize, ofp);

		    unsigned long nodenum = 1;
		    printf("\nWriting records:  0%"); fflush(stdout);
	            while (!feof(ifp))
		    {
			unsigned long node;
		        if (fread((char*)&node, 4, 1, ifp) != 1) break;
			unsigned char x = ((node>>24)&0xFF)-1;
			// rearrange bits
			longbuf[0] = ((x&0x60)<<1) | ((x&0x1F)<<1);
			if (nnodes < (128l*1024l))
			{
			    longbuf[0] |= (node>>16)&0x01;
			    longbuf[1] = (node>>8)&0xFF;
			    longbuf[2] = (node)&0xFF;
			}
			else
			{
			    longbuf[1] = (node>>16)&0xFF;
			    longbuf[2] = (node>>8)&0xFF;
			    longbuf[3] = (node)&0xFF;
			}
			fwrite(longbuf, sizeof(char), nodesize, ofp);
			nodenum++;
//			printf("\b\b\b%2d%%", (nodenum*100)/nnodes); fflush(stdout);
#if 1
			printf("%ld : %c%c '%c' %08lx [%02x %02x %02x %02x]\n",
				nodenum, 
				((node&TERMINAL)?'T':' '),
				' ',
				' ',
				node, 
				longbuf[0],
				longbuf[1],
				longbuf[2],
				longbuf[3]);
#endif
		    }
		    printf("\n");
		    // pad the last record
		    memset(longbuf, 0, 4);
		    while ((nodenum++)%NodesPerRecord)
		    {
		        printf("%ld PAD\n", nodenum);
			fwrite(longbuf, sizeof(char), nodesize, ofp);
		    }
	            fclose(ofp);
	        }
	        fclose(ifp);
	    }
	}
    }
}
