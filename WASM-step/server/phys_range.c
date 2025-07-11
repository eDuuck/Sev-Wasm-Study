#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>
#include <errno.h>

#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE - 1))

/* ── helpers ──────────────────────────────────────────────── */
static uint64_t virt_to_phys(void *virt_addr)
{
    uint64_t virt_pn = (uint64_t)virt_addr / PAGE_SIZE;
    uint64_t offset  = virt_pn * sizeof(uint64_t);
    uint64_t entry   = 0;

    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) { perror("open pagemap"); return 0; }

    if (lseek(fd, offset, SEEK_SET) == -1) {
        perror("lseek pagemap"); close(fd); return 0;
    }
    if (read(fd, &entry, sizeof(uint64_t)) != sizeof(uint64_t)) {
        perror("read pagemap"); close(fd); return 0;
    }
    close(fd);

    if (!(entry & (1ULL << 63))) {   /* page not present */
        fprintf(stderr, "page not present\n");
        return 0;
    }
    uint64_t frame = entry & ((1ULL << 55) - 1);
    return (frame * PAGE_SIZE) + ((uint64_t)virt_addr & (PAGE_SIZE - 1));
}

/* return 1 if the /proc/self/maps line refers to our binary */
static int line_matches_exe(const char *line, const char *exe_path)
{
    const char *pos = strstr(line, exe_path);
    if (!pos) return 0;

    /* OK if exact end or followed by space, tab, newline or " (deleted)" */
    size_t len = strlen(exe_path);
    char   c   = pos[len];
    return c == '\0' || c == '\n' || c == ' ' || c == '\t' || c == '(';
}

/* ── main ────────────────────────────────────────────────── */
int main(void)
{
    /* 1. allocate & touch 16 MiB so it’s resident */
    size_t heap_sz = 16 * 1024 * 1024;
    char  *heap    = malloc(heap_sz);
    if (!heap) { perror("malloc"); return 1; }
    memset(heap, 0, heap_sz);              /* fault pages in */

    /* 2. canonicalise our own executable path */
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len < 0) { perror("readlink"); return 1; }
    exe_path[len] = '\0';

    char real_exe[PATH_MAX];
    if (!realpath(exe_path, real_exe)) {   /* resolve symlinks etc. */
        perror("realpath"); return 1;
    }

    /* 3. scan /proc/self/maps */
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) { perror("open maps"); return 1; }

    uint64_t min_va = UINT64_MAX, max_va = 0;
    char line[1024];
    while (fgets(line, sizeof(line), maps)) {
        uint64_t start, end, offset;
        char perm[5] = {0};

        if (sscanf(line, "%lx-%lx %4s %lx", &start, &end, perm, &offset) != 4)
            continue;

        /* prefer lines that explicitly name our executable */
        if (line_matches_exe(line, real_exe)) {
            if (start < min_va) min_va = start;
            if (end   > max_va) max_va = end;
        }
    }
    rewind(maps);

    /* Fallback: if still not found, take first r-xp mapping with offset 0 */
    if (min_va == UINT64_MAX) {
        while (fgets(line, sizeof(line), maps)) {
            uint64_t start, end, offset;
            char perm[5] = {0};
            if (sscanf(line, "%lx-%lx %4s %lx", &start, &end, perm, &offset) != 4)
                continue;
            if (perm[0] == 'r' && perm[2] == 'x' && offset == 0) {
                min_va = start;
                max_va = end;
                break;
            }
        }
    }
    fclose(maps);

    if (min_va == UINT64_MAX) {
        fprintf(stderr, "still could not locate binary range\n");
        return 1;
    }

    /* 4. translate ranges to physical */
    uint64_t phys_start = virt_to_phys((void *)min_va);
    uint64_t phys_end   = virt_to_phys((void *)(max_va - 1)) + 1;

    uint64_t heap_phys_start = virt_to_phys(heap);
    uint64_t heap_phys_end   = virt_to_phys(heap + heap_sz - 1) + 1;

    printf("Executable virtual range : 0x%lx‑0x%lx\n", min_va, max_va);
    printf("Executable physical range : 0x%lx‑0x%lx\n", phys_start, phys_end);
    printf("Heap 16 MiB virtual range : 0x%lx‑0x%lx\n",
           (uint64_t)heap, (uint64_t)heap + heap_sz);
    printf("Heap 16 MiB physical range: 0x%lx‑0x%lx\n",
           heap_phys_start, heap_phys_end);

    free(heap);
    return 0;
}