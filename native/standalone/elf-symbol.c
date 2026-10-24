#include "elf-symbol.h"
#include <elf.h>
#include <errno.h>
#include <string.h>

#define ELF_LIMIT UINT64_C(134217728)
#define TABLE_LIMIT UINT32_C(1048576)
typedef struct {
    SilphElfSource *source;
    Elf64_Phdr programs[256];
    uint16_t count;
    uint64_t base, bias, symtab, strtab, strsz;
} Image;
static int range(uint64_t total, uint64_t offset, uint64_t bytes) {
    return offset <= total && bytes <= total - offset;
}
static int read_at(SilphElfSource *source, uint64_t offset, void *out, size_t bytes) {
    if (!range(source->bytes, offset, bytes)) { errno = ENOEXEC;return -1; }
    return source->read(source->context, offset, out, bytes);
}
static int virtual_range(Image *image, uint64_t value, uint64_t bytes, int flags) {
    for (unsigned i = 0; i < image->count; i++) {
        const Elf64_Phdr *p = image->programs + i;
        if (p->p_type == PT_LOAD && (p->p_flags & flags) == (unsigned)flags && value >= p->p_vaddr
            && range(p->p_filesz, value - p->p_vaddr, bytes)) return 1;
    }
    return 0;
}
static int read_virtual(Image *image, uint64_t value, void *out, size_t bytes) {
    for (unsigned i = 0; i < image->count; i++) {
        const Elf64_Phdr *p = image->programs + i;
        if (p->p_type != PT_LOAD || !(p->p_flags & PF_R) || value < p->p_vaddr
            || !range(p->p_filesz, value - p->p_vaddr, bytes)) continue;
        uint64_t offset = value - p->p_vaddr;
        if (image->source->memory) {
            if (value < image->base) break;
            offset = value - image->base;
        } else {
            if (offset > UINT64_MAX - p->p_offset) break;
            offset += p->p_offset;
        }
        return read_at(image->source, offset, out, bytes);
    }
    errno = ENOEXEC;return -1;
}
static uint64_t pointer(Image *image, uint64_t value) {
    if (virtual_range(image, value, 1, PF_R)) return value;
    if (image->source->memory && value >= image->bias
        && virtual_range(image, value - image->bias, 1, PF_R)) return value - image->bias;
    return 0;
}
static int item_at(Image *image, uint64_t base, uint64_t index, size_t size, void *output) {
    if (index > TABLE_LIMIT || index > (UINT64_MAX - base) / size) { errno = ENOEXEC;return -1; }
    return read_virtual(image, base + index * size, output, size);
}
static int match(Image *image, uint32_t index, const char *wanted, uint64_t *value) {
    Elf64_Sym entry;
    if (item_at(image, image->symtab, index, sizeof(entry), &entry)) return -1;
    if (entry.st_shndx == SHN_UNDEF || ELF64_ST_TYPE(entry.st_info) != STT_FUNC || !entry.st_value
        || (ELF64_ST_BIND(entry.st_info) != STB_GLOBAL && ELF64_ST_BIND(entry.st_info) != STB_WEAK)
        || ((entry.st_other & 3) != STV_DEFAULT && (entry.st_other & 3) != STV_PROTECTED)) return 0;
    size_t length = strlen(wanted) + 1;
    if (!range(image->strsz, entry.st_name, length) || image->strtab > UINT64_MAX - entry.st_name) return 0;
    char name[256];
    if (read_virtual(image, image->strtab + entry.st_name, name, length)) return -1;
    if (memcmp(name, wanted, length)) return 0;
    if (!virtual_range(image, entry.st_value, 4, PF_X)) { errno = ENOEXEC;return -1; }
    *value = entry.st_value;return 1;
}
static int lookup_sysv(Image *image, uint64_t address, const char *name, uint64_t *value) {
    uint32_t counts[2], hash = 0, high, index;
    if (read_virtual(image, address, counts, sizeof(counts))) return -1;
    if (!counts[0] || !counts[1] || counts[0] > TABLE_LIMIT || counts[1] > TABLE_LIMIT
        || address > UINT64_MAX - 8 - (uint64_t)counts[0] * 4) { errno = ENOEXEC;return -1; }
    for (const unsigned char *p = (const void *)name; *p; p++) {
        hash = (hash << 4) + *p;high = hash & UINT32_C(0xf0000000);
        if (high) hash ^= high >> 24;
        hash &= ~high;
    }
    if (item_at(image, address + 8, hash % counts[0], 4, &index)) return -1;
    uint64_t chains = address + 8 + (uint64_t)counts[0] * 4;
    for (uint32_t steps = 0; index; steps++) {
        if (index >= counts[1] || steps >= counts[1]) { errno = ENOEXEC;return -1; }
        int result = match(image, index, name, value);
        if (result) return result;
        if (item_at(image, chains, index, 4, &index)) return -1;
    }
    return 0;
}
static int lookup_gnu(Image *image, uint64_t address, const char *name, uint64_t *value) {
    uint32_t header[4], hash = 5381, index;
    if (read_virtual(image, address, header, sizeof(header))) return -1;
    if (!header[0] || !header[2] || header[0] > TABLE_LIMIT || header[1] > TABLE_LIMIT || header[2] > TABLE_LIMIT
        || address > UINT64_MAX - 16 - (uint64_t)header[2] * 8 - (uint64_t)header[0] * 4) { errno = ENOEXEC;return -1; }
    for (const unsigned char *p = (const void *)name; *p; p++) hash = hash * 33 + *p;
    uint64_t buckets = address + 16 + (uint64_t)header[2] * 8;
    uint64_t chains = buckets + (uint64_t)header[0] * 4;
    if (item_at(image, buckets, hash % header[0], 4, &index)) return -1;
    if (!index) return 0;
    if (index < header[1] || index > TABLE_LIMIT) { errno = ENOEXEC;return -1; }
    for (uint32_t steps = 0; steps < TABLE_LIMIT; steps++, index++) {
        uint32_t chain;
        if (index > TABLE_LIMIT) { errno = ENOEXEC;return -1; }
        if (item_at(image, chains, index - header[1], 4, &chain)) return -1;
        if ((chain | 1) == (hash | 1)) {
            int result = match(image, index, name, value);
            if (result) return result;
        }
        if (chain & 1) return 0;
    }
    errno = ENOEXEC;return -1;
}
int silph_elf_symbol(SilphElfSource *source, const char *name, uint64_t *value, uint64_t *base) {
    if (source) source->stage = "arguments";
    if (!source || !source->read || !name || !*name || strlen(name) >= 256 || !value || !base
        || !source->page_size || (source->page_size & (source->page_size - 1))
        || source->bytes > ELF_LIMIT) { errno = EINVAL;return -1; }
    Image image = {.source = source};
    Elf64_Ehdr header;
    source->stage = "elf_header";
    if (read_at(source, 0, &header, sizeof(header))) return -1;
    if (memcmp(header.e_ident, ELFMAG, SELFMAG) || header.e_ident[EI_CLASS] != ELFCLASS64
        || header.e_ident[EI_DATA] != ELFDATA2LSB || header.e_ident[EI_VERSION] != EV_CURRENT
        || header.e_machine != EM_AARCH64 || header.e_type != ET_DYN || header.e_version != EV_CURRENT
        || header.e_ehsize != sizeof(header)) { errno = ENOEXEC;return -1; }
    source->stage = "program_headers";
    if (!header.e_phnum || header.e_phnum > 256 || header.e_phentsize != sizeof(Elf64_Phdr)
        || read_at(source, header.e_phoff, image.programs, (size_t)header.e_phnum * sizeof(Elf64_Phdr))) {
        errno = ENOEXEC;return -1;
    }
    image.count = header.e_phnum;
    image.base = UINT64_MAX;
    const Elf64_Phdr *dynamic = NULL;
    for (unsigned i = 0; i < image.count; i++) {
        const Elf64_Phdr *p = image.programs + i;
        if (p->p_type == PT_LOAD) {
            if (p->p_filesz > p->p_memsz || p->p_vaddr > UINT64_MAX - p->p_memsz
                || (!source->memory && !range(source->bytes, p->p_offset, p->p_filesz))) { errno = ENOEXEC;return -1; }
            if (!p->p_offset && p->p_filesz >= sizeof(header)) {
                uint64_t candidate = p->p_vaddr & ~(source->page_size - 1);
                if (image.base != UINT64_MAX && image.base != candidate) { errno = ENOEXEC;return -1; }
                image.base = candidate;
            }
        }
        if (p->p_type == PT_DYNAMIC) {
            if (dynamic) { errno = ENOEXEC;return -1; }
            dynamic = p;
        }
    }
    if (image.base == UINT64_MAX || !dynamic || (source->memory && source->mapping_start < image.base)) { errno = ENOEXEC;return -1; }
    image.bias = source->memory ? source->mapping_start - image.base : 0;
    if (source->executable) {
        int found = 0;
        source->stage = "executable_image";
        for (unsigned i = 0; i < image.count; i++) {
            const Elf64_Phdr *p = image.programs + i;
            if (p->p_type != PT_LOAD || !(p->p_flags & PF_X) || p->p_filesz < 4) continue;
            int match = source->executable(source->context, p->p_vaddr, p->p_offset, image.base);
            if (match < 0) return -1;
            if (match) found = 1;
        }
        if (!found) { errno = ENOENT;return -1; }
    }
    source->stage = "dynamic_table";
    if (!dynamic->p_filesz || dynamic->p_filesz % sizeof(Elf64_Dyn) || dynamic->p_filesz / sizeof(Elf64_Dyn) > 4096
        || dynamic->p_vaddr > UINT64_MAX - dynamic->p_filesz) { errno = ENOEXEC;return -1; }
    uint64_t hash = 0, gnu = 0, syment = 0;
    int terminated = 0;
    for (uint64_t i = 0; i < dynamic->p_filesz / sizeof(Elf64_Dyn); i++) {
        Elf64_Dyn d;
        if (read_virtual(&image, dynamic->p_vaddr + i * sizeof(d), &d, sizeof(d))) return -1;
        if (d.d_tag == DT_NULL) { terminated = 1;break; }
        uint64_t *slot = NULL;
        switch (d.d_tag) {
            case DT_SYMTAB: slot = &image.symtab;break;
            case DT_STRTAB: slot = &image.strtab;break;
            case DT_STRSZ: slot = &image.strsz;break;
            case DT_SYMENT: slot = &syment;break;
            case DT_HASH: slot = &hash;break;
            case DT_GNU_HASH: slot = &gnu;break;
            default: break;
        }
        if (slot) {
            if (*slot && *slot != d.d_un.d_val) { errno = ENOEXEC;return -1; }
            *slot = d.d_un.d_val;
        }
    }
    image.symtab = pointer(&image, image.symtab);image.strtab = pointer(&image, image.strtab);
    uint64_t raw_hash = hash, raw_gnu = gnu;
    hash = pointer(&image, hash);gnu = pointer(&image, gnu);
    if (!terminated || syment != sizeof(Elf64_Sym) || !image.symtab || !image.strtab || !image.strsz
        || image.strsz > ELF_LIMIT || (raw_hash && !hash) || (raw_gnu && !gnu) || (!hash && !gnu)) { errno = ENOEXEC;return -1; }
    source->stage = gnu ? "gnu_hash" : "sysv_hash";
    int result = gnu ? lookup_gnu(&image, gnu, name, value) : lookup_sysv(&image, hash, name, value);
    if (result == 1) { *base = image.base;source->stage = "complete";return 0; }
    if (!result) errno = ENOENT;
    return -1;
}
