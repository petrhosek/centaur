#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <libelfu/libelfu.h>


/* Meta-functions */

void* elfu_mPhdrForall(ElfuElf *me, PhdrHandlerFunc f, void *aux1, void *aux2)
{
  ElfuPhdr *mp;

  CIRCLEQ_FOREACH(mp, &me->phdrList, elem) {
    ElfuPhdr *mp2;
    void *rv = f(me, mp, aux1, aux2);
    if (rv) {
      return rv;
    }

    CIRCLEQ_FOREACH(mp2, &mp->childPhdrList, elemChildPhdr) {
      void *rv = f(me, mp2, aux1, aux2);
      if (rv) {
        return rv;
      }
    }
  }

  return NULL;
}




/* Counting */

static void* subCounter(ElfuElf *me, ElfuPhdr *mp, void *aux1, void *aux2)
{
  size_t *i = (size_t*)aux1;
  (void)aux2;

  *i += 1;

  /* Continue */
  return NULL;
}

size_t elfu_mPhdrCount(ElfuElf *me)
{
  size_t i = 0;

  assert(me);

  elfu_mPhdrForall(me, subCounter, &i, NULL);

  return i;
}




/* Finding by exact address/offset */

static void* subFindLoadByAddr(ElfuElf *me, ElfuPhdr *mp, void *aux1, void *aux2)
{
  GElf_Addr addr = *(GElf_Addr*)aux1;
  (void)aux2;

  if (mp->phdr.p_type == PT_LOAD
      && FULLY_OVERLAPPING(mp->phdr.p_vaddr, mp->phdr.p_memsz, addr, 1)) {
    return mp;
  }

  /* Continue */
  return NULL;
}

ElfuPhdr* elfu_mPhdrByAddr(ElfuElf *me, GElf_Addr addr)
{
  return elfu_mPhdrForall(me, subFindLoadByAddr, &addr, NULL);
}


static void* subFindLoadByOffset(ElfuElf *me, ElfuPhdr *mp, void *aux1, void *aux2)
{
  GElf_Off offset = *(GElf_Off*)aux1;
  (void)aux2;

  if (mp->phdr.p_type == PT_LOAD
      && FULLY_OVERLAPPING(mp->phdr.p_offset, mp->phdr.p_filesz, offset, 1)) {
    return mp;
  }

  /* Continue */
  return NULL;
}

ElfuPhdr* elfu_mPhdrByOffset(ElfuElf *me, GElf_Off offset)
{
  return elfu_mPhdrForall(me, subFindLoadByOffset, &offset, NULL);
}




/* Find lowest/highest address/offset */

void elfu_mPhdrLoadLowestHighest(ElfuElf *me,
                                 ElfuPhdr **lowestAddr, ElfuPhdr **highestAddr,
                                 ElfuPhdr **lowestOffs, ElfuPhdr **highestOffsEnd)
{
  ElfuPhdr *mp;

  assert(me);
  assert(lowestAddr);
  assert(highestAddr);
  assert(lowestOffs);
  assert(highestOffsEnd);

  *lowestAddr = NULL;
  *highestAddr = NULL;
  *lowestOffs = NULL;
  *highestOffsEnd = NULL;

  /* Find first and last LOAD PHDRs.
   * Don't compare p_memsz - segments don't overlap in memory. */
  CIRCLEQ_FOREACH(mp, &me->phdrList, elem) {
    if (mp->phdr.p_type != PT_LOAD) {
      continue;
    }
    if (!*lowestAddr || mp->phdr.p_vaddr < (*lowestAddr)->phdr.p_vaddr) {
      *lowestAddr = mp;
    }
    if (!*highestAddr || mp->phdr.p_vaddr > (*highestAddr)->phdr.p_vaddr) {
      *highestAddr = mp;
    }
    if (!*lowestOffs || mp->phdr.p_offset < (*lowestOffs)->phdr.p_offset) {
      *lowestOffs = mp;
    }
    if (!*highestOffsEnd
        || (OFFS_END(mp->phdr.p_offset,
                     mp->phdr.p_filesz)
            > OFFS_END((*highestOffsEnd)->phdr.p_offset,
                       (*highestOffsEnd)->phdr.p_filesz))) {
      *highestOffsEnd = mp;
    }
  }
}




/* Layout update */

void elfu_mPhdrUpdateChildOffsets(ElfuPhdr *mp)
{
  ElfuScn *ms;
  ElfuPhdr *mpc;

  assert(mp);
  assert(mp->phdr.p_type == PT_LOAD);

  CIRCLEQ_FOREACH(mpc, &mp->childPhdrList, elemChildPhdr) {
    mpc->phdr.p_offset = mp->phdr.p_offset + (mpc->phdr.p_vaddr - mp->phdr.p_vaddr);
  }

  CIRCLEQ_FOREACH(ms, &mp->childScnList, elemChildScn) {
    ms->shdr.sh_offset = mp->phdr.p_offset + (ms->shdr.sh_addr - mp->phdr.p_vaddr);
  }
}



/*
 * Allocation, destruction
 */

ElfuPhdr* elfu_mPhdrAlloc()
{
  ElfuPhdr *mp;

  mp = malloc(sizeof(ElfuPhdr));
  if (!mp) {
    ELFU_WARN("mPhdrAlloc: malloc() failed for ElfuPhdr.\n");
    return NULL;
  }

  memset(mp, 0, sizeof(*mp));

  CIRCLEQ_INIT(&mp->childScnList);
  CIRCLEQ_INIT(&mp->childPhdrList);

  return mp;
}

void elfu_mPhdrDestroy(ElfuPhdr* mp)
{
  ElfuPhdr *mp2;
  ElfuScn *ms;

  assert(mp);

  CIRCLEQ_FOREACH(mp2, &mp->childPhdrList, elemChildPhdr) {
    CIRCLEQ_REMOVE(&mp->childPhdrList, mp2, elemChildPhdr);
    elfu_mPhdrDestroy(mp2);
  }

  CIRCLEQ_FOREACH(ms, &mp->childScnList, elemChildScn) {
    CIRCLEQ_REMOVE(&mp->childScnList, ms, elemChildScn);
    elfu_mScnDestroy(ms);
  }

  free(mp);
}
