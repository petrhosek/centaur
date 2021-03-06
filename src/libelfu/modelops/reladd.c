/* This file is part of centaur.
 *
 * centaur is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * centaur is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with centaur.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <libelfu/libelfu.h>


static ElfuScn* cloneScn(ElfuScn *ms)
{
  ElfuScn *newscn;

  assert(ms);

  newscn = elfu_mScnAlloc();
  if (!newscn) {
    return NULL;
  }

  newscn->shdr = ms->shdr;

  if (ms->databuf) {
    void *newbuf = malloc(ms->shdr.sh_size);
    if (!newbuf) {
      ELFU_WARN("cloneScn: Could not allocate memory for new data buffer.\n");
      free(newscn);
      return NULL;
    }

    memcpy(newbuf, ms->databuf, ms->shdr.sh_size);
    newscn->databuf = newbuf;
  }

  newscn->oldptr = ms;

  return newscn;
}



static ElfuScn* insertSection(ElfuElf *me, ElfuElf *mrel, ElfuScn *oldscn)
{
  ElfuScn *newscn = NULL;
  GElf_Addr injAddr;
  GElf_Off injOffset;
  ElfuPhdr *injPhdr;

  if (!(oldscn->shdr.sh_flags & SHF_ALLOC)) {
    ELFU_WARN("insertSection: Skipping non-memory section %s (type %d flags %u).\n",
              elfu_mScnName(mrel, oldscn),
              oldscn->shdr.sh_type,
              (unsigned)oldscn->shdr.sh_flags);
    goto ERROR;
  } else {
    newscn = cloneScn(oldscn);
    if (!newscn) {
      return NULL;
    }

    if (newscn->shdr.sh_type == SHT_NOBITS) {
      /* Expand this to SHT_PROGBITS, then insert as such. */

      assert(!newscn->databuf);

      newscn->databuf = malloc(newscn->shdr.sh_size);
      if (!newscn->databuf) {
        goto ERROR;
      }
      newscn->shdr.sh_type = SHT_PROGBITS;
    }

    injAddr = elfu_mLayoutGetSpaceInPhdr(me,
                                         newscn->shdr.sh_size,
                                         newscn->shdr.sh_addralign,
                                         newscn->shdr.sh_flags & SHF_WRITE,
                                         newscn->shdr.sh_flags & SHF_EXECINSTR,
                                         &injPhdr);

    if (!injPhdr) {
      ELFU_WARN("insertSection: Could not find a place to insert section.\n");
      goto ERROR;
    }

    ELFU_INFO("Inserting %s at address 0x%x...\n",
              elfu_mScnName(mrel, oldscn),
              (unsigned)injAddr);

    injOffset = injAddr - injPhdr->phdr.p_vaddr + injPhdr->phdr.p_offset;

    newscn->shdr.sh_addr = injAddr;
    newscn->shdr.sh_offset = injOffset;

    /* Insert section in child list, ordered by memory address */
    if (CIRCLEQ_EMPTY(&injPhdr->childScnList)
        || CIRCLEQ_LAST(&injPhdr->childScnList)->shdr.sh_addr < injAddr) {
      CIRCLEQ_INSERT_TAIL(&injPhdr->childScnList, newscn, elemChildScn);
    } else {
      ElfuScn *ms;
      CIRCLEQ_FOREACH(ms, &injPhdr->childScnList, elemChildScn) {
        if (injAddr < ms->shdr.sh_addr) {
          CIRCLEQ_INSERT_BEFORE(&injPhdr->childScnList, ms, newscn, elemChildScn);
          break;
        }
      }
    }


    /* Inject name */
    if (me->shstrtab) {
      size_t newnamelen;

      newnamelen = strlen("reladd") + 1;
      if (elfu_mScnName(mrel, oldscn)) {
        newnamelen += strlen(elfu_mScnName(mrel, oldscn));
      }

      char newname[newnamelen];

      strcpy(newname, "reladd");
      strcat(newname, elfu_mScnName(mrel, oldscn));

      newscn->shdr.sh_name = me->shstrtab->shdr.sh_size;

      if (elfu_mScnAppendData(me->shstrtab, newname, newnamelen)) {
        newscn->shdr.sh_name = 0;
      }
    }

    return newscn;
  }

  ERROR:
  if (newscn) {
    elfu_mScnDestroy(newscn);
  }
  return NULL;
}


static void* subScnAdd1(ElfuElf *mrel, ElfuScn *ms, void *aux1, void *aux2)
{
  ElfuScn *newscn;
  ElfuElf *me = (ElfuElf*)aux1;
  (void)aux2;


  switch(ms->shdr.sh_type) {
    case SHT_PROGBITS: /* 1 */
    case SHT_NOBITS: /* 8 */
      /* Ignore empty sections */
      if (ms->shdr.sh_size == 0) {
        break;
      }

      /* Find a place where it belongs and shove it in. */
      newscn = insertSection(me, mrel, ms);
      if (!newscn) {
        ELFU_WARN("mReladd: Could not insert section %s (type %d), skipping.\n",
                  elfu_mScnName(mrel, ms),
                  ms->shdr.sh_type);
      }
      break;
  }

  return NULL;
}


static void* subScnAdd2(ElfuElf *mrel, ElfuScn *ms, void *aux1, void *aux2)
{
  ElfuElf *me = (ElfuElf*)aux1;
  (void)aux2;

  switch(ms->shdr.sh_type) {
    case SHT_NULL: /* 0 */
    case SHT_PROGBITS: /* 1 */
    case SHT_SYMTAB: /* 2 */
    case SHT_STRTAB: /* 3 */
    case SHT_NOBITS: /* 8 */
      break;

    case SHT_RELA: /* 4 */
    case SHT_REL: /* 9 */
      /* Relocate. */
      if (elfu_mRelocate(me, elfu_mScnByOldscn(me, ms->infoptr), ms)) {
        return (void*)-1;
      }
      break;

    /* The next section types either do not occur in .o files, or are
     * not strictly necessary to process here. */
    case SHT_NOTE: /* 7 */
    case SHT_HASH: /* 5 */
    case SHT_DYNAMIC: /* 6 */
    case SHT_SHLIB: /* 10 */
    case SHT_DYNSYM: /* 11 */
    case SHT_INIT_ARRAY: /* 14 */
    case SHT_FINI_ARRAY: /* 15 */
    case SHT_PREINIT_ARRAY: /* 16 */
    case SHT_GROUP: /* 17 */
    case SHT_SYMTAB_SHNDX: /* 18 */
    case SHT_NUM: /* 19 */
    default:
      ELFU_WARN("mReladd: Skipping section %s (type %d).\n",
                elfu_mScnName(mrel, ms),
                ms->shdr.sh_type);
  }

  return NULL;
}



static void insertSymClone(ElfuElf *me, const ElfuScn *oldmsst, const ElfuSym *oldsym)
{
  GElf_Xword newsize;
  char *newbuf;
  ElfuScn *newscn = NULL;
  ElfuSym *newsym;
  char *oldsymname;

  assert(me);
  assert(oldmsst);
  assert(oldsym);

  /* If the old symbol pointed to a section, find its clone in the target */
  if (oldsym->scnptr) {
    newscn = elfu_mScnByOldscn(me, oldsym->scnptr);

    /* If we didn't copy the section referenced, we won't
     * copy this symbol either */
    if (!newscn) {
      return;
    }
  }

  /* If we don't have a symbol table, create one so we have somewhere to
   * write our new symbols to. */
  elfu_mSymtabAddGlobalDymtabIfNotPresent(me);

  /* Allocate memory for the cloned symbol */
  newsym = malloc(sizeof(*newsym));
  if (!newsym) {
    ELFU_WARN("insertSymClone: malloc() failed for newsym.\n");
    goto ERROR;
  }

  oldsymname = ELFU_SYMSTR(oldmsst, oldsym->name);

  /* Expand .strtab, append symbol name, link newsym to it */
  newsize = me->symtab->linkptr->shdr.sh_size + strlen(oldsymname) + 1;
  newbuf = realloc(me->symtab->linkptr->databuf, newsize);
  if (!newbuf) {
    ELFU_WARN("insertSymClone: realloc() failed for strtab.\n");
    goto ERROR;
  }

  me->symtab->linkptr->databuf = newbuf;

  newsym->name = me->symtab->linkptr->shdr.sh_size;

  strcpy(newbuf + newsym->name, oldsymname);

  me->symtab->linkptr->shdr.sh_size = newsize;


  /* Copy all other fields */
  newsym->scnptr = newscn;
  newsym->shndx = oldsym->shndx; /* If scnptr == NULL, this becomes relevant */
  newsym->bind = oldsym->bind;
  newsym->other = oldsym->other;
  newsym->size = oldsym->size;
  newsym->type = oldsym->type;
  newsym->value = oldsym->value;

  /* In executables, symbol addresses need to be in memory */
  if (newscn) {
    newsym->value += newscn->shdr.sh_addr;
  }

  /* Insert symbol */
  CIRCLEQ_INSERT_TAIL(&me->symtab->symtab.syms, newsym, elem);

  return;

  ERROR:
  if (newsym) {
    free(newsym);
  }
}

static void mergeSymtab(ElfuElf *me, const ElfuElf *mrel)
{
  ElfuSym *sym;

  assert(me);
  assert(mrel);

  CIRCLEQ_FOREACH(sym, &mrel->symtab->symtab.syms, elem) {
    insertSymClone(me, mrel->symtab, sym);
  }

  elfu_mSymtabFlatten(me);
}



int elfu_mReladd(ElfuElf *me, const ElfuElf *mrel)
{
  assert(me);
  assert(mrel);

  /* For each section in object file, guess how to insert it */
  elfu_mScnForall((ElfuElf*)mrel, subScnAdd1, me, NULL);

  mergeSymtab(me, mrel);

  /* Do relocations and other stuff */
  if (elfu_mScnForall((ElfuElf*)mrel, subScnAdd2, me, NULL)) {
    ELFU_WARN("elfu_mReladd: Reladd aborted. Target model is unclean.\n");
    return -1;
  }

  /* Re-layout to accommodate new contents */
  elfu_mLayoutAuto(me);

  return 0;
}
