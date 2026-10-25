#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <mach/mach_time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct TypeStats {
    uint64_t count = 0;
    uint16_t min_len = UINT16_MAX;
    uint16_t max_len = 0;
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <itch file>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { std::perror("open"); return 1; }

    struct stat st;
    if (fstat(fd, &st) != 0) { std::perror("fstat"); return 1; }
    const size_t size = static_cast<size_t>(st.st_size);

    void* map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) { std::perror("mmap"); return 1; }

    const unsigned char* p = static_cast<const unsigned char*>(map);
    const unsigned char* end = p + size;

    TypeStats stats[256];
    uint64_t total = 0;
    bool truncated = false;

    const uint64_t t0 = mach_absolute_time();

    while (end - p >= 2) {
        const uint16_t len = static_cast<uint16_t>((p[0] << 8) | p[1]);

        if (len == 0 || end - p < static_cast<ptrdiff_t>(2 + len)) {
            truncated = true;
            break;
        }

        const unsigned char type = p[2];
        TypeStats& s = stats[type];
        ++s.count;
        if (len < s.min_len) s.min_len = len;
        if (len > s.max_len) s.max_len = len;
        ++total;

        p += 2 + len;
    }

    const uint64_t t1 = mach_absolute_time();
    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    const double secs = static_cast<double>(t1 - t0) * tb.numer / tb.denom / 1e9;

    std::printf("%-6s %12s %8s %8s\n", "type", "count", "min_len", "max_len");
    for (int t = 0; t < 256; ++t) {
        if (stats[t].count == 0) continue;
        std::printf("%-6c %12llu %8u %8u\n", static_cast<char>(t),
                    static_cast<unsigned long long>(stats[t].count),
                    stats[t].min_len, stats[t].max_len);
    }
    std::printf("\ntotal: %llu messages in %.3f s (%.2f M msg/s)%s\n",
                static_cast<unsigned long long>(total), secs,
                total / secs / 1e6, truncated ? "  [file ended mid-message]" : "");

    munmap(map, size);
    close(fd);
    return 0;
}