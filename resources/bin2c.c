#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
 FILE *in;
 unsigned char c;
 int i,z=0;
 char name[128],*s;
 for (i=1; i<argc; i++)
 {
   strncpy(name,argv[i],127);
   s=strstr(name,".");
   if (s) s[0]=0;

   in=fopen(argv[i],"rb");
   if (!in) {fprintf(stderr,"Error open %s\n",argv[i]); perror("Could not open file"); exit(1);}
   z=0;
   fprintf(stdout,"uint8 %s={\n"
		  "    /*           +0   +1   +2   +3   +4   +5   +6   +7   +8   +9   +a   +b   +c   +d   +e   +f */\n"
		  "    /* 0000 */ ",name);
   while(!feof(in))
   {
     c=fgetc(in);
     fprintf(stdout,"0x%02x",c); z++;
     if (!feof(in)) fprintf(stdout,",");
     if ((z & 15)==0) fprintf(stdout,"\n    /* %04x */ ",z);
   }
   fprintf(stdout,"};\n\n");
 }


}
