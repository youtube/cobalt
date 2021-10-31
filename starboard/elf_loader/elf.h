// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_ELF_LOADER_ELF_H_
#define STARBOARD_ELF_LOADER_ELF_H_

// Subset of the ELF specification for loading Dynamic Shared Libraries.
// System V Application Binary Interface - DRAFT - 10 June 2013
// http://www.sco.com/developers/gabi/latest/contents.html

#ifndef __cplusplus
#error "Only C++ files can include this header."
#endif

#include "starboard/types.h"

namespace starboard {
namespace elf_loader {

// 32 bit data types

// Unsigned program address - 4 bytes.
typedef uint32_t Elf32_Addr;

// Unsigned medium integer - 2 bytes.
typedef uint16_t Elf32_Half;

// Unsigned file offset - 4 bytes.
typedef uint32_t Elf32_Off;

// Signed large integer - 4 bytes.
typedef int32_t Elf32_Sword;

// Unsigned large integer - 4 bytes.
typedef uint32_t Elf32_Word;

// 64 bit data types

// Unsigned program address - 8 bytes.
typedef uint64_t Elf64_Addr;

// Unsigned file offset - 8 bytes.
typedef uint64_t Elf64_Off;

// Unsigned medium intege 2 - bytes.
typedef uint16_t Elf64_Half;

// Unsigned integer - 4 bytes.
typedef uint32_t Elf64_Word;

// Signed integer - 4 bytes.
typedef int32_t Elf64_Sword;

// Unsigned long integer - 8 bytes.
typedef uint64_t Elf64_Xword;

// Signed long integer - 8 bytes.
typedef int64_t Elf64_Sxword;

#define EI_NIDENT (16)

// Pack all the structs at 1 byte alignment.
#pragma pack(push)
#pragma pack(1)

// 32 bit ELF file header.
typedef struct {
  // The initial bytes mark the file as an object file and provide
  // machine-independent data.
  unsigned char e_ident[EI_NIDENT];

  // The object file type. We support only ET_DYN.
  Elf32_Half e_type;

  // Architecture of the file.
  Elf32_Half e_machine;

  // Object file version. The value should be 1.
  Elf32_Word e_version;

  // Virtual address to which the system first transfers
  // control, thus starting the process.
  Elf32_Addr e_entry;

  // Program header table's file offset in bytes.
  Elf32_Off e_phoff;

  // Section header table's file offset in bytes.
  Elf32_Off e_shoff;

  // Processor-specific flags associated with the file.
  Elf32_Word e_flags;

  // ELF header's size in bytes.
  Elf32_Half e_ehsize;

  // Size in bytes of one entry in the file's program  header table
  Elf32_Half e_phentsize;

  // The number of entries in the program header table.
  Elf32_Half e_phnum;

  // Section header's size in bytes.
  Elf32_Half e_shentsize;

  // The number of entries in the section header table.
  Elf32_Half e_shnum;

  // The section header table index of the entry associated
  // with the section name string table.
  Elf32_Half e_shstrndx;
} Elf32_Ehdr;

// 64 bit ELF file header.
typedef struct {
  // The initial bytes mark the file as an object file and provide
  // machine-independent data.
  unsigned char e_ident[EI_NIDENT];

  // The object file type. We support only ET_DYN.
  Elf64_Half e_type;

  // Architecture of the file.
  Elf64_Half e_machine;

  // Object file version. The value should be 1.
  Elf64_Word e_version;

  // Virtual address to which the system first transfers
  // control, thus starting the process.
  Elf64_Addr e_entry;

  // Program header table's file offset in bytes.
  Elf64_Off e_phoff;

  // Section header table's file offset in bytes.
  Elf64_Off e_shoff;

  // Processor-specific flags associated with the file.
  Elf64_Word e_flags;

  // This member holds the ELF header's size in bytes.
  Elf64_Half e_ehsize;

  // Size in bytes of one entry in the file's program  header table
  Elf64_Half e_phentsize;

  // The number of entries in the section header table.
  Elf64_Half e_phnum;

  // Section header's size in bytes.
  Elf64_Half e_shentsize;

  // The number of entries in the section header table.
  Elf64_Half e_shnum;

  // The section header table index of the entry associated
  // with the section name string table.
  Elf64_Half e_shstrndx;
} Elf64_Ehdr;

// 32 bit Note header.
typedef struct {
  // Length of the note's name
  Elf32_Word n_namesz;

  // Length of the note's descriptor.
  Elf32_Word n_descsz;

  // Type of the note.
  Elf32_Word n_type;
} Elf32_Nhdr;

// 64 bit Note header.
typedef struct {
  // Length of the note's name
  Elf64_Word n_namesz;

  // Length of the note's descriptor.
  Elf64_Word n_descsz;

  // Type of the note.
  Elf64_Word n_type;
} Elf64_Nhdr;

// 32 bit Program header.
typedef struct {
  // The kind of segment this array element describes.
  Elf32_Word p_type;

  // The offset from the beginning of the file at which the
  // first byte of the segment resides.
  Elf32_Off p_offset;

  // The virtual address at which the first byte of the
  // segment resides in memory.
  Elf32_Addr p_vaddr;

  // Segment's physical address. Unused for shared libraries.
  Elf32_Addr p_paddr;

  // The number of bytes in the file image of the segment.  May be zero.
  Elf32_Word p_filesz;

  // The number of bytes in the memory image of the segment.  May be zero.
  Elf32_Word p_memsz;

  // Segment flags
  Elf32_Word p_flags;

  // Segment alignment constraint.
  Elf32_Word p_align;
} Elf32_Phdr;

// 64 bit Program header.
typedef struct {
  // The kind of segment this array element describes.
  Elf64_Word p_type;

  // Segment flags
  Elf64_Word p_flags;

  // The offset from the beginning of the file at which the
  // first byte of the segment resides.
  Elf64_Off p_offset;

  // The virtual address at which the first byte of the
  // segment resides in memory.
  Elf64_Addr p_vaddr;

  // Segment's physical address. Unused for shared libraries.
  Elf64_Addr p_paddr;

  // The number of bytes in the file image of the segment. May be zero.
  Elf64_Xword p_filesz;

  // The number of bytes in the memory image of the segment.  May be zero.
  Elf64_Xword p_memsz;

  // Segment alignment constraint
  Elf64_Xword p_align;
} Elf64_Phdr;

// 32 bit Dynamic Section Entry
typedef struct {
  // Controls the interpretation of d_un.
  Elf32_Sword d_tag;
  union {
    // These objects represent integer values with various interpretations.
    Elf32_Word d_val;
    // These objects represent program virtual addresses.
    Elf32_Addr d_ptr;
  } d_un;
} Elf32_Dyn;

// 64 bit Dynamic Section Entry
typedef struct {
  // Controls the interpretation of d_un.
  Elf64_Sxword d_tag;
  union {
    // These objects represent integer values with various interpretations.
    Elf64_Xword d_val;
    // These objects represent program virtual addresses.
    Elf64_Addr d_ptr;
  } d_un;
} Elf64_Dyn;

// 32 bit Symbol Table Entry
typedef struct {
  // An index into the object file's symbol string table,
  // which holds the character representations of the symbol names. If the value
  // is non-zero, it represents a string table index that gives the symbol name.
  // Otherwise, the symbol table entry has no name.
  Elf32_Word st_name;

  // The value of the associated symbol. Depending on the
  // context, this may be an absolute value, an address, and so on;
  Elf32_Addr st_value;

  // Many symbols have associated sizes. For example, a data object's size is
  // the number of bytes contained in the object.
  Elf32_Word st_size;

  // The symbol's type and binding attributes.
  unsigned char st_info;

  // Symbol's visibility.
  unsigned char st_other;

  // Every symbol table entry is defined in relation to some section. This
  // member holds the relevant section header table index.
  Elf32_Half st_shndx;
} Elf32_Sym;

// 64 bit Symbol Table Entry
typedef struct {
  // An index into the object file's symbol string table,
  // which holds the character representations of the symbol names. If the value
  // is non-zero, it represents a string table index that gives the symbol name.
  // Otherwise, the symbol table entry has no name.
  Elf64_Word st_name;

  // The symbol's type and binding attributes.
  unsigned char st_info;

  // Symbol's visibility.
  unsigned char st_other;

  // Every symbol table entry is defined in relation to some section. This
  // member holds the relevant section header table index.
  Elf64_Half st_shndx;

  // The value of the associated symbol. Depending on the
  // context, this may be an absolute value, an address, and so on;
  Elf64_Addr st_value;

  // Many symbols have associated sizes. For example, a data object's size is
  // the number of bytes contained in the object.
  Elf64_Xword st_size;
} Elf64_Sym;

// 32 bit Relocation Entry
typedef struct {
  // The location at which to apply the relocation action. For  a relocatable
  // file, the value is the byte offset from the beginning of the  section to
  // the storage unit affected by the relocation. For an executable file or a
  // shared object, the value is the virtual address of the storage unit
  // affected by the relocation.
  Elf32_Addr r_offset;
  // The symbol table index with respect to which the
  // relocation must be made, and the type of relocation to apply.
  Elf32_Word r_info;
} Elf32_Rel;

// 64 bit Relocation Entry
typedef struct {
  // The location at which to apply the relocation action. For
  // a relocatable file, the value is the byte offset from the beginning of the
  // section to the storage unit affected by the relocation. For an executable
  // file or a shared object, the value is the virtual address of the storage
  // unit affected by the relocation.
  Elf64_Addr r_offset;

  // The symbol table index with respect to which the
  // relocation must be made, and the type of relocation to apply.
  Elf64_Xword r_info;
} Elf64_Rel;

// 32 bit Relocation Entry with Addend.
typedef struct {
  // The location at which to apply the relocation action. For
  // a relocatable file, the value is the byte offset from the beginning of the
  // section to the storage unit affected by the relocation. For an executable
  // file or a shared object, the value is the virtual address of the storage
  // unit affected by the relocation.
  Elf32_Addr r_offset;

  // The symbol table index with respect to which the
  // relocation must be made, and the type of relocation to apply.
  Elf32_Word r_info;

  // A constant addend used to compute the value to be stored into the
  // relocatable field.
  Elf32_Sword r_addend;
} Elf32_Rela;

// 64 bit Relocation Entry with Addend.
typedef struct {
  // The location at which to apply the relocation action. For  a relocatable
  // file, the value is the byte offset from the beginning of the section to the
  // storage unit affected by the relocation. For an executable file or a shared
  // object, the value is the virtual address of the storage unit affected by
  // the relocation.
  Elf64_Addr r_offset;

  // The symbol table index with respect to which the
  // relocation must be made, and the type of relocation to apply.
  Elf64_Xword r_info;

  // A constant addend used to compute the value to be stored into the
  // relocatable field.
  Elf64_Sxword r_addend;
} Elf64_Rela;

#pragma pack(pop)

#define EI_CLASS 4
#define ELFCLASS32 1
#define ELFCLASS64 2
#define EI_DATA 5
#define ELFDATA2LSB 1
#define ET_DYN 3
#define EV_CURRENT 1

#if SB_SIZE_OF(POINTER) == 4
typedef Elf32_Ehdr Ehdr;
typedef Elf32_Nhdr Nhdr;
typedef Elf32_Phdr Phdr;
typedef Elf32_Addr Addr;
typedef Elf32_Dyn Dyn;
typedef Elf32_Word Word;
typedef Elf32_Sym Sym;
typedef Elf32_Rel Rel;
typedef Elf32_Rela Rela;
typedef Elf32_Word Relr;
typedef Elf32_Sword Sword;
#define ELF_BITS 32
#define ELF_R_TYPE ELF32_R_TYPE
#define ELF_R_SYM ELF32_R_SYM
#define ELF_CLASS_VALUE ELFCLASS32
#elif SB_SIZE_OF(POINTER) == 8
typedef Elf64_Ehdr Ehdr;
typedef Elf64_Nhdr Nhdr;
typedef Elf64_Phdr Phdr;
typedef Elf64_Addr Addr;
typedef Elf64_Dyn Dyn;
typedef Elf64_Word Word;
typedef Elf64_Sym Sym;
typedef Elf64_Rel Rel;
typedef Elf64_Rela Rela;
typedef Elf64_Word Relr;
typedef Elf64_Sword Sword;
#define ELF_BITS 64
#define ELF_R_TYPE ELF64_R_TYPE
#define ELF_R_SYM ELF64_R_SYM
#define ELF_CLASS_VALUE ELFCLASS64
#else
#error "Unsupported pointer size"
#endif

#define ELF32_R_SYM(val) ((val) >> 8)
#define ELF32_R_TYPE(val) ((val)&0xff)
#define ELF32_R_INFO(sym, type) (((sym) << 8) + ((type)&0xff))

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((i)&0xffffffff)
#define ELF64_R_INFO(sym, type) ((((Elf64_Xword)(sym)) << 32) + (type))

#define ELFMAG "\177ELF"
#define SELFMAG 4

// TODO: Refactor the code to detect it at runtime
// using DT_PLTREL.
#if SB_IS(ARCH_ARM64) || SB_IS(ARCH_X64)
#define USE_RELA
#endif

#if defined(USE_RELA)
typedef Rela rel_t;
#else
typedef Rel rel_t;
#endif

#if SB_IS(ARCH_ARM)
#define ELF_MACHINE 40
#elif SB_IS(ARCH_X86)
#define ELF_MACHINE 3
#elif SB_IS(ARCH_X64)
#define ELF_MACHINE 62
#elif SB_IS(ARCH_ARM64)
#define ELF_MACHINE 183
#else
#error "Unsupported target CPU architecture"
#endif

// Segment types.
typedef enum SegmentTypes {
  // Unused segment.
  PT_NULL = 0,

  // Loadable segment.
  PT_LOAD = 1,

  // Dynamic linking information.
  PT_DYNAMIC = 2,

  // Interpreter pathname.
  PT_INTERP = 3,

  // Auxiliary information.
  PT_NOTE = 4,

  // Reserved.
  PT_SHLIB = 5,

  // The program header table itself.
  PT_PHDR = 6,

  // The thread-local storage template.
  PT_TLS = 7
} SegmentTypes;

// Symbol bindings.
typedef enum SymbolBindings {
  // Local symbol, not visible outside obj file containing def
  STB_LOCAL = 0,

  // Global symbol, visible to all object files being combined
  STB_GLOBAL = 1,

  // Weak symbol, like global but lower-precedence
  STB_WEAK = 2,

  STB_GNU_UNIQUE = 10,

  // Lowest operating system-specific binding type
  STB_LOOS = 10,

  // Highest operating system-specific binding type
  STB_HIOS = 12,

  // Lowest processor-specific binding type
  STB_LOPROC = 13,

  // Highest processor-specific binding type
  STB_HIPROC = 15
} SymbolBindings;

#define ELF_ST_BIND(x) ((x) >> 4)
#define ELF32_ST_BIND(x) ELF_ST_BIND(x)
#define ELF64_ST_BIND(x) ELF_ST_BIND(x)

#define PF_X (1 << 0)
#define PF_W (1 << 1)
#define PF_R (1 << 2)
#define PF_MASKOS 0x0ff00000
#define PF_MASKPROC 0xf0000000

// Dynamic table tags.
typedef enum DynamicTags {
  DT_NULL = 0,
  DT_NEEDED = 1,
  DT_PLTRELSZ = 2,
  DT_PLTGOT = 3,
  DT_HASH = 4,
  DT_STRTAB = 5,
  DT_SYMTAB = 6,
  DT_RELA = 7,
  DT_RELASZ = 8,
  DT_RELAENT = 9,
  DT_STRSZ = 10,
  DT_SYMENT = 11,
  DT_INIT = 12,
  DT_FINI = 13,
  DT_SONAME = 14,
  DT_RPATH = 15,
  DT_SYMBOLIC = 16,
  DT_REL = 17,
  DT_RELSZ = 18,
  DT_RELENT = 19,
  DT_PLTREL = 20,
  DT_DEBUG = 21,
  DT_TEXTREL = 22,
  DT_JMPREL = 23,
  DT_BIND_NOW = 24,
  DT_INIT_ARRAY = 25,
  DT_FINI_ARRAY = 26,
  DT_INIT_ARRAYSZ = 27,
  DT_FINI_ARRAYSZ = 28,
  DT_RUNPATH = 29,
  DT_FLAGS = 30,
  DT_ENCODING = 32,
  DT_PREINIT_ARRAY = 32,
  DT_PREINIT_ARRAYSZ = 33,
  DT_SYMTAB_SHNDX = 34,
  DT_RELRSZ = 35,
  DT_RELR = 36,
  DT_RELRENT = 37,
  DT_LOOS = 0x6000000D,
  DT_ANDROID_REL = 0x6000000F,
  DT_ANDROID_RELSZ = 0x60000010,
  DT_ANDROID_RELA = 0x60000011,
  DT_ANDROID_RELASZ = 0x60000012,
  DT_HIOS = 0x6ffff000,
  DT_ANDROID_RELR = 0x6fffe000,
  DT_ANDROID_RELRSZ = 0x6fffe001,
  DT_ANDROID_RELRENT = 0x6fffe003,
  DT_GNU_HASH = 0x6ffffef5,
  DT_LOPROC = 0x70000000,
  DT_HIPROC = 0x7fffffff,
} DynamicTags;

typedef enum DynamicFlags {
  // This flag signifies that the object being loaded may make reference to the
  DF_ORIGIN = 0x00000001,

  // If this flag is set in a shared object library, the dynamic linker's symbol
  // resolution algorithm for references within the library is changed. Instead
  // of starting a symbol search with the executable file, the dynamic linker
  // starts from the shared object itself. If the shared object fails to supply
  // the referenced symbol, the dynamic linker then searches the executable file
  // and other shared objects as usual.
  DF_SYMBOLIC = 0x00000002,

  //  This flag is not set, no relocation entry should cause a modification to a
  //  non-writable segment, as specified by the segment permissions in the
  //  program header table.
  DF_TEXTREL = 0x00000004,

  // If set in a shared object or executable, this flag instructs the dynamic
  // linker to process all relocations for the object containing this entry
  // before transferring control to the program.
  DF_BIND_NOW = 0x00000008,

  // If set in a shared object or executable, this flag instructs the dynamic
  // linker to reject attempts to load this file dynamically. It indicates that
  // the shared object or executable contains code using a static thread-local
  // storage scheme. Implementations need not support any form of thread-local
  // storage.
  DF_STATIC_TLS = 0x00000010,
} DynamicFlags;

// Relocation types per CPU architecture
#if SB_IS(ARCH_ARM)
typedef enum RelocationTypes {
  R_ARM_ABS32 = 2,
  R_ARM_REL32 = 3,
  R_ARM_GLOB_DAT = 21,
  R_ARM_JUMP_SLOT = 22,
  R_ARM_COPY = 20,
  R_ARM_RELATIVE = 23,
} RelocationTypes;
#elif SB_IS(ARCH_ARM64)
typedef enum RelocationTypes {
  R_AARCH64_ABS64 = 257,
  R_AARCH64_COPY = 1024,
  R_AARCH64_GLOB_DAT = 1025,
  R_AARCH64_JUMP_SLOT = 1026,
  R_AARCH64_RELATIVE = 1027,
} RelocationTypes;
#elif SB_IS(ARCH_X86)
typedef enum RelocationTypes {
  R_386_32 = 1,
  R_386_PC32 = 2,
  R_386_GLOB_DAT = 6,
  R_386_JMP_SLOT = 7,
  R_386_RELATIVE = 8,
} RelocationTypes;
#elif SB_IS(ARCH_X64)
typedef enum RelocationTypes {
  R_X86_64_64 = 1,
  R_X86_64_PC32 = 2,
  R_X86_64_GLOB_DAT = 6,
  R_X86_64_JMP_SLOT = 7,
  R_X86_64_RELATIVE = 8,
} RelocationTypes;
#else
#error "Unsupported architecture for relocations."
#endif

// Note types
#define NT_GNU_BUILD_ID 3
#define NOTE_PADDING(a) ((a + 3) & ~3)

// Helper macros for memory page computations.
#ifndef PAGE_SIZE
#define PAGE_SHIFT 12

#if SB_SIZE_OF(POINTER) == 4
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#elif SB_SIZE_OF(POINTER) == 8
#define PAGE_SIZE (1ULL << PAGE_SHIFT)
#else
#error "Unsupported pointer size"
#endif
#endif

#ifndef PAGE_MASK
#define PAGE_MASK (~(PAGE_SIZE - 1))
#endif

#define PAGE_START(x) ((x)&PAGE_MASK)
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)
#define PAGE_END(x) PAGE_START((x) + (PAGE_SIZE - 1))

#define SHN_UNDEF 0

}  // namespace elf_loader
}  // namespace starboard
#endif  // STARBOARD_ELF_LOADER_ELF_H_
