#include <stdio.h>

/*
 * Prints the content of stdin to a C character array suitable
 * for including in a program. Also emit a int foo_size=n; so
 * the code can know how large the array is.
 */
int main(int argc, char **argv)
{
    int c, i = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <variable name>\n", argv[0]);
        return 1;
    }

    printf("unsigned char %s[]={\n", argv[1]);
    
    while ((c=getc(stdin)) != EOF) {
        i++;
        printf("0x%02x,%s", c, (i%16) == 0?"\n":"");
    }
    
    printf("\n};\nint %s_size=%d;\n", argv[1], i);
}

