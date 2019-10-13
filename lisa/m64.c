/**************************************************************************************\
*                                                                                      *
*                         Tiny and Fast mime64 encode                                  *
*              A Future component of The Lisa Emulator Project                         *
*                            http://lisaem.sunder.net                                  *
*                                                                                      *
*                      Copyright (C) 2018 Ray A. Arachelian                            *
*                               All Rights Reserved                                    *
*                                                                                      *
*           This program is free software; you can redistribute it and/or              *
*           modify it under the terms of the GNU General Public License                *
*           as published by the Free Software Foundation; either version 2             *
*           of the License, or (at your option) any later version, or the LGPL         *
*                                                                                      *
*           This program is distributed in the hope that it will be useful,            *
*           but WITHOUT ANY WARRANTY; without even the implied warranty of             *
*           MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
*           GNU General Public License / LGPL License for more details.                *
*                                                                                      *
*           You should have received a copy of the GNU General Public License          *
*           along with this program;  if not, write to the Free Software               *
*           Foundation, Inc., 59 Temple Place #330, Boston, MA 02111-1307, USA.        *
*                                                                                      *
*                   or visit: http://www.gnu.org/licenses/gpl.html                     *
*                                                                                      *
* BUGS:                                                                                *
* TODO: fix length error when decoding.                                                *
*                                                                                      *
*                                                                                      *
\**************************************************************************************/

// get machine depenendent things like uint8, int8, ... uint32, etc.
#include "../include/machine.h"

// typically you'll want to allocate 125%+4 bytes of space for the m64 encoded output
// string, and about 75%+4 of the input string size when decoding.
// LFs added to the output string only in the m64_encode_lf fn, which will require an
// extra byte for every 76 chars of output.


size_t m64_decode_lf(char *in, char *out); // slower, but ignores whitespace
size_t m64_decode(char *in, char *out);    // faster, expects a single long string w/o whitespace
void   m64_encode(char *in, size_t size, char *out);  // returns a long string
void   m64_encode_lf(char *in, size_t size, char *out); // returns lines of b64 encoded text with LFs



// static so internals won't be exported.
static char *mime64_table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
//                         01234567890123456789012345678901234567890123456789012345678901234
//                         0         1         2         3         4         5         6


// For purposes of speed, we don't verify the incoming characters are valid MIME64 chars
// so garbage in=garbage out. We also don't check for = signs in here.  Caller should
// properly zero fill inputs when it gets an = or two, we also don't output = for the
// end.
static inline uint8 m64_decode1(uint8 a)
{
   static int adj_table[8]={93,418,-16,-4,65,65,71,71};
   static int idx;
   idx=((a>>4) & 7);
   return (uint8)( ((int)(a))-adj_table[idx]);
}

// decodes a chunk of 4 into 3 bytes
static inline void m64_decode4(uint8 i1, uint8 i2, uint8 i3, uint8 i4,  uint8 *output)
{
   static uint8 c1,c2,c3,c4;
   c1=m64_decode1(i1);
   c2=m64_decode1(i2);
   c3=m64_decode1(i3);
   c4=m64_decode1(i4);

   output[0] = (c1  << 2) | (c2 >> 4);
   output[1] = (c2  << 4) | (c3 >> 2);
   output[2] = (c3  << 6) | (c4     );
}

size_t m64_decode_lf(char *in, char *o)
{
  // skip whitespace and most other invalid chars
  #define SKIP_SPACE {while( (*in<=' ' || *in>'z') && *in) in++;}
  size_t size=0;
  uint8 c1=0,c2=0,c3=0,c4=0, stop=0;
  uint8 *out=(uint8 *) o; // supress conversion warning

  while(*in && !stop)
  {
  	c1=c2=c3=c4=0;
    SKIP_SPACE;                     {c1=*in; size++; in++;}
  	SKIP_SPACE; if (c1 && c1 !='=') {c2=*in; size++; in++;} else stop=1;
  	SKIP_SPACE; if (c2 && c2 !='=') {c3=*in; size++; in++;} else stop=1;
  	SKIP_SPACE; if (c3 && c3 !='=') {c4=*in; size++; in++;} else stop=1;
    m64_decode4(c1,c2,c3,c4,out);
    out+=3;
  }
  return size;
}

size_t m64_decode(char *in, char *o)
{
  size_t size=0;
  uint8 c1=0,c2=0,c3=0,c4=0, stop=0;
  uint8 *out=(uint8 *) o; // supress conversion warning

  while(*in && !stop)
  {
  	c1=c2=c3=c4=0;
    if (     *in !='=') {c1=*in; size++; in++;} else stop=1;
  	if (c1 && c1 !='=') {c2=*in; size++; in++;} else stop=1;
  	if (c2 && c2 !='=') {c3=*in; size++; in++;} else stop=1;
  	if (c3 && c3 !='=') {c4=*in; size++; in++;} else stop=1;
    m64_decode4(c1,c2,c3,c4,out);
    out+=3;
  }    
  return size;
}


// encode 3 bytes of binary into 4 bytes of mime64 text.
void m64_encode_chunk(uint8 b0, uint8 b1, uint8 b2, char *output)
{
    output[0] = mime64_table[ ((b0 & 0xfc) >> 2)                     ];
    output[1] = mime64_table[ ((b0 & 0x03) << 4) + ((b1 & 0xf0) >> 4)];
    output[2] = mime64_table[ ((b1 & 0x0f) << 2) + ((b2 & 0xc0) >> 6)];
    output[3] = mime64_table[ ((b2 & 0x3f)     )                     ];
}


void m64_encode(char *in, size_t ssize, char *out)
{
	long int size=ssize; // if size is unsigned causes segfault, so convert
    uint8 i1, i2, i3;
    while (size>0)  
    {
    	i1=in[0]; i2=in[1]; i3=in[2]; size-=3;
    	m64_encode_chunk(i1,i2,i3,out); out+=4; in+=3;
    }
    switch(size) // note this falls through, ignore the warning.
    {
          case -2: *out='='; out++;
          case -1: *out='='; out++;
          case  0: *out=0;
    }
}

void m64_encode_lf(char *in, size_t ssize, char *out)
{
	long int size=ssize; 
    int outcount=0;
    uint8 i1, i2, i3;

    *out=0;
    while (size>0)  
    {
    	i1=in[0]; i2=in[1]; i3=in[2]; size-=3; outcount+=4;
    	m64_encode_chunk(i1,i2,i3,out); out+=4;
    	if (outcount==76) {out++; *out='\n'; outcount=0;}
    }
    switch(size) // note this falls through, ignore the warning.
    {
          case -2: out[1]='=';
          case -1: out[2]='=';
          case  0: out[3]=0;
    }
}

#ifdef M64MAIN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef M64SELFTEST
char buffer1[4096];
char buffer2[4096];

//gcc m64.c -DM64MAIN -DM64SELFTEST $(wx-config --cflags) -o m64 && ./m64
int main(int argc, char *argv[])
{
    char *testin="Testing m64 encode - plaintext. Foo Bar Baz qUUx!\n";

    // from wikipedia
    char *testin2="TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlz"
                  "IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2Yg"
                  "dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGlu"
                  "dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRo"
                  "ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=";
    bzero(buffer1,4095);
    bzero(buffer2,4095);
    m64_encode(testin, strlen(testin), buffer1);
    printf("input :%s\n",testin);
    printf("output:%s\n",buffer1);
    m64_decode_lf(buffer1,buffer2);
    printf("decode:%s\n",buffer2);
    if (strcmp(testin,buffer2)!=0) puts("FAIL - input and output mismatch");
    bzero(buffer1,4095);
    m64_decode_lf(testin2,buffer1);
    printf("static decode:%s\n",buffer1);
    
}
#else

int main(int argc, char *argv)
{
	printf(	"not yet implemented\n");
}
#endif
#endif