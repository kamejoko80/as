/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2018  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#ifndef _ELFINTERNAL_H_
#define _ELFINTERNAL_H_
/* ============================ [ INCLUDES  ] ====================================================== */
#include <stdlib.h>
#include "vfs.h"
#include "elf.h"
#include "elfloader.h"
#include "asdebug.h"
/* ============================ [ MACROS    ] ====================================================== */
#define ELF32_MAGIC  0xEEFF3322
#define ELF64_MAGIC  0xEEFF6644

#define ELF_DYNSYM              ".dynsym"      /* dynamic symbol table */
/* ============================ [ TYPES     ] ====================================================== */
typedef struct
{
	void       *addr;
	char* name;
}ELF32_SymtabType;

typedef struct
{
	uint32_t magic;
	uint32_t vstart_addr;
	uint32_t size;
	uint32_t nsym;
	void* space;
	int (*entry)(int argc, char* argv[]);
	ELF32_SymtabType* symtab;
}ELF32_ObjectType;
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void* ELF32_Load(void* elfFile);
int ELF32_Relocate(ELF32_ObjectType *elfObj, Elf32_Rel *rel, Elf32_Addr sym_val);
void* ELF32_LookupSymbol(ELF32_ObjectType *elfObj, const char *symbol);
void ELF32_Close(ELF32_ObjectType *elfObj);

void* ELF_FindSymbol(const char* name);

#endif /* _ELFINTERNAL_H_ */
