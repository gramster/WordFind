#include <stdio.h>
#include <string.h>
#include <ctype.h>

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++)
    {
        FILE *fp = fopen(argv[i], "r");
        if (fp)
        {
            long wrds = 0;
            int maxlen = 0, l;
	    long lengths[64];
	    memset((char*)lengths, 0, 64*sizeof(lengths[0]));
            while (!feof(fp))
            {
                char buff[80];
                if (fgets(buff, sizeof(buff), fp) == 0) break;
                buff[strlen(buff)-1] = 0;
		if (isalpha(buff[0]))
		{
		    int l = strlen(buff);
		    if (l > maxlen) maxlen = l;
		    if (l > 32) puts(buff);
		    if (l < 64) lengths[l]++;
		    ++wrds;
		}
            }
            fclose(fp);
	    printf("File %s has %ld words of up to %d characters\n", argv[i], wrds, maxlen);
	    for (l = 1; l < 64; l++)
	        if (lengths[l]) 
		    printf("%ld words of length %d\n", lengths[l], l);
        }
    }
    return 0;
}

