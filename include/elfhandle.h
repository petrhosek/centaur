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

#ifndef __ELFHANDLE_H__
#define __ELFHANDLE_H__

#include <libelf.h>

/*!
 * A simple pair of a file descriptor and a libelf handle,
 * used to simplify elfucli.
 */
typedef struct {
  int fd;   /*!< File handle */
  Elf *e;   /*!< libelf handle */
} ELFHandles;


void openElf(ELFHandles *h, char *fn, Elf_Cmd elfmode);
void closeElf(ELFHandles *h);

#endif
