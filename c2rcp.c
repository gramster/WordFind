#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Extract(int typ, FILE *ifp, FILE *ofp)
{
    while (!feof(ifp))
    {
        char buf[256];
	if (fgets(buf, sizeof(buf), ifp) == 0)
	    break;
	if (typ == 0)
	{
	    if (strncmp(buf, "//! ", 4) == 0)
	        fputs(buf+4, ofp);
	}
	else
	{
	    if (strncmp(buf, "//& ", 4) == 0)
	        fputs(buf+4, ofp);
	}
    }
}

int Filter(int typ, const char *infile, const char *outfile)
{
    FILE *ifp = fopen(infile, "r"), *ofp;
    if (ifp == NULL) return -1;
    ofp = fopen(outfile, "w");
    if (ofp == NULL)
    {
        fclose(ifp);
	return -2;
    }
    Extract(typ, ifp, ofp);
    fclose(ofp);
    fclose(ifp);
    return 0;
}

int Process(const char *fname, int typ)
{
    char outname[256], *s;
    strncpy(outname, fname, sizeof(outname));
    s = strrchr(outname, '.');
    if (s == 0) return -3;
    strcpy(s, (typ ? "rsc.h" : ".rcp"));
    return Filter(typ, fname, outname);
}

void useage(void)
{
    fprintf(stderr, "Useage: c2rcp ( [-H | -R] <C_file_path> )...\n");
    exit(0);
}

int main(int argc, char **argv)
{
    int i, typ = 0;
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
	{
	    switch (argv[i][1])
	    {
	    case 'R': typ = 0; break;
	    case 'H': typ = 1; break;
	    default: useage();
	    }
	}
	else switch (Process(argv[i], typ))
	{
	case 0:
	    printf("%s\n", argv[i]);
	    break;
	case -1:
	    fprintf(stderr, "Couldn't open input file %s\n", argv[i]);
	    break;
	case -2:
	    fprintf(stderr, "Couldn't open output file for input %s\n", argv[i]);
	    break;
	case -3:
	    fprintf(stderr, "Couldn't grok filename argument %s\n", argv[i]);
	    break;
	}
    }
    return 0;
}

