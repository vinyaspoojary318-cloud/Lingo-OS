#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ELF64 constants
#define ELF_MAGIC     0x464C457F  // "\x7FELF"
#define ELF_CLASS64   2
#define ELF_DATA_LSB  1
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_X86_64 0x3E

#define PT_LOAD 1

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

struct elf64_header {
    uint32_t magic;
    uint8_t  cls;
    uint8_t  endian;
    uint8_t  version;
    uint8_t  osabi;
    uint8_t  abiversion;
    uint8_t  pad[7];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct elf64_phdr {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed));

// Result of loading an ELF
struct elf_load_info {
    uint64_t entry;          // Virtual entry point
    uint64_t user_stack_top; // Suggested user stack top
    bool     success;
};

// Load an ELF64 executable from a memory buffer into the current address space.
// For now this maps segments into kernel space for testing.
// Later it will use a per-process page table.
//
// Returns true on success and fills info.
bool elf_load(const void *data, size_t size, struct elf_load_info *info);

// Validate ELF header (does not load)
bool elf_check(const void *data, size_t size);
