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
/* ============================ [ INCLUDES  ] ====================================================== */
#include "elfinternal.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_ELF32 1
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
static boolean ELF32_GetVirtualAddress(void* elfFile, Elf32_Addr *vstart_addr, Elf32_Addr *vend_addr)
{
	uint32_t i;
	boolean has_vstart;
	Elf32_Ehdr *fileHdr = elfFile;
	Elf32_Phdr *phdr = elfFile + fileHdr->e_phoff;

	has_vstart = FALSE;
	for(i=0; i < fileHdr->e_phnum; i++)
	{
		if(phdr[i].p_type != PT_LOAD)
		{
			continue;
		}
		ASLOG(ELF32, "LOAD segment: %d, 0x%08X, 0x%08x\n",
				i, phdr[i].p_vaddr, phdr[i].p_memsz);
		if(phdr[i].p_memsz < phdr[i].p_filesz)
		{
			ASLOG(ERROR, "invalid elf: segment %d: p_vaddr: %d, p_memsz: %d\n",
					i, phdr[i].p_vaddr, phdr[i].p_memsz);
			has_vstart = FALSE;
			break;
		}
		if (!has_vstart)
		{
			*vstart_addr = phdr[i].p_vaddr;
			*vend_addr = phdr[i].p_vaddr + phdr[i].p_memsz;
			has_vstart = TRUE;
			if (*vend_addr < *vstart_addr)
			{
				ASLOG(ERROR, "invalid elf: segment %d: p_vaddr: %d, p_memsz: %d\n",
						i, phdr[i].p_vaddr, phdr[i].p_memsz);
				has_vstart = FALSE;
				break;
			}
		}
		else
		{
			if (phdr[i].p_vaddr < *vend_addr)
			{
				ASLOG(ERROR, "invalid elf: segment should be sorted and not overlapped\n");
				has_vstart = FALSE;
				break;
			}
			if (phdr[i].p_vaddr > *vend_addr + 16)
			{
				/* There should not be too much padding in the object files. */
				ASWARNING("too much padding before segment %d\n", i);
			}

			*vend_addr = phdr[i].p_vaddr + phdr[i].p_memsz;
			if (*vend_addr < phdr[i].p_vaddr)
			{
				ASLOG(ERROR, "invalid elf: "
						"segment %d address overflow\n", i);
				has_vstart = FALSE;
				break;
			}
		}
	}

	if(*vstart_addr >= *vend_addr)
	{
		ASLOG(ERROR, "invalid eld: start=%08X end=%08X\n",
				*vstart_addr, *vend_addr);
		has_vstart = FALSE;
	}

	return has_vstart;
}
static boolean ELF32_LoadObject(void* elfFile,ELF32_ObjectType* elfObj)
{
	boolean r = TRUE;
	uint32_t i;
	Elf32_Ehdr *fileHdr = elfFile;
	Elf32_Phdr *phdr = elfFile + fileHdr->e_phoff;
	Elf32_Shdr *shdr = elfFile + fileHdr->e_shoff;

	for(i=0; i < fileHdr->e_phnum; i++)
	{
		if(PT_LOAD == phdr[i].p_type)
		{
			memcpy(elfObj->space + phdr[i].p_vaddr - elfObj->vstart_addr,
					elfFile + phdr[i].p_offset, phdr[i].p_filesz);
		}
	}

	elfObj->entry = elfObj->space + fileHdr->e_entry - elfObj->vstart_addr;
	ASLOG(ELF32, "entry is %p\n", elfObj->entry);
	/* handle relocation section */
	for (i = 0; i < fileHdr->e_shnum; i ++)
	{
		uint32_t j, nr_reloc;
		Elf32_Sym *symtab;
		Elf32_Rel *rel;
		uint8_t *strtab;

		if(SHT_REL == shdr[i].sh_type)
		{
			/* get relocate item */
			rel = (Elf32_Rel *)(elfFile + shdr[i].sh_offset);
			/* locate .rel.plt and .rel.dyn section */
			symtab = (Elf32_Sym *)(elfFile + shdr[shdr[i].sh_link].sh_offset);
			strtab = (uint8_t *)(elfFile +
					shdr[shdr[shdr[i].sh_link].sh_link].sh_offset);
			nr_reloc = (uint32_t)(shdr[i].sh_size / sizeof(Elf32_Rel));
			/* relocate every items */
			for (j = 0; j < nr_reloc; j ++)
			{
				Elf32_Sym *sym = &symtab[ELF32_R_SYM(rel->r_info)];

				ASLOG(ELF32, "relocate symbol %s shndx %d\n",
						strtab + sym->st_name, sym->st_shndx);
				if ((sym->st_shndx != SHT_NULL) ||
					(ELF32_ST_BIND(sym->st_info) == STB_LOCAL) ||
					((ELF32_ST_BIND(sym->st_info) == STB_GLOBAL) &&
					 (ELF32_ST_TYPE(sym->st_info) == STT_OBJECT)) )
				{
					ELF32_Relocate(elfObj, rel,
								(Elf32_Addr)(elfObj->space
										+ sym->st_value
										- elfObj->vstart_addr));
				}
				else
				{
					Elf32_Addr addr;

					/* need to resolve symbol in kernel symbol table */
					addr = (Elf32_Addr)ELF_FindSymbol((const char *)(strtab + sym->st_name));
					if (addr == 0)
					{
						ASLOG(ERROR,"ELF: can't find %s in kernel symbol table\n",
								strtab + sym->st_name);
						r = FALSE;
					}
					else
					{
						ELF32_Relocate(elfObj, rel, addr);
					}

				}
				rel ++;
			}
		}
	}

	return r;
}
static uint32_t ELF32_GetSymbolTableSize(void* elfFile, uint32_t *symtabCount)
{
	uint32_t i,sz;
	Elf32_Ehdr *fileHdr = elfFile;
	Elf32_Shdr *shdr = elfFile + fileHdr->e_shoff;

	/* construct module symbol table */
	for (i = 0; i < fileHdr->e_shnum; i ++)
	{
		/* find .dynsym section */
		uint8_t *shstrab;
		shstrab = elfFile + shdr[fileHdr->e_shstrndx].sh_offset;
		if (0 == strcmp((const char *)(shstrab + shdr[i].sh_name), ELF_DYNSYM))
			break;
	}
	/* found .dynsym section */
	if (i != fileHdr->e_shnum)
	{
		uint32_t j, count = 0;
		Elf32_Sym  *symtab = NULL;
		uint8_t *strtab = NULL;

		symtab = elfFile + shdr[i].sh_offset;
		strtab = elfFile + shdr[shdr[i].sh_link].sh_offset;

		for (j = 0; j < shdr[i].sh_size / sizeof(Elf32_Sym); j++)
		{
			if ((ELF32_ST_BIND(symtab[j].st_info) == STB_GLOBAL) &&
				(ELF32_ST_TYPE(symtab[j].st_info) == STT_FUNC))
				count ++;
		}

		sz = count * sizeof(ELF32_SymtabType);
		*symtabCount = count;

		for (j = 0, count = 0; j < shdr[i].sh_size / sizeof(Elf32_Sym); j++)
		{
			size_t length;

			if ((ELF32_ST_BIND(symtab[j].st_info) != STB_GLOBAL) ||
				(ELF32_ST_TYPE(symtab[j].st_info) != STT_FUNC))
				continue;

			sz += strlen((const char *)(strtab + symtab[j].st_name)) + 1;
			count ++;
		}
	}
	else
	{
		sz = 0;
		*symtabCount = 0;
	}

	return sz;
}
static void ELF32_ConstructSymbolTable(void* elfFile, ELF32_ObjectType* elfObj)
{
	uint32_t i;
	Elf32_Ehdr *fileHdr = elfFile;
	Elf32_Shdr *shdr = elfFile + fileHdr->e_shoff;

	/* construct module symbol table */
	for (i = 0; i < fileHdr->e_shnum; i ++)
	{
		/* find .dynsym section */
		uint8_t *shstrab;
		shstrab = elfFile + shdr[fileHdr->e_shstrndx].sh_offset;
		if (0 == strcmp((const char *)(shstrab + shdr[i].sh_name), ELF_DYNSYM))
			break;
	}
	/* found .dynsym section */
	if (i != fileHdr->e_shnum)
	{
		uint32_t j, count = 0;
		Elf32_Sym  *symtab = NULL;
		uint8_t *strtab = NULL;
		void* strpool;

		symtab = elfFile + shdr[i].sh_offset;
		strtab = elfFile + shdr[shdr[i].sh_link].sh_offset;

		strpool = elfObj->symtab + elfObj->nsym*sizeof(ELF32_SymtabType);

		for (j = 0, count = 0; j < shdr[i].sh_size / sizeof(Elf32_Sym); j++)
		{
			size_t length;

			if ((ELF32_ST_BIND(symtab[j].st_info) != STB_GLOBAL) ||
				(ELF32_ST_TYPE(symtab[j].st_info) != STT_FUNC))
				continue;

			length = strlen((const char *)(strtab + symtab[j].st_name)) + 1;

			elfObj->symtab[count].addr =
				(void *)(elfObj->space + symtab[j].st_value);
			elfObj->symtab[count].name = strpool;
			strpool += length;
			memset((void *)elfObj->symtab[count].name, 0, length);
			memcpy((void *)elfObj->symtab[count].name,
					  strtab + symtab[j].st_name,
					  length);
			ASLOG(ELF32, "stmtab[%d] %s %p\n", count,
						elfObj->symtab[count].name,
						elfObj->symtab[count].addr);
			count ++;
		}
	}
	else
	{
		elfObj->symtab = NULL;
		elfObj->nsym = 0;
	}
}

static ELF32_ObjectType* ELF32_LoadSharedObject(void* elfFile)
{
	ELF32_ObjectType* elfObj = NULL;
	Elf32_Addr vstart_addr, vend_addr;
	uint32_t elf_size;
	uint32_t symtab_size;
	uint32_t nsym;

	if(ELF32_GetVirtualAddress(elfFile, &vstart_addr, &vend_addr))
	{
		elf_size = vend_addr - vstart_addr;

		symtab_size = ELF32_GetSymbolTableSize(elfFile,&nsym);

		elfObj = malloc(sizeof(ELF32_ObjectType)+elf_size+symtab_size);
		if(NULL != elfObj)
		{
			elfObj->magic = ELF32_MAGIC;
			elfObj->space = &elfObj[1];
			elfObj->symtab = ((void*)&elfObj[1]) + elf_size;
			elfObj->nsym   = nsym;
			elfObj->size  = elf_size;
			elfObj->vstart_addr = vstart_addr;
			memset(elfObj->space, 0, elf_size);
			if(FALSE == ELF32_LoadObject(elfFile, elfObj))
			{
				free(elfObj);
				elfObj = NULL;
			}
			else
			{
				ELF32_ConstructSymbolTable(elfFile, elfObj);
			}
		}
	}

	return elfObj;
}
/* ============================ [ FUNCTIONS ] ====================================================== */
void* ELF32_Load(void* elfFile)
{
	void* elf = NULL;
	switch(((Elf32_Ehdr*)elfFile)->e_type)
	{
		case ET_REL:
			break;
		case ET_EXEC:
			break;
		case ET_DYN:
			elf = ELF32_LoadSharedObject(elfFile);
			break;
		default:
		break;
	}
	return elf;
}

void* ELF32_LookupSymbol(ELF32_ObjectType *elfObj, const char *symbol)
{
	void* addr = NULL;
	uint32_t i;

	for(i=0; i<elfObj->nsym; i++)
	{
		if(0 == strcmp(elfObj->symtab[i].name, symbol))
		{
			addr = elfObj->symtab[i].addr;
			break;
		}
	}

	return addr;
}

void ELF32_Close(ELF32_ObjectType *elfObj)
{
	free(elfObj);
}
