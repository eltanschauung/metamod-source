// Linux regression checks for the Kogasatopia 1.12 map-parser backports.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
#include "sourcehook.h"
#include "sh_memory.h"

static void Check(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

static void CheckBits(void *address, int expected)
{
    int actual = -1;
    Check(SourceHook::GetPageBits(address, &actual), "mapped address was not found");
    Check(actual == expected, "mapping permission mismatch");
}

struct Mapping {
    uintptr_t lower;
    uintptr_t upper;
    int bits;
};

int main()
{
    // Compare each mapping against its /proc entry without disclosing addresses.
    std::vector<Mapping> mappings;
    FILE *stream = std::fopen("/proc/self/maps", "r");
    Check(stream != nullptr, "cannot open process maps");
    char *line = nullptr;
    size_t capacity = 0;
    while (getline(&line, &capacity, stream) != -1) {
        unsigned long lower, upper;
        char r, w, x;
        Check(std::sscanf(line, "%lx-%lx %c%c%c", &lower, &upper, &r, &w, &x) == 5,
              "unexpected maps entry");
        int bits = (r == 'r' ? SH_MEM_READ : 0)
                 | (w == 'w' ? SH_MEM_WRITE : 0)
                 | (x == 'x' ? SH_MEM_EXEC : 0);
        mappings.push_back({lower, upper, bits});
    }
    std::free(line);
    std::fclose(stream);
    for (const Mapping &mapping : mappings) {
        CheckBits(reinterpret_cast<void *>(mapping.lower), mapping.bits);
        CheckBits(reinterpret_cast<void *>(mapping.upper - 1), mapping.bits);
        Check(SourceHook::ModuleInMemory(reinterpret_cast<char *>(mapping.lower), 1),
              "mapping start not recognized");
        Check(SourceHook::ModuleInMemory(reinterpret_cast<char *>(mapping.upper - 1), 1),
              "mapping final byte not recognized");
    }

    const size_t pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    char *memory = static_cast<char *>(mmap(nullptr, pageSize * 3,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Check(memory != MAP_FAILED, "mmap failed");
    CheckBits(memory, SH_MEM_READ | SH_MEM_WRITE);
    CheckBits(memory + pageSize * 3 - 1, SH_MEM_READ | SH_MEM_WRITE);
    Check(SourceHook::ModuleInMemory(memory, pageSize * 3), "mapped range not recognized");

    Check(mprotect(memory + pageSize, pageSize, PROT_READ) == 0, "read protection failed");
    CheckBits(memory + pageSize, SH_MEM_READ);
    Check(!SourceHook::ModuleInMemory(memory, pageSize * 3),
          "range spanning separate mappings must preserve original rejection semantics");
    Check(SourceHook::MakePageWritable(memory + pageSize), "make-writable failed");
    CheckBits(memory + pageSize, SH_MEM_READ | SH_MEM_WRITE);

    Check(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "no-access protection failed");
    CheckBits(memory + pageSize, 0);
    Check(SourceHook::ModuleInMemory(memory + pageSize, pageSize),
          "no-access mapping should still be present");
    Check(mprotect(memory + pageSize, pageSize, PROT_READ | PROT_EXEC) == 0,
          "executable protection failed");
    CheckBits(memory + pageSize, SH_MEM_READ | SH_MEM_EXEC);
    Check(SourceHook::MakePageWritable(memory + pageSize), "make RX page writable failed");
    CheckBits(memory + pageSize, SH_MEM_READ | SH_MEM_WRITE | SH_MEM_EXEC);

    Check(munmap(memory, pageSize * 3) == 0, "unmap failed");
    int bits = -1;
    Check(!SourceHook::GetPageBits(memory, &bits), "unmapped address incorrectly found");
    Check(!SourceHook::ModuleInMemory(memory, 1), "unmapped range incorrectly found");
    Check(!SourceHook::GetPageBits(nullptr, &bits), "null address incorrectly found");
    Check(!SourceHook::ModuleInMemory(nullptr, 1), "null range incorrectly found");
    std::printf("PASS: %zu process mappings; boundaries, RW/R/none/RX/RWX, "
                "split ranges, make-writable, unmap and null checks.\n", mappings.size());
    return 0;
}
