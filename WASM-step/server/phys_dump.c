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

/* ------------------------------------------------------------------ */
/*  pagemap helpers                                                    */
#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define VIRT_PAGE(v)   ((uint64_t)(v) / PAGE_SIZE)
#define PAGE_OFFSET(v) ((uint64_t)(v) & (PAGE_SIZE - 1))

static int pagemap_fd = -1;

static uint64_t virt_to_phys(void *va)
{
    if (pagemap_fd < 0) {
        pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
        if (pagemap_fd < 0) { perror("open pagemap"); return 0; }
    }

    uint64_t entry = 0;
    off_t off = VIRT_PAGE(va) * sizeof(entry);

    if (lseek(pagemap_fd, off, SEEK_SET) == (off_t)-1) { perror("lseek"); return 0; }
    if (read(pagemap_fd, &entry, sizeof(entry)) != sizeof(entry)) { perror("read"); return 0; }

    if (!(entry & (1ULL << 63))) return 0;            /* not present */
    uint64_t pfn = entry & ((1ULL << 55) - 1);
    return (pfn * PAGE_SIZE) + PAGE_OFFSET(va);
}

/* pretty‑print like iomem (phys_start-phys_end : label) -------------- */
static void dump_range(uint64_t p_start, uint64_t p_end_incl, const char *label)
{
    if (!p_start || !p_end_incl) return;              /* translation failed */
    printf("%08lx-%08lx : %s\n", p_start, p_end_incl, label);
}

/* path helpers ------------------------------------------------------- */
static void canon_exe(char *buf, size_t n)
{
    ssize_t len = readlink("/proc/self/exe", buf, n - 1);
    if (len < 0) { perror("readlink"); exit(1); }
    buf[len] = '\0';
    char real[PATH_MAX];
    if (realpath(buf, real)) strncpy(buf, real, n);
}

/* Does this /proc/self/maps line belong to our binary? --------------- */
static int line_for_own_bin(const char *line, const char *exe)
{
    const char *hit = strstr(line, exe);
    if (!hit) return 0;
    /* ok if string just ends or "(deleted)" follows */
    char c = hit[strlen(exe)];
    return (c == '\0' || c == '\n' || c == ' ' || c == '\t' || c == '(');
}

/* ------------------------------------------------------------------ */
int main(void)
{
    /* ---- 1. make sure we have an extra resident heap block -------- */
    size_t extra_heap_sz = 16 * 1024 * 1024;          /* 16 MiB */
    char  *extra_heap    = malloc(extra_heap_sz);
    if (!extra_heap) { perror("malloc"); return 1; }
    /* fault in every page of the extra malloc block */
    for (size_t i = 0; i < extra_heap_sz; i += PAGE_SIZE)
        extra_heap[i] = (char)i;

    /* ---- 2. open and walk /proc/self/maps ------------------------- */
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) { perror("maps"); return 1; }

    char exe[PATH_MAX]; canon_exe(exe, sizeof(exe));

    char  line[1024];
    while (fgets(line, sizeof(line), maps)) {
        uint64_t v_start, v_end, offset;
        char perms[5] = {0};

        if (sscanf(line, "%lx-%lx %4s %lx", &v_start, &v_end, perms, &offset) != 4)
            continue;

        /* Tag recognised areas */
        const char *label = NULL;

        if (strstr(line, "[heap]"))
            label = "Process heap";
        else if (strstr(line, "[stack]"))
            label = "Process stack";
        else if (line_for_own_bin(line, exe)) {
            if (perms[0]=='r' && perms[2]=='x')        label="User binary code";
            else if (perms[0]=='r' && perms[2]=='-')   label="User binary rodata";
            else if (perms[0]=='r' && perms[1]=='w')   label="User binary data/bss";
        }

        if (!label) continue;                         /* skip other mappings */

        uint64_t p_start = virt_to_phys((void *)v_start);
        uint64_t p_end   = virt_to_phys((void *)(v_end-1));
        dump_range(p_start, p_end | (PAGE_SIZE-1), label); /* inclusive end */
    }
    fclose(maps);

    /* ---- 3. our extra malloc block ------------------------------- */
    uint64_t heap_p_start = virt_to_phys(extra_heap);
    uint64_t heap_p_end   = virt_to_phys(extra_heap + extra_heap_sz - 1);
    if (heap_p_start && heap_p_end)
        dump_range(heap_p_start, heap_p_end | (PAGE_SIZE-1), "Extra malloc 16MiB");



    /* ---- 4. local stack variable for good measure ---------------- */
    int stack_var = 0;
    uint64_t stk_p = virt_to_phys(&stack_var);
    dump_range(stk_p & PAGE_MASK, (stk_p | (PAGE_SIZE-1)), "Current stack page");

    return 0;
}