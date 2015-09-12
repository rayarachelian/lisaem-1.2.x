/* Generator is (c) James Ponder, 1997-2001 http://www.squish.net/generator/ */

// This has been heavily modified for the Lisa Emulator...  Modifications of this file
// are (c) 1998-2002 Ray Arachelian, http://lisa.sunder.net  updated to match gen 0.35

#define IN_CPU68K_C 1
#include "vars.h"
#include "generator.h"
#include "cpu68k.h"
#include "ui.h"
#include "def68k-iibs.h"
#include "def68k-proto.h"
#include "def68k-funcs.h"


#ifdef DEBUG
#undef DEBUG
#endif


/*** externally accessible variables ***/
int do_iib_check=0;
t_iib *cpu68k_iibtable[65536];
void (*cpu68k_functable[65536 * 2]) (t_ipc * ipc);
int cpu68k_totalinstr;
int cpu68k_totalfuncs;
unsigned long cpu68k_frames;
unsigned long cpu68k_frozen;     /* cpu frozen, do not interrupt, make pending */
int abort_opcode=0;              /* used by MMU to signal access to illegal/invalid memory -- RA*/



t_regs regs;
uint8 movem_bit[256];

            #ifdef DEBUG
            void dumpallmmu(void);
            #endif

// used by cpu68k_makeipclist as scratch space during opcode corrections.
// We allocate these but don't free them as they're reused, and repeated malloc/free's can
// fragment memory and are slow.  So we just regrow as needed.  But we need a nice big chunk
// of them so we can avoid realloc as much as possible.  16K worth of ipc's should be good.
t_ipc **ipcs=NULL;
// 2005.04.14 - set IPCS much much higher - sorry, LisaTest needs'em.
#define ipcs_to_get 8192                                                            //1024

/*** forward references ***/

void cpu68k_reset(void);


//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//  //  //  //  //  //  //     D E B U G G I N G   A S S E R T I O N   F U N C T I O N S    //  //  //  //  //  //  //  //  //
//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //


#ifdef DEBUG

static int iib_checks_enabled=0;
static void *iib_table_ptr[65536];
static void *iib_check_fns[65536];

void seed_iib_check(void)
{
  long i;

  return;  // disabled for speed
  for ( i=0; i<65536; i++)
  {
    iib_table_ptr[i]=cpu68k_iibtable[i];
    if ( cpu68k_iibtable[i])
               iib_check_fns[i]=(void *)(cpu68k_iibtable[i]->funcnum);
    else
            iib_check_fns[i]=NULL;
  }
  iib_checks_enabled=1;
}

void my_check_iib(char *filename, char *function, long line)
{
 t_iib *myiib;

 long i;

 return; // disabled
 myiib=cpu68k_iibtable[0x6602];
 if (!myiib->funcnum)
 {  EXIT(171,0,"IIB for 6602 got clobbered above this! %s:%s:%ld",filename,function,line);  }

 myiib=cpu68k_iibtable[0x4841];
 if (!myiib || !myiib->funcnum)
 {   EXIT(409,0,"IIB for 4841 got clobbered above this! %s:%s:%ld",filename,function,line); }

  return;                             // disable the full check to get some speed

  if ( !iib_checks_enabled) {seed_iib_check();iib_checks_enabled=1; return;  }

  for ( i=0; i<65536; i++)
   {
    if (iib_table_ptr[i]!=cpu68k_iibtable[i])
    {
        fprintf(buglog,"CRASH! IIB table # %04x got clobbered here or above this! %s:%s:%ld",(uint16)i,filename,function,line);
        fflush(buglog);
        EXIT(410);
    }
    if (iib_check_fns[i])
    {
       if ((void *)iib_check_fns[i]!=(void *)(cpu68k_iibtable[i]->funcnum))
       {
        fprintf(buglog,"BOINK! IIB for opcode # %04x got clobbered here or above this! %s:%s:%ld",(uint16)i,filename,function,line);
        fflush(buglog);
        EXIT(411);
       }
    }
   }//////////////////////////////////////////

}


void my_check_valid_ipct(t_ipc_table *ipct, char *file, char *function, int line, char *text)
{
 uint16 i;
 int valid=0;
 t_ipc_table *start=NULL, *end=NULL;
 void *vstart, *vend, *vipct;

  vipct =(void *)ipct;

  for (i=0; i<=iipct_mallocs && !valid; i++)
   {
     start=ipct_mallocs[i];           vstart=(void *) start;
     end=&start[sipct_mallocs[i]];    vend  =(void *) end;

     if ( vipct>=vstart && vipct<=vend) valid=1;
   }

  fflush(buglog);
  if ( valid) { fprintf(buglog,"%s:%s:%d valid ipct %p %s",file,function,line,ipct,text);  fflush(buglog);}
  else        { fprintf(buglog,"%s:%s:%d ***HALT*** invalid ipct %p %s",file,function,line,ipct,text);  fflush(buglog); EXIT(412);}
}

 #define DEBUG_IPCT( ipct, text ) if (debug_log_enabled) my_check_valid_ipct(ipct, __FILE__, __FUNCTION__, __LINE__, text)

#else

 #define DEBUG_IPCT( ipct, text ) {}

#endif

//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //
//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //




int cpu68k_init(void)
{
  t_iib *iib;
  uint16 bitmap;
  int i, j, sbit, dbit, sbits, dbits;

  memset(cpu68k_iibtable, 0, sizeof(cpu68k_iibtable));  // 0x55 is a sentinel value - odd so it traps long/word writes
  memset(cpu68k_functable,0, sizeof(cpu68k_functable));
  memset(&regs, 0, sizeof(regs));

  cpu68k_frozen = 0;
  cpu68k_totalinstr = 0;

  for (i = 0; i < iibs_num; i++) {
    iib = &iibs[i];

    bitmap = iib->mask;
    sbits = 0;
    dbits = 0;

    for (j = 0; j < 2; j++) {
      switch (j ? iib->stype : iib->dtype) {
      case dt_Dreg:
      case dt_Areg:
      case dt_Aind:
      case dt_Ainc:
      case dt_Adec:
      case dt_Adis:
      case dt_Aidx:
        if (j) {
          bitmap ^= 7 << iib->sbitpos;
          sbits = 3;
        } else {
          bitmap ^= 7 << iib->dbitpos;
          dbits = 3;
        }
        break;
      case dt_AbsW:
      case dt_AbsL:
      case dt_Pdis:
      case dt_Pidx:
        break;
      case dt_ImmB:
      case dt_ImmW:
      case dt_ImmL:
      case dt_ImmS:
        break;
      case dt_Imm3:
        if (j) {
          bitmap ^= 7 << iib->sbitpos;
          sbits = 3;
        } else {
          bitmap ^= 7 << iib->dbitpos;
          dbits = 3;
        }
        break;
      case dt_Imm4:
        if (j) {
          bitmap ^= 15 << iib->sbitpos;
          sbits = 4;
        } else {
          bitmap ^= 15 << iib->dbitpos;
          dbits = 4;
        }
        break;
      case dt_Imm8:
      case dt_Imm8s:
        if (j) {
          bitmap ^= 255 << iib->sbitpos;
          sbits = 8;
        } else {
          bitmap ^= 255 << iib->dbitpos;
          dbits = 8;
        }
        break;
      case dt_ImmV:
        sbits = 12;
        bitmap ^= 0x0FFF;
        break;
      case dt_Ill:
        /* no src/dst parameter */
        break;
      default:
        LOG_CRITICAL("CPU definition #%d incorrect", i);
        return 1;
      }
    }
    if (bitmap != 0xFFFF) {
      LOG_CRITICAL("CPU definition #%d incorrect (0x%x)", i, bitmap);
      return 1;
    }
    for (sbit = 0; sbit < (1 << sbits); sbit++) {
      for (dbit = 0; dbit < (1 << dbits); dbit++) {
        bitmap = iib->bits | (sbit << iib->sbitpos) | (dbit << iib->dbitpos);
        if (iib->stype == dt_Imm3 || iib->stype == dt_Imm4
            || iib->stype == dt_Imm8) {
          if (sbit == 0 && iib->flags.imm_notzero) {
            continue;
          }
        }
        if (cpu68k_iibtable[bitmap] != NULL) {EXIT(283,0,"CPU definition #%d conflicts (0x%x)", i, bitmap);}

        cpu68k_iibtable[bitmap] = iib;
        /* set both flag and non-flag versions */

#ifdef NORMAL_GENERATOR_FLAGS
        cpu68k_functable[bitmap * 2] = cpu68k_funcindex[i * 2];
        cpu68k_functable[bitmap * 2 + 1] = cpu68k_funcindex[i * 2 + 1];
#else
        // This forces Generator to calculate flags for every instruction that does it.
        // it's slower.
        cpu68k_functable[bitmap * 2] = cpu68k_funcindex[i * 2  +1];
        cpu68k_functable[bitmap * 2 + 1] = cpu68k_funcindex[i * 2 +1];
#endif
        cpu68k_totalinstr++;
      }
    }
  }

  j = 0;

  for (i = 0; i < 65536; i++) if (cpu68k_iibtable[i]) j++;


  if (j != cpu68k_totalinstr) 
    {
      EXIT(84,0,"Instruction count not verified (%d/%d)\n",
                  cpu68k_totalinstr, i);
    }

  cpu68k_totalfuncs = iibs_num;

  for (i = 0; i < 256; i++) {
    for (j = 0; j < 8; j++) {
      if (i & (1 << j))
        break;
    }
    movem_bit[i] = j;
  }

  //LOG_VERBOSE("CPU: %d instructions supported by %d routines",
  //             cpu68k_totalinstr, cpu68k_totalfuncs);
  return 0;
}

#ifdef DEBUG
void cpu68k_printipc(t_ipc * ipc)
{

    if ( DEBUGLEVEL>4 || debug_log_enabled==0) return;


    fprintf(buglog,"IPC @ 0x%p\n", ipc);
    fprintf(buglog,"  opcode: %04X, uses %X set %X\n", ipc->opcode, ipc->used,
		ipc->set);
    fprintf(buglog,"  src = %08X\n", ipc->src);
    fprintf(buglog,"  dst = %08X\n", ipc->dst);
    fprintf(buglog,"  fn  = %p\n", ipc->function);
    //if ( !ipc->function)
    //{
    //    fprintf(buglog,"**DANGER*** No function pointer!\n");
    //}
    fprintf(buglog,"next  = %p\n", ipc->next);
    fprintf(buglog,"length= %d\n",ipc->wordlen);
    fflush(buglog);

    //check_iib();
	//printf("Next  = %0X\n",ipc->next);
}
#else
 void cpu68k_printipc(t_ipc * ipc) {}
#endif
/* fill in ipc */

// this comment is no longer applicable.
/********* Note, there is a small possibility of things breaking in catastrphic ways here.
The problem is this function is called with a pointer to translated memory (*addr)  if the
address falls at the very end of a page, but the operand words of this opcode fall on a totally
different page in a different MMU segment that we'll get the wrong operands.  To fix this we
could get rid of *addr and instead use addr68k and replace all accesses with fetchbyte, fetchword,
etc. ***/

void cpu68k_ipc(uint32 addr68k, t_iib * iib, t_ipc * ipc)
{
	t_type type;
	uint32 *p, a9,adr;
    //uint16 rfn;
    //uint8 xaddr[32];
    uint8  *addr;
    #ifdef DEBUG
     int dbx=debug_log_enabled;
    #endif
    addr=NULL;

   #ifdef DEBUG
	checkcontext(context,"pre-entry to cpu68k_ipc");

    if ( !ipc)
    {
      ALERT_LOG(0,"I was passed a NULL ipc.\nLet the bodies hit the floor... 1, Nothin' wrong with me. 2, Nothing wrong with me, 3. Nothing wrong with me. 4. Nothing wrong with me.");
  	  EXIT(123,0,"Received NULL IPC!");
    }

    if ( !iib)
    {
      ALERT_LOG(0,"I was passed a NULL iib.\nLet the bodies hit the floor... 1, Nothin' wrong with me. 2, Nothing wrong with me, 3. Nothing wrong with me. 4. Nothing wrong with me."); EXIT(123);
  	  EXIT(123,0,"Received NULL IIB!");    }
   #endif

    abort_opcode=2;
	addr68k &= 0x00ffffff;

	a9    =((addr68k)    & 0x00ffffff)>>9;
	adr   =((addr68k+11) & 0x00ffffff)>>9; // test version 11 bytes later of the same.


  #ifdef DEBUG
     dbx=debug_log_enabled;      debug_log_enabled=0;
  #endif

  abort_opcode=2;
  ipc->opcode = fetchword(addr68k);
  if (abort_opcode==1) {DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly"); return; }

  ipc->clks=iib->clocks;

  #ifdef DEBUG
     debug_log_enabled=dbx;
  #endif



  ipc->wordlen = 1;
  if (!iib) {
    /* illegal instruction, no further details (wordlen must be set to 1) */
    return;
  }

  ipc->used = iib->flags.used;
  ipc->set = iib->flags.set;

  //check_iib();

  if ((iib->mnemonic == i_Bcc) || (iib->mnemonic == i_BSR)) {
    /* special case - we can calculate the offset now */
    /* low 8 bits of current instruction are addr+1 */
    //ipc->src = (sint32)(*(sint8 *)(addr + 1));
    abort_opcode=2;                     // force memory fn's to ignore it.
    ipc->src = (sint32)((sint8)(fetchbyte(addr68k+1)));
    if (abort_opcode==1)     // flush mmu and retry once
    {   DEBUG_LOG(0,"Got abort_opcode, doing mmuflush and retrying.");
        mmuflush(0x2000);
        abort_opcode=2;
        ipc->src = (sint32)((sint8)(fetchbyte(addr68k+1)));
        DEBUG_LOG(0,"Got abort_opcode previously, did mmuflush and retried.  abort_opcode is %d",abort_opcode);
        if (abort_opcode==1)  return;           // MMU exception
    }
    abort_opcode=0;
    DEBUG_LOG(205,"i_Bcc @ %08x target:%08x opcode:%04x",addr68k,ipc->src,ipc->opcode);

    if (ipc->src == 0) {
     #ifdef DEBUG
      dbx=debug_log_enabled;      debug_log_enabled=0;
     #endif

      abort_opcode=2;
      ipc->src = (sint32)(sint16)(fetchword(addr68k + 2));
      if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");
      abort_opcode=0;

      DEBUG_LOG(205,"i_Bcc2 @ %08x target:%08x opcode:%04x",addr68k,ipc->src,ipc->opcode);
      ipc->wordlen++;
    }
    #ifdef DEBUG
     dbx=debug_log_enabled;
    #endif

    ipc->src += addr68k + 2;    /* add PC of next instruction */
    DEBUG_LOG(205,"i_Bcc2 @ %08x target:%08x opcode:%04x",addr68k,ipc->src,ipc->opcode);
    return;
  }
  if (iib->mnemonic == i_DBcc || iib->mnemonic == i_DBRA) {
    /* special case - we can calculate the offset now */
    #ifdef DEBUG
     dbx=debug_log_enabled;   debug_log_enabled=0;
    #endif

    abort_opcode=2;
    ipc->src = (sint32)(sint16)fetchword(addr68k + 2);
    if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");
    abort_opcode=0;

     #ifdef DEBUG
     debug_log_enabled=dbx;
     #endif
    ipc->src += addr68k + 2;    /* add PC of next instruction */
    DEBUG_LOG(205,"i_DBcc/DBRA @ %08x target:%08x opcode:%04x",addr68k,ipc->src,ipc->opcode);
    ipc->wordlen++;
    return;
  }

  addr += 2;
  addr68k += 2;

  //check_iib();

 for (type = 0; type < 2; type++)
 {
   if (type == tp_src)
     p = &(ipc->src);
   else
     p = &(ipc->dst);

   switch (type == tp_src ? iib->stype : iib->dtype)
   {
    case dt_Adis:
     #ifdef DEBUG
        dbx=debug_log_enabled; debug_log_enabled=0;
     #endif

      *p = (sint32)(sint16)(fetchword(addr68k));
     #ifdef DEBUG
       debug_log_enabled=dbx;
     #endif

      ipc->wordlen++;
      DEBUG_LOG(205,"dt_Adis @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
      addr += 2;
      addr68k += 2;
      break;


    case dt_Aidx:
      // 68K Ref says: Address Register Indirect with Index requires one word of extension formatted as:
      // D/A bit 15, reg# bits 14,13,12, Word/Long bit 11.  Bits 10-8 are 0's.  low 8 bits are displacement

      abort_opcode=2;
      *p = (sint32)(sint8)fetchbyte(addr68k+1);   // displacement (low byte of extension)
      if (abort_opcode==1)     // flush mmu and retry once
       {   abort_opcode=2;   mmuflush(0x2000);
           DEBUG_LOG(0,"Got abort_opcode=1, did mmu_flush retrying");
           *p = (sint32)(sint8)fetchbyte(addr68k+1);
           if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");
           if (abort_opcode==1)  return;           // MMU exception occured again, bye bye.
       }

      abort_opcode=2;

      //20060203// *p = (*p & 0xFFFFFF) | (fetchbyte(addr68k) << 24);  // push mode bits to high (D/A,reg#,W/L)
      ipc->reg=fetchbyte(addr68k);  //20060203//

       if (abort_opcode==1)     // flush mmu and retry once
       {   abort_opcode=2;   mmuflush(0x2000);
           DEBUG_LOG(0,"Got abort_opcode, mmuflush'ed, retrying.");
           //*p = (*p & 0xFFFFFF) | (fetchbyte(addr68k) << 24);
           ipc->reg=fetchbyte(addr68k);   //20060203//
           if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");
           if (abort_opcode==1)  return;           // MMU exception
       }
       abort_opcode=0;

      DEBUG_LOG(205,"dt_Adix @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
      ipc->wordlen++;
      addr += 2;
      addr68k += 2;
      break;


    case dt_AbsW:
     #ifdef DEBUG
        dbx=debug_log_enabled; debug_log_enabled=0;
     #endif
      abort_opcode=2;
      *p = (sint32)(sint16)fetchword(addr68k);
      if (abort_opcode==1) {DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly"); return;}
      abort_opcode=0;
     #ifdef DEBUG
        debug_log_enabled=dbx;
     #endif

      ipc->wordlen++;
      DEBUG_LOG(205,"dt_AbsW @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
      addr += 2;
      addr68k += 2;
      break;


    case dt_AbsL:
      //*p = (uint32)((LOCENDIAN16(*(uint16 *)addr) << 16) +
      //              LOCENDIAN16(*(uint16 *)(addr + 2)));
      abort_opcode=2;
      *p=fetchlong(addr68k);
      if (abort_opcode==1) {DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly"); return;}
      abort_opcode=0;
      DEBUG_LOG(205,"dt_AbsL @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
      ipc->wordlen += 2;
      addr += 4;
      addr68k += 4;
      break;


    case dt_Pdis:
     #ifdef DEBUG
        dbx=debug_log_enabled; debug_log_enabled=0;
     #endif
      abort_opcode=2;
      *p = (sint32)(sint16)fetchword(addr68k);
      if (abort_opcode==1) {DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly"); return;}
      abort_opcode=0;
     #ifdef DEBUG
        debug_log_enabled=dbx;
     #endif

      *p += addr68k;

      DEBUG_LOG(0,"dt_Pdis @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
      ipc->wordlen++;

      addr += 2;
      addr68k += 2;

      break;


    case dt_Pidx:
     #ifdef DEBUG
        dbx=debug_log_enabled; debug_log_enabled=0;
     #endif


      abort_opcode=2;
      *p = ((sint32)(sint8)(fetchbyte(addr68k+1))  + addr68k);
      if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");
      if (abort_opcode==1)     // flush mmu and retry once
       {   abort_opcode=2;   mmuflush(0x2000);
           DEBUG_LOG(0,"got abort_ocode, mmuflush'ed - retyring.");
           *p = ((sint32)(sint8)(fetchbyte(addr68k+1))  + addr68k);
           if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");
           if (abort_opcode==1)  return;
       }  // retry failed, abort it.

      abort_opcode=2;

      //20060203// *p = (*p & 0x00FFFFFF) | (fetchbyte(addr68k)) << 24;  // these are the ext word flags+register
      ipc->reg=fetchbyte(addr68k);  //20060203//

      if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");
       if (abort_opcode==1)     // flush mmu and retry once
       {   abort_opcode=2;   mmuflush(0x2000);
           DEBUG_LOG(0,"got mmu exception, abort_opcode=1, mmuflush'ed - retrying.");

           // so apprently I cannot comment this out at all, something in the code apparently uses
           // this flag, and I don't know what. :-(  shyte.  Ditto for AIDX

           /****** THIS WAS THE MOTHER FUCKER THAT WAS MAKING PC AND A-REGS GO NEGATIVE! ****/
           //*p = (*p & 0x00FFFFFF) | (fetchbyte(addr68k)) << 24;  // <<--- is this correct???
           ipc->reg=fetchbyte(addr68k);  //20060203//

           if (abort_opcode==1)  return;
       }  // retry failed, abort it.



      abort_opcode=0;
     #ifdef DEBUG
        debug_log_enabled=dbx;
     #endif

      DEBUG_LOG(0,"dt_Pidx @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
      ipc->wordlen++;
      addr += 2;
      addr68k += 2;
      break;


    case dt_ImmB:
      /* low 8 bits of next 16 bit word is addr+1 */
     #ifdef DEBUG
        dbx=debug_log_enabled; debug_log_enabled=0;
     #endif
       abort_opcode=2;
       *p = (uint32)fetchbyte(addr68k + 1);
       if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");
       if (abort_opcode==1)     // flush mmu and retry once
       {   abort_opcode=2;   mmuflush(0x2000);
           *p = (uint32)fetchbyte(addr68k + 1);
           if (abort_opcode==1)  return;
       }  // retry failed, abort it.


       abort_opcode=0;
     #ifdef DEBUG
        debug_log_enabled=dbx;
     #endif

      DEBUG_LOG(205,"dt_ImmB @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
      ipc->wordlen++;
      addr += 2;
      addr68k += 2;
      break;


    case dt_ImmW:
     #ifdef DEBUG
        dbx=debug_log_enabled; debug_log_enabled=0;
     #endif
      abort_opcode=2;
      *p = (uint32)fetchword(addr68k);
      if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");
      abort_opcode=0;

     #ifdef DEBUG
        debug_log_enabled=dbx;
     #endif
      DEBUG_LOG(205,"dt_ImmW @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
      ipc->wordlen++;
      addr += 2;
      addr68k += 2;
      break;


    case dt_ImmL:
     #ifdef DEBUG
        dbx=debug_log_enabled; debug_log_enabled=0;
     #endif
      abort_opcode=2;
      *p = (uint32)fetchlong(addr68k); // ((LOCENDIAN16(*(uint16 *)addr) << 16) +  LOCENDIAN16(*(uint16 *)(addr + 2)));
      if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");
      abort_opcode=0;

     #ifdef DEBUG
        debug_log_enabled=dbx;
     #endif


      DEBUG_LOG(205,"dt_ImmL @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
      ipc->wordlen += 2;
      addr += 4;
      addr68k += 4;
      break;


    case dt_Imm3:
      if (type == tp_src)
         {
             *p = (ipc->opcode >> iib->sbitpos) & 7;
             DEBUG_LOG(205,"dt_Imm3 src @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
         }
      else
         {
             *p = (ipc->opcode >> iib->dbitpos) & 7;
             DEBUG_LOG(205,"dt_Imm3 dst @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
         }
      break;


    case dt_Imm4:
      if (type == tp_src)
         {
            *p = (ipc->opcode >> iib->sbitpos) & 15;
            DEBUG_LOG(205,"dt_Imm4 src @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
         }
      else
        {
            *p = (ipc->opcode >> iib->dbitpos) & 15;
            DEBUG_LOG(205,"dt_Imm4 dst @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
        }
      break;


    case dt_Imm8:
      if (type == tp_src)
        {
            *p = (ipc->opcode >> iib->sbitpos) & 255;
            DEBUG_LOG(205,"dt_Imm8 src @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
        }
      else
        {
            *p = (ipc->opcode >> iib->dbitpos) & 255;
            DEBUG_LOG(205,"dt_Imm8 dst @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
        }
      break;



    case dt_Imm8s:
      if (type == tp_src)
        {
            *p = (sint32)(sint8)((ipc->opcode >> iib->sbitpos) & 255);
            DEBUG_LOG(200,"dt_Imm8s src @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
        }
      else
        {
            *p = (sint32)(sint8)((ipc->opcode >> iib->dbitpos) & 255);
            DEBUG_LOG(200,"dt_Imm8s dst @ %08x target:%08x opcode:%04x",addr68k,*p,ipc->opcode);
        }
      break;




    default:
      break;
    }
  }
  /******************* FUN ENDS HERE ***********************************/
    #ifdef DEBUG
    check_iib();
    checkcontext(context,"exit from cpu68k_ipc");
    #endif
    //DEBUG_LOG(200,"returned.");
}



/****
typedef struct _t_ipc_table
{
	// Pointers to all the IPC's in this page.  Since the min 68k opcode is 2 bytes in size
	// the most you can have are 256 instructions per page.  We thus no longer need a hash table
	// nor any linked list of IPC's as this is a direct pointer to the IPC.  Ain't life grand?
	t_ipc ipc[256];

	// These are merged together so that on machines with 32 bit architectures we can
	// save four bytes.  It will still save 2 bytes on 64 bit machines.
	union _t
	{ 	uint32 clocks;
		_t_ipc_table *next;
	} t;


#ifdef PROCESSOR_ARM
	void (*compiled)(struct _t_ipc *ipc);
	//uint8 norepeat;	// what's this do? this only gets written to, but not read.  Maybe Arm needs it?
#endif
}t_ipc_table;
****/

// keep me - I've been checked.
void init_ipct_allocator(void)
{
	uint32 i; t_ipc_table *ipct;
	iipct_mallocs=0;
	ipcts_allocated=0;
	ipcts_used=0;
	ipcts_free=0;

    DEBUG_LOG(205,"init ipct_allocator.");
	// clear our table of pointers
	for (i=0; i<MAX_IPCT_MALLOCS; i++) ipct_mallocs[i]=NULL;

    if  (  (ipct_mallocs[0]=(t_ipc_table *)malloc(initial_ipcts * (sizeof(t_ipc_table) ) ))==NULL)
	{
        EXIT(85,0,"Out of memory while allocating initial ipc list ");
	}
	sipct_mallocs[0]=initial_ipcts;


	/* ----- Add all of them to the free ipc_linked list  -----*/
	ipct=ipct_free_head=((t_ipc_table *)ipct_mallocs[0]);

    for (i=0; i<initial_ipcts-1; i++)  ipct[i].t.next = &(ipct[i+1]);

    ipct[i].t.next=NULL; ipct_free_tail=&ipct[i];
	ipcts_used=0; ipcts_allocated=initial_ipcts;
    ipcts_free=i;


    DEBUG_LOG(0,"zzzzzzz ipct land allocated:: %p -to- %p", ipct_mallocs[0], (void *)(ipct_mallocs[0]+initial_ipcts * sizeof(t_ipc_table)));

}

// Keep me, I've been checked.

// *** DANGER YOU MUST DO mt->table=NULL immediately after calling this function to avoid lots of grief!  It can't do it for you!

//20061223  Hmmm, this shows up as the biggest cpu hog in gprof... MUST OPTIMIZE!
void free_ipct(t_ipc_table *ipct)
{
	int i;
    t_ipc *ipc=NULL;

    //#ifdef DEBUG
    //do_iib_check=1;   seed_iib_check();
    //#endif
    //
    //DEBUG_LOG(200,"ipct pointer is %p",ipct);

	if (!ipct) return;

    //DEBUG_IPCT( ipct, "about to free" );
    //DEBUG_LOG(200,"ipct pointer is %p",ipct);
    //check_iib();

    // add the freed ipc table to the end of the free chain
	ipct_free_tail->t.next=ipct;

	// clear each of the IPC's in this blocks so incase we accidentally re-used them
	// we'll get an error.

    //DEBUG_LOG(200,"ipct pointer is %p",ipct);

    for (i=0; i<256; i++)
      {
        ipc=&(ipct->ipc[i]);
        ipc->function=NULL;
        ipc->used=0;
        // might be able to remove these since used is set, but is it checked?
        ipc->set=0;  ipc->opcode=0;  ipc->src=0;
        //check_iib();
      }

	ipct->t.next=NULL; // the next in the free chain of ipc blocks isn't yet here.
    ipcts_free++; ipcts_used--;         // update free/used counts
    ipct_free_tail=ipct;                // this ipct is now the tail of the free chain
    //check_iib();
}


// keep me, I've been checked.
/*---- Take an IPC off the top of the free chain and return it to the caller ---*/
t_ipc_table *get_ipct(void)
{
	uint32 size_to_get, i, j;
    t_ipc_table *ipct=NULL;

    //check_iib();
	/*--- Do we have any free ipcs? ---*/
	if (ipcts_free && ipct_free_head!=NULL)
	{
        ipcts_free--; ipcts_used++;     // update free/used count
        ipct=ipct_free_head;            // pop an ipct off the chain
        ipct_free_head=ipct->t.next;    // the next one on the chain is the new head
        ipct->t.next=NULL;              // clean the "next" link off the one we grabbed to avoid mis-reuse

//        check_iib();
//        DEBUG_LOG(200,":::::::::::: %ld ipcts_free, %ld ipcts_used ::::::::::::",ipcts_free,ipcts_used);
//        DEBUG_LOG(200,"zzzzzzz ipct returned is: %p zzzzzzzzzzzzzzz",ipct);
        //DEBUG_IPCT( ipct, "from free" );

        return ipct;
    }
	else /*---- Nope! We're out of IPCt's, allocate some more.  ----*/
	{

        if ( ipcts_free) { EXIT(3,0,"Bug! ipcts_free is %ld, but ipct_free_head is null",ipcts_free);}

        //check_iib();

        /*--- Did we call Malloc too many times? ---*/
        if ((iipct_mallocs++)>MAX_IPCT_MALLOCS) { EXIT(2,0,"Excessive mallocs of ipct's recompile with more!");}

		size_to_get = (ipcts_allocated/IPCT_ALLOC_PERCENT)+1; // add a percentange of what we have, least 1
		if ( (ipct_mallocs[iipct_mallocs]=(t_ipc_table *)malloc(size_to_get * sizeof(t_ipc_table)+1)  )==NULL)
		{
            DEBUG_LOG(0,"Out of memory getting more ipcs: %p was returned",  ipct_mallocs[iipct_mallocs]);
            DEBUG_LOG(0,"%d ipcts allocated so far, %d are free, %d used, %d mallocs done", ipcts_allocated, ipcts_free, ipcts_used, iipct_mallocs);
            EXIT(86,0,"Out of memory while allocating more ipct's");
		}

//        DEBUG_LOG(0,"zzzzzzz ipct land allocated:: %p -to- %p", ipct_mallocs[iipct_mallocs], (void *)(ipct_mallocs[iipct_mallocs]+size_to_get * sizeof(t_ipc_table)));

		// Zap the new IPC's and link them to the free linked list ---  since we're fresh out
		// of ipc table entries what we've just allocated is the new head of the free list

        sipct_mallocs[iipct_mallocs]=size_to_get;  ipcts_allocated+=size_to_get;
        ipct_free_head=&ipct_mallocs[iipct_mallocs][1]; ipct=ipct_mallocs[iipct_mallocs];
//        DEBUG_LOG(205,"Allocating more ipct's.  we now did %ld mallocs total are:%ld",iipct_mallocs,ipcts_allocated);

		// the 1st entry goes back to the caller.  Wipe the unused IPC's just to be sure
		// there's no bugs accidentally reusing them.
		for (j=0; j<256; j++)
		{
			ipct->ipc[j].function=NULL;
            ipct->ipc[j].used=0;  ipct->ipc[j].set=0; ipct->ipc[j].opcode=0; ipct->ipc[j].src=0; ipct->ipc[j].dst=0;
        }
        ipct->t.next=NULL;


//        check_iib();

        ipcts_free=size_to_get-1;      // no need to add since we know we have none left
        for (i=1; i<ipcts_free; i++)   // start at 1 since we will return ipct0 to caller
        {
            ipct[i].t.next=&ipct[i+1]; // link it to the next free one in the chain
			for (j=0; j<256; j++)
            { ipct[i].ipc[j].function=NULL; ipct[i].ipc[j].used=0; ipct[i].ipc[j].set=0;
              ipct[i].ipc[j].opcode=0; ipct[i].ipc[j].src=0; ipct[i].ipc[j].dst=0; }
        }
        ipct_free_tail=&ipct[i];       // fix the tail.
        ipct_free_tail->t.next=NULL;   //last free one in chain has no next link.
	}
    //check_iib();
//    DEBUG_LOG(200,":::::::::::: %ld ipcts_free, %ld ipcts_used ::::::::::::",ipcts_free,ipcts_used);
//    DEBUG_LOG(200,"zzzzzzz ipct returned is: %p zzzzzzzzzzzzzzz",ipct);
    //DEBUG_IPCT( ipct, "freshly allocated" );
    return ipct;
}



/*
	Need to check all of these.  Remember:  Pages are 512 bytes long, it's ok to leave one at the end.
	should any block be greater than 512 bytes long, chopt it there and force it to set the flags if it
	does set any flags.  Also, we should have each function set it's own clocks.  Yes, it's slower, but
	it's far more acurate.

*/

void checkNullIPCFN(void)
{
long int i; int q=0;

for ( i=0; i<65536*2; i++)
        if ( !cpu68k_functable[i]) {DEBUG_LOG(0,"Null function: for opcode %04x:%d",(uint16)(i>>1),(uint16)(i &1) ); q=1;}
 if ( q) {EXIT(13,9,"failed checkNullIPCFN"); }
 //check_iib();
}



// 20061223 need to optimize this - it's the heaviest fn according to gprof.
t_ipc_table *cpu68k_makeipclist(uint32 pc)
{
	// Generator was meant to work from ROMs, not from volatile RAM.
	// In fact, the original code checks to see if code is executing in RAM,
	// and if it is, it disables the IPC's.  But this would be slow.  In the
	// Lisa emulator, the problem is that we have an MMU and we also have
	// virtual memory which pages things in and out of memory.  So I had to
	// change the way Generator's IPC's work.  For one thing it has to release
	// any IPC's that are no longer needed when the MMU releases or changes the
	// segment that they're built for.  Repeated calls to malloc and free would
	// fragment RAM and cause problems.  So instead I opted to replace the IPC
	// arrays with ones linked to the MMU pages and got rid of the linked list
	// and array based hash.
	//
	// Since this is the only function that ever does ipc--, we can save
	// some RAM by keeping an array of pointers to ipc's so we can walk
	// backwards when we need to. This potentially saves us RAM and access
	// speed in the IPC linked list space, though here we give up 32k or 64k,
	// but only for this function, and if it goes over it will alloc more.
	//
	// So that's an acceptable price to pay.
	//
	// (access speed is of course the overhead needed to also maintain a
	// previous IPC pointer. Memory depends on how many IPC's we wind up
	// with, and keeping the IPC structures small saves the L1/L2 caches
	// from reading in previous pointers that are unnecessary for the rest
	// of the code except for this function.)  -- Ray Arachelian.

    //int size = 0;
    t_iib *iib, *illegaliib; //*myiib,
	uint32 instrs = 0;
	uint16 required;
    uint32 xpc=pc|511;
    //int i;
     #ifdef DEBUG
        int dbx=debug_log_enabled;
     #endif

	mmu_trans_t *mmu_trn;
    t_ipc *ipc; // *opc; myipc,
	//t_ipc **ipcs; // since this is the only function that walks backwards in the IPC's we
	// collect them here temporarily in an array of pointers.  Since the malloc should be
	// immediately followed by a free (reallocs granted, yes), it shouldn't fragment ram too much.

    //int ipcptr=0;

	t_ipc_table *rettable, *table;


	uint32 ix=0; // index to the ipcs.

    abort_opcode=0;

    //DEBUG_LOG(1000,"PC I was passed: %08x",pc);

	pc &= 0x00ffffff;
    //if (pc&1) {fprintf(buglog,"%s:%s:%d odd pc!",__FILE__,__FUNCTION__,__LINE__); EXIT(88);}

    #ifdef DEBUG
    DEBUG_LOG(1000,"PC I filtered to 24 bit: %08x",pc);
    //check_iib();
    checkcontext(context,"pre-entry to cpu68k_makeipc");
    #endif
    //DEBUG_LOG(1000,"allocating %ld ipcs\n",ipcs_to_get);
    if (!ipcs) {
                 DEBUG_LOG(205,"Didn't have any IPCS, so allocating now.");
                 ipcs=(t_ipc **)malloc(sizeof(t_ipc *) * ipcs_to_get);
               }

	if (!ipcs) {ui_err("Out of memory in makeipclist trying to get ipc pointers!");}


	mmu_trn=&mmu_trans[(pc>>9) & 32767];
    if (mmu_trn->readfn==bad_page) return NULL;



	ipc=NULL; // Make sure it's clear
    //#ifdef DEBUG
    //check_iib();
    //#endif
    DEBUG_LOG(200,"ipc is now %p (should be null) at pc %06x max %06x",ipc,pc,xpc);



    //  DEBUG_LOG(1000,"Is mmu_trn there?  Is it's table there?");

    if (mmu_trn && mmu_trn->table)
	{
		// Get the pointer to the IPC.
		ipc = &(mmu_trn->table->ipc[((pc>>1) & 0xff)]);
        DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);
    }
    //if (pc&1) {fprintf(buglog,"odd pc!"); EXIT(12);}


    //check_iib();

    if (!ipc)
	{
        DEBUG_LOG(1000,"Nope - calling get_ipct()");
		mmu_trn->table=get_ipct(); // allocate an ipc table for this mmu_t
        table=mmu_trn->table;
        if (!table) {EXIT(21,0,"Couldn't get IPC Table! Doh!");}
        if (pc&1) {EXIT(14,0,"odd pc!");}

        //check_iib();

        if (mmu_trn && table)
		{
			// ipc points to the MMU translation table entry for this page.
            ipc = &(table->ipc[((pc>>1) & 0xff)]);
            DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);
            if (!ipc) {EXIT(501,0,"cpu68k_makeipclist: But! ipc is null!"); }
            if (pc&1) {EXIT(501,0,"odd pc!");}
		}
        else
        {EXIT(502,0,"Let the bodies hit the floor!\nLet the bodies hit the floor!\nLet the bodies hit the floor!\n\n  Either mmu_trn or table is null!");}
	}

    //check_iib();
    //if (pc&1) {fprintf(buglog,"odd pc!"); EXIT(16);}

        if ( !ipc)
                {
                    EXIT(17,0,"ipc=NULL\n1. Something's got to give 2. Something's got to give. 3. Something's got to give 4. Something's got to give.\nNOW!");
                }

    //check_iib();

    DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);

    //if (pc&1) {fprintf(buglog,"odd pc!"); EXIT(18);}
    table=(mmu_trn->table); // we might not yet have table until we get here.

    rettable=table;                     // save this to return, we now have a pointer to the table we want to return.
	//list->pc = pc;
	table->t.clocks = 0;
    //check_iib();
    //if (pc&1) {fprintf(buglog,"odd pc!"); EXIT(19);}
    illegaliib=cpu68k_iibtable[0x4afc]; // cache for later.


    if ( !ipc)
                {
                    EXIT(20,0,"ipc=NULL\n1. Something's got to give 2. Something's got to give. 3. Something's got to give 4. Something's got to give.\nNOW!");
                }

    //check_iib();
    ////list->norepeat = 0;
    //if (pc&1) {fprintf(buglog,"odd pc!"); EXIT(20);}
	xpc=pc | 0x1ff; // set the end to the end of the current page. (was 1fe)
    //DEBUG_LOG(205,"ipc is %s",(!ipc)?"null":"ok");
    instrs=0;
	do {
        uint16 opcode;

		instrs++;
        //check_iib();
        //DEBUG_LOG(205,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);

		// Get the IIB, if it's NULL, then get the IIB for an illegal instruction.
     #ifdef DEBUG
     //   dbx=debug_log_enabled; debug_log_enabled=0;
     #endif

       abort_opcode=2;
       opcode=fetchword(pc);
       if (abort_opcode==1) ALERT_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");

       if (abort_opcode==1)     // flush mmu and retry once
       {   abort_opcode=2;
           mmuflush(0x2000);
           opcode=fetchword(pc);
           if (abort_opcode==1) ALERT_LOG(0,"DANGER! GOT abort_opcode=1 the 2nd time!");
           if (abort_opcode==1)  return 0;                        }  // retry failed, abort it.
     #ifdef DEBUG
     //   debug_log_enabled=dbx;
     #endif
        abort_opcode=0;
        iib=cpu68k_iibtable[opcode]; // iib =  myiib ? myiib : illegaliib;  *** REMOVED DUE TO ILLEGAL OPCODES *****
		if (!iib) return 0;
//        if (!iib) {EXIT(53,0,"There's no proper IIB for the possibly illegal instruction opcode %04x @ pc=%08x\n",opcode,pc);}


        #ifdef DEBUG
        {
            int i=0;
            uint16 x=opcode;
            while (x==0)
            {
     #ifdef DEBUG
     //   dbx=debug_log_enabled; debug_log_enabled=0;
     #endif

               abort_opcode=2;
               x=fetchword(pc+i);
               if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 unexpectedly");

               if (abort_opcode==1)     // flush mmu and retry once
               {
                   fprintf(buglog,"Got abort opcode on accessing:%08x\n",pc+i);
                   fflush(buglog);
                   abort_opcode=2;   mmuflush(0x2000);
                   fprintf(buglog,"Got abort opcode on accessing:%08x\n",pc+i); fflush(buglog);
                   x=fetchword(pc+i);
                   if (abort_opcode==1) DEBUG_LOG(0,"DANGER! GOT abort_opcode=1 2nd time unexpectedly");
                   if (abort_opcode==1)  return rettable;      // retry failed, abort it.
                }
                abort_opcode=0;
               i+=2;
     #ifdef DEBUG
     //   debug_log_enabled=dbx;
     #endif
               #ifdef BOOGABOOGA
               if (i>32)
               {
                fflush(buglog);
                debug_log_enabled=1;
                fprintf(buglog,"\n\n\n\n\n***BUG BE HERE! RAM is now zeros for last 32 bytes, it's time to go bye bye.\n");
                fprintf(buglog,"sio from %08x  ",CHK_MMU_A_TRANS((1+segment1+segment2),pc));
                fprintf(buglog,"pc=%08x %d/%d/%d  abort_opcode:%d \n\n\n\n",pc,segment1,segment2,start,abort_opcode);
                fprintf(buglog,"%s\n",chk_mtmmu(pc, 0));

                fprintf(stderr,"\n\n\n\n\n***BUG BE HERE! RAM is now zeros for last 32 bytes, it's time to go bye bye.\n");
                fprintf(stderr,"sio from %08x  ",CHK_MMU_A_TRANS((1+segment1+segment2),pc));
                fprintf(stderr,"pc=%08x %d/%d/%d  abort_opcode:%d \n\n\n\n",pc,segment1,segment2,start,abort_opcode);
                fprintf(stderr,"%s\n",chk_mtmmu(pc, 0));

                fprintf(stdout,"\n\n\n\n\n***BUG BE HERE! RAM is now zeros for last 32 bytes, it's time to go bye bye.\n");
                fprintf(stdout,"sio from %08x  ",CHK_MMU_A_TRANS((1+segment1+segment2),pc));
                fprintf(stdout,"pc=%08x %d/%d/%d  abort_opcode:%d \n\n\n\n",pc,segment1,segment2,start,abort_opcode);
                fprintf(stdout,"%s\n",chk_mtmmu(pc, 0));
                fflush(stderr);
                fflush(stdout);
                fflush(buglog);

                #ifdef DEBUG
                 dumpallmmu();
                #endif
                fflush(buglog); fflush(stdout);

                //EXIT(51);               // this breaks Xenix, so perhaps I should let it continue.
               }
               #endif
            }
        }
        #endif

        // #ifdef DEBUG
        // fflush(buglog);
        // if (debug_log_enabled) fprintf(buglog,"%s:%s:%d processing ipc for instruction #%d opcode:%04x @ %d/%08x (%d/%d/%d)\n",__FILE__,__FUNCTION__,__LINE__,instrs-1,opcode,context,pc,segment1,segment2,start);
        // fflush(buglog);
        // #endif

        if (!iib) {EXIT(53,0,"There's no proper IIB for the possibly illegal instruction opcode %04x @ pc=%08x\n",opcode,pc);}
        if ( !ipc)  {EXIT(54,0,"Have a cow man! ipc=NULL\n"); }

        //DEBUG_LOG(200,"ipc is %s",(!ipc)?"null":"ok");


     #ifdef DEBUG
        dbx=debug_log_enabled; debug_log_enabled=0;
     #endif

        #if DEBUG
         if (!iib) DEBUG_LOG(0,"about to pass NULL IIB");
         if (!ipc)  DEBUG_LOG(0,"about to pass NULL IIB");
        #endif

        cpu68k_ipc(pc, iib, ipc);
        table->t.clocks += iib->clocks;

        if (abort_opcode==1) return rettable;       // got MMU Exception

     #ifdef DEBUG
        debug_log_enabled=dbx;
     #endif




        DEBUG_LOG(200,"******* for next ip at instrs %d, pc=%08x, opcode=%04x",instrs,pc,opcode);
        //cpu68k_printipc(ipc);
        pc += (iib->wordlen) << 1;

        //if (pc&1) {fprintf(buglog,"odd pc in cpu68k opcode handling!!!"); EXIT(23);}

        //fprintf(buglog,"%s:%s:%d ipc is now %p at (new)pc %06x max %06x",__FILE__,__FUNCTION__,__LINE__,ipc,pc,xpc);

        #ifdef DEBUG
        {
          char text[1024];
          diss68k_gettext(ipc, text);
          DEBUG_LOG(200,"ipc ipc ipc opcode I got %08x :%s instr#:%d",pc,text,instrs);
        }
        #endif


		// grow the list of ipcs if we need to.
		if (instrs>=ipcs_to_get)
		{
            EXIT(24,0,"Welcome to the realms of chaos! I'm dealing with over %d instructions, %d ipcs! %d/%d/%d pc=%d/%08x",instrs,ipcs_to_get,segment1,segment2,start,context,pc);
            pc24=pc;
		}

        DEBUG_LOG(200,"Copying ipc to ipcs buffer");
        ipcs[instrs-1]=ipc; // copy pointer to ipcs buffer
        //check_iib();
        DEBUG_LOG(200,"XPC I set as limit: %08x pc is %08x",xpc,pc);
        // ******** IS THIS WHAT'S FUCKED? *****************
		if (pc>xpc) // did we step over the page? If so, setup for the next page.
		{
			xpc=pc | 0x1ff;
            DEBUG_LOG(200,"XPC I set as limit: %08x pc is %08x",xpc,pc);
            //if (pc&1) {fprintf(buglog,"odd pc!"); EXIT(25);}

            DEBUG_LOG(200,"XPC I set as limit: %08x pc is %08x",xpc,pc);
            mmu_trn=&mmu_trans[(pc>>9) & 32767];
            table=mmu_trn->table; // we might not yet have table until we get here.
            if ( !table)
            {
              DEBUG_LOG(1000,"Nope - calling get_ipct()");
              mmu_trn->table=get_ipct(); // allocate an ipc table for this mmu_t
              table=mmu_trn->table;

              if (!table) {
                           EXIT(99,0,"Couldnt get IPC Table! Doh!\n");}

              //if (pc&1) {DEBUG_LOG(200,"odd pc!"); EXIT(26);}
            }

            DEBUG_LOG(200,"ipc is %s",
                     (!ipc ?"null":"ok"));

        //check_iib();
            ipc = &(table->ipc[((pc>>1) & 0xff)]);  //setup next ipc
            DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);
			//ipcs[instrs-1]=ipc; // copy pointer to ipcs buffer
		}
        else       // No we didn't go over the MMU page limited yet, it's cool
        {
                // check_iib();

                if (!mmu_trn->table)
                {

                  mmu_trn->table=get_ipct();
                  table=mmu_trn->table;

                 // check_iib();

                  if (!table) {EXIT(27,0,"Couldnt get IPC Table! Doh!");}
                  //ipc = &(mmu_trn->table->ipc[((pc>>1) & 0xff)]);
                  //myiib=cpu68k_iibtable[opcode]; iib=myiib; // iib =  myiib ? myiib : illegaliib;
                  //////cpu68k_ipc(pc, iib, ipc);
                }
                //check_iib();
                DEBUG_LOG(200,"Getting IPC from mmu_trn->table->ipc");

                ipc = &(table->ipc[((pc>>1) & 0xff)]); // ipc points to the ipc in mmu_trans
                DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);
        }
        //check_iib();

        DEBUG_LOG(200,"ipc is %s",(!ipc)?"null":"ok");
        DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);
     }
	while (!iib->flags.endblk);
    //check_iib();
    DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);
    DEBUG_LOG(200,"\n\n *** IMPORTANT, WE DID %d instructions! pc=%08x xpc=%08x**** \n\n",instrs,pc,xpc);


    // Do we need this?  correct final ipc out of the loop.
    //iib=cpu68k_iibtable[(fetchword(pc))]; // iib =  myiib ? myiib : illegaliib;  *** REMOVED DUE TO ILLEGAL OPCODES *****
    //if ( !iib ) {DEBUG_LOG(205,"Worse yet, there's no proper IIB for the illegal instruction opcode 0x4afc!"); EXIT(555);}
    //if ( !ipc )  { DEBUG_LOG(205,"I'm about to do something really stupid.(ipc=NULL)\n");  EXIT(1);  }
    //
    //cpu68k_ipc(pc, iib, ipc);
    table->t.clocks += iib->clocks;
    DEBUG_LOG(200,"hangover: %08x,%d",pc,instrs);cpu68k_printipc(ipc);

    //table->ipc[((pc>>1) & 0xff)]=NULL;      // make sure next one is not set up.
    ////check_iib();
    //  *(int *)ipc = 0; // wtf?  why set it to null? indicate end of list. yup. yup.
    if (instrs == 2)
    {

        if (pc&1) {EXIT(28,0,"odd pc!");}
        DEBUG_LOG(200,"*~*~*~*~*~*~ in 2instrs ipc is now %p at pc %06x max %06x",ipc,pc,xpc);
        ipc=ipcs[instrs-1-1]; //ipc--
        DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);


        if (iib->mnemonic == i_Bcc && ipc->src == xpc)
                { // RA list->pc <- xpc
                 /* we have a 2-instruction block ending in a branch to start */
                  ipc = ipcs[instrs-1+1]; //ipc++
                  DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x instruction # %d",ipc,pc,xpc,instrs-1);
                  // Get the IIB, if it's NULL, then get the IIB for an illegal instruction.


                 iib=cpu68k_iibtable[ipc->opcode];
                 if ( !iib) {DEBUG_LOG(0,"Boink! Got null IIB for this opcode=%04x",ipc->opcode); }
                 //iib =  myiib ? myiib : illegaliib;
                 //if (iib->mnemonic == i_TST || iib->mnemonic == i_CMP)
                 //{
                  /* it's a tst/cmp and then a Bcc */
				//				if (!(iib->stype == dt_Ainc || iib->stype == dt_Adec)) {
                 /* no change could happen during the block */
                //                  list->norepeat = 1;  // RA, I think we can remove this.
				//			}
               //}
                }
	}
    //check_iib();

	// This walks the ipc's backwards... this is why we need the ipcs array.
	// otherwise we'd need to set up a set of previous and next link pointers
	// and eat more valuable ram.  this too eats ram, but only once, and about
	// half of what would needed (with a two way linked list, those pointers would
	// have been wasted as they're only used here!)  We could walk the page array
	// of ipcs, but that's slower and it sucks, etc.

	//	ipc = ((t_ipc *) (list + 1)) + instrs - 1;

    ipc->next=NULL; // next pointer of last IPC is always null as there is no next one yet.
    DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);

    //check_iib();
    ix=instrs ; /*****************/
	//	ipc = ipcs[ix];
	required = 0x1F;              /* all 5 flags need to be correct at end */
    DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x",ipc,pc,xpc);

    DEBUG_LOG(200,"**** About to correct ipc's: %d instructions **** \n\n",instrs);


  // while ( ix--)
  // {
  //   check_iib();
  //   ipc=ipcs[ix];
  //   DEBUG_LOG(205,"---dumping ipc[%d]",ix);
  //   if ( ipc) {
  //      cpu68k_printipc(ipc);
  //   }
  //   else DEBUG_LOG(205,"---IS NULL");
  // }
   //check_iib();
   DEBUG_LOG(250,"------------- correcting -------");
   ix=instrs; /*****************/
    while(ix--)
	{

        //check_iib();
        DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x ix=%d",ipc,pc,xpc,ix);
        ipc=ipcs[ix];

        DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x ix=%d",ipc,pc,xpc,ix);
        if ( !ipc)
        {
            EXIT(29,0,"Null ipc, bye");
        }

        //check_iib();

        ipc->set &= required;
		required &= ~ipc->set;
		required |= ipc->used;

        if (ipc->set)
            { ipc->function = cpu68k_functable[(ipc->opcode << 1) + 1]; }
        else
            { ipc->function = cpu68k_functable[ipc->opcode << 1];  }


        if (!ipc->function)
             {
              EXIT(3,0,"Null IPC fn returned for opcode:%04x ix=%d of %d instrs",ipc->opcode,ix,instrs);
              // cpu68k_printipc(ipc);
             }

       // cpu68k_printipc(ipc);
        DEBUG_LOG(200,"ipc is now %p at pc %06x max %06x ix=%d",ipc,pc,xpc,ix);
        if (ix) ipcs[ix-1]->next=ipc; // previous ipc's next link points to current ipc.

        //DEBUG_LOG(200,"Corrected ipc #%d ptr %p",ix,ipc);

    }

    // Sanity check - something is running away somewhere - likely clobbering the iib table and some ipc's...
    // remove this after we are done.
    for (ix=0; ix<instrs; ix++)
		if (ipcs[ix]->function==NULL)
		{
          //  check_iib();
            ipc=ipcs[ix];
            EXIT(6,0,"FATAL ipc with null fnction at index %d-> used:%d, set:%d, opcode %04x, len %02x, src %08x, dst %08x\n",
                ix, ipc->used, ipc->set, ipc->opcode, ipc->wordlen, ipc->src, ipc->dst);
		}


	// even though our page is only 512 bytes, we only return the 1st table
	// to the page we started with.  That's why we have both rettable and table.
	// the other ones do allocate space in the tables of the followed pages, but
	// we only have to return the top one.
    //#ifdef DEBUG
    //checkcontext(context,"exit from cpu68k_makeipc");
    //#endif
	//free(ipcs); // must call this before exiting! <- made this static.  Sort of
    //DEBUG_LOG(200,"Done with ipc corrections\n\n");
    //
    //#ifdef DEBUG
    //check_iib();
    //#endif
    //
    //DEBUG_LOG(200,"returning ipctable. ipc is now %p at pc %06x max %06x ix=%d",ipc,pc,xpc,ix);

    return rettable;
}

void cpu68k_endfield(void)
{
//    cpu68k_clocks = 0;
}


// Before we call this, Lisa memory must be allocated, ROM mem must be allocated and loaded!
// the MALLOC pointers must be set to NULL the first time around, otherwise we'd attempt to free garbage.
// these must be properly done in main!

void cpu68k_reset(void)
{
    //int i;


	regs.pc = fetchlong(4);
	regs.regs[15] = fetchlong(0);

    DEBUG_LOG(205,"CPU68K Reset: Context: %d, PC set to %08x, SSP set to %08x\n",context,regs.pc,regs.regs[15]);

	regs.sr.sr_int = 0;
	regs.sr.sr_struct.s = 1;      /* Supervisor mode */
	regs.stop = 0;

	cpu68k_frames = 0;            /* Number of frames */
    virq_start=FULL_FRAME_CYCLES;
    fdir_timer=-1;
    cpu68k_clocks_stop=ONE_SECOND;
    cpu68k_clocks=0;
    cops_event=-1;
    tenth_sec_cycles =TENTH_OF_A_SECOND;



    // for (i=0; i<MAX_IPCT_MALLOCS; i++)
    //    if (ipct_mallocs[i]!=NULL) {free(ipct_mallocs[i]); ipct_mallocs[i]=NULL;}
    // iipct_mallocs=0;
    //
    // init_ipct_allocator();
	floppy_6504_wait=0;
	segment1=0; segment2=0;
}


