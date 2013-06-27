#ifndef __LIBELFU_MODELOPS_H__
#define __LIBELFU_MODELOPS_H__

#include <elf.h>
#include <gelf.h>

#include <libelfu/types.h>


#define ELFU_SYMSTR(symtabscn, off) ((symtabscn)->linkptr->databuf + (off))


      int elfu_mSymtabLookupSymToAddr(ElfuElf *me, ElfuScn *msst, ElfuSym *sym, GElf_Addr *result);
    char* elfu_mSymtabSymToName(ElfuScn *msst, ElfuSym *sym);
 ElfuSym* elfu_mSymtabIndexToSym(ElfuScn *msst, GElf_Word entry);
GElf_Addr elfu_mSymtabLookupAddrByName(ElfuElf *me, ElfuScn *msst, char *name);
     void elfu_mSymtabFlatten(ElfuElf *me);
     void elfu_mSymtabAddGlobalDymtabIfNotPresent(ElfuElf *me);


typedef void* (PhdrHandlerFunc)(ElfuElf *me, ElfuPhdr *mp, void *aux1, void *aux2);
    void* elfu_mPhdrForall(ElfuElf *me, PhdrHandlerFunc f, void *aux1, void *aux2);
   size_t elfu_mPhdrCount(ElfuElf *me);
ElfuPhdr* elfu_mPhdrByAddr(ElfuElf *me, GElf_Addr addr);
ElfuPhdr* elfu_mPhdrByOffset(ElfuElf *me, GElf_Off offset);
     void elfu_mPhdrLoadLowestHighest(ElfuElf *me,
                                      ElfuPhdr **lowestAddr,
                                      ElfuPhdr **highestAddr,
                                      ElfuPhdr **lowestOffs,
                                      ElfuPhdr **highestOffsEnd);
     void elfu_mPhdrUpdateChildOffsets(ElfuPhdr *mp);
ElfuPhdr* elfu_mPhdrAlloc();
     void elfu_mPhdrDestroy(ElfuPhdr* mp);


typedef void* (SectionHandlerFunc)(ElfuElf *me, ElfuScn *ms, void *aux1, void *aux2);
    void* elfu_mScnForall(ElfuElf *me, SectionHandlerFunc f, void *aux1, void *aux2);
   size_t elfu_mScnCount(ElfuElf *me);
   size_t elfu_mScnIndex(ElfuElf *me, ElfuScn *ms);
 ElfuScn* elfu_mScnByOldscn(ElfuElf *me, ElfuScn *oldscn);
    char* elfu_mScnName(ElfuElf *me, ElfuScn *ms);
ElfuScn** elfu_mScnSortedByOffset(ElfuElf *me, size_t *count);
      int elfu_mScnAppendData(ElfuScn *ms, void *buf, size_t len);
 ElfuScn* elfu_mScnAlloc();
     void elfu_mScnDestroy(ElfuScn* ms);


ElfuElf* elfu_mElfAlloc();
    void elfu_mElfDestroy(ElfuElf* me);


GElf_Addr elfu_mLayoutGetSpaceInPhdr(ElfuElf *me, GElf_Word size,
                                     GElf_Word align, int w, int x,
                                     ElfuPhdr **injPhdr);
int elfu_mLayoutAuto(ElfuElf *me);


int elfu_mDynLookupPltAddrByName(ElfuElf *me, char *name, GElf_Addr *result);
int elfu_mDynLookupReldynAddrByName(ElfuElf *me, char *name, GElf_Addr *result);


int elfu_mRelocate(ElfuElf *metarget, ElfuScn *mstarget, ElfuScn *msrt);


int elfu_mCheck(ElfuElf *me);


void elfu_mDumpPhdr(ElfuElf *me, ElfuPhdr *mp);
void elfu_mDumpScn(ElfuElf *me, ElfuScn *ms);
void elfu_mDumpElf(ElfuElf *me);


ElfuElf* elfu_mFromElf(Elf *e);
    void elfu_mToElf(ElfuElf *me, Elf *e);

 int elfu_mReladd(ElfuElf *me, const ElfuElf *mrel);

void elfu_mDetour(ElfuElf *me, GElf_Addr from, GElf_Addr to);

#endif
