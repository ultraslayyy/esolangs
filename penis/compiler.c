#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <intrin.h>
#include <io.h>
#include <fcntl.h>
#include <windows.h>

#define fseek64 _fseeki64
#define ftell64 _ftelli64
#else
#include <immintrin.h>
#include <cpuid.h>
#include <fcntl.h>
#include <unistd.h>

#define _FILE_OFFSET_BITS 64
#define fseek64 feesko
#define ftell64 ftello
#endif

#define MAX_DIGITS 2048

static const uint64_t penis64 = 0x73696e6570ULL; // little endian ("penis")

int g_has_avx2 = 0;
int g_no_hints = 0;

#pragma region BigInt

typedef struct {
    int digits[MAX_DIGITS];
    int length;
} BigInt;

void init_bigint(BigInt *n) {
    memset(n->digits, 0, sizeof(n->digits));
    n->length = 1;
}

void add_long(BigInt *n, long long value) {
    int i = 0;
    long long carry = value;

    while (carry > 0 || i < n->length) {
        if (i >= MAX_DIGITS) {
            fprintf(stderr, "Error: BigInt Overflow\n");
            return;
        }

        long long current = n->digits[i] + carry;
        n->digits[i] = (int)(current % 10);
        carry = current / 10;

        i++;
    }

    while (i > 1 && n->digits[i - 1] == 0) i--;
    n->length = i;
}

void add_bigint(BigInt *dest, const BigInt *src) {
    int carry = 0;
    int max_len = (dest->length > src->length) ? dest->length : src->length;
    
    for (int i = 0; i < max_len || carry > 0; ++i) {
        int sum = dest->digits[i] + (i < src->length ? src->digits[i] : 0) + carry;
        dest->digits[i] = sum % 10;
        carry = sum / 10;
        if (i >= dest->length && dest->digits[i] > 0) dest->length = i + 1;
    }
}

void multiply_long(BigInt *n, long long factor) {
    if (factor == 0) {
        init_bigint(n);
        return;
    }

    long long carry = 0;
    for (int i = 0; i < n->length || carry > 0; ++i) {
        if (i >= MAX_DIGITS) {
            fprintf(stderr, "BigInt Overflow\n");
            return;
        }

        long long current = carry;
        if (i < n->length) {
            current += (long long)n->digits[i] * factor;
        }

        n->digits[i] = (int)(current % 10);
        carry = current / 10;

        if (i >= n->length && n->digits[i] > 0) {
            n->length = i + 1;
        }
    }
}

int div_bigint(BigInt *n, int divisor) {
    long long remainder = 0;

    for (int i = n->length - 1; i >= 0; --i) {
        long long current = n->digits[i] + remainder * 10;
        n->digits[i] = current / divisor;
        remainder = current % divisor;
    }

    while (n->length > 0 && n->digits[n->length - 1] == 0) {
        n->length--;
    }

    return (int)remainder;
}

long double bigint_to_long_double(BigInt *n) {
    long double result = 0;
    long double multiplier = 1;

    for (int i = 0; i < n->length; ++i) {
        result += (long double)n->digits[i] * multiplier;
        multiplier *= 10.0;
    }
    return result;
}

char *bigint_to_str(const BigInt *n) {
    static char str_buf[MAX_DIGITS + 1];

    int len = n->length;
    int j = 0;

    for (int i = len - 1; i >= 0; --i) {
        str_buf[j++] = (char)(n->digits[i] + '0');
    }

    str_buf[j] = '\0';
    return str_buf;
}

#pragma endregion BigInt

#pragma region AVX2 & Files

void check_cpu_support() {
#ifdef _WIN32
    int cpuInfo[4];
    __cpuid(cpuInfo, 7);
    g_has_avx2 = (cpuInfo[1] & (1 << 5)) != 0; // EBX bit 5
#else
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        g_has_avx2 = (ebx & (1 << 5)) != 0;
    }
#endif
}

#ifdef _MSC_VER
__declspec(noinline) 
#else
__attribute__((target("avx2"), noinline))
#endif
size_t find_next_p_avx2(const unsigned char *data, size_t len) {
    size_t i = 0;
    __m256i p_vec = _mm256_set1_epi8('p');

    for (; i + 32 <= len; i += 32) {
        __m256i haystack = _mm256_loadu_si256((const __m256i *)&data[i]);
        __m256i vmask = _mm256_cmpeq_epi8(haystack, p_vec);
        int mask = _mm256_movemask_epi8(vmask);

        if (mask != 0) {
#ifdef _WIN32
            unsigned long trailing;
            _BitScanForward(&trailing, (unsigned long)mask);
            return i + trailing;
#else
            return i + __builtin_ctz(mask);
#endif
        }
    }
    return i;
}

size_t find_next_p_portable(const unsigned char *data, size_t len) {
    size_t i = 0;
    
    for (; i + 8 <= len; i += 8) {
        uint64_t chunk;
        memcpy(&chunk, &data[i], 8);

        uint64_t x = chunk ^ 0x7070707070707070ULL;
        uint64_t zero_metadata = (x - 0x0101010101010101ULL) & ~x & 0x8080808080808080ULL;

        if (zero_metadata != 0) {
            return i;
        }
    }

    return i;
}

FILE* smart_open(const char *path, const char *mode) {
    if (g_no_hints) {
        return fopen(path, mode);
    }

#ifdef _WIN32
    DWORD access = (strchr(mode, 'r')) ? GENERIC_READ : GENERIC_WRITE;
    DWORD disp = (strchr(mode, 'r')) ? OPEN_EXISTING : CREATE_ALWAYS;
    HANDLE h = CreateFileA(path, access, FILE_SHARE_READ, NULL, disp, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    int fd = _open_osfhandle((intptr_t)h, (access == GENERIC_READ) ? _O_RDONLY : _O_WRONLY);
    return _fdopen(fd, mode);
#else
    FILE *f = fopen(path, mode);
    if (f) {
        int fd = fileno(f);
        posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
        posix_fadvise(fd, 0, 0, POSIX_FADV_NOREUSE);
    }
    return f;
#endif
}

#pragma endregion AVX2 & Files

#pragma region Compile & Encode

int has_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    return (dot && strcmp(dot, ".penis") == 0);
}

void compile_file(const char *input, const char *output) {
    FILE *in = smart_open(input, "rb");
    if (!in) {
        perror("fopen input");
        exit(1);
    }

    FILE *out = smart_open(output, "wb");
    if (!out) {
        perror("fopen output");
        fclose(in);
        exit(1);
    }

    setvbuf(in, NULL, _IOFBF, 1 << 20);
    setvbuf(out, NULL, _IOFBF, 1 << 20);

    unsigned char inbuf[1 << 15];
    unsigned char outbuf[1 << 15];
    size_t outpos = 0;

    int state = 0;
    int bangs = 0;

    int prev = -1;
    int first = 1;

    unsigned char byte = 0;
    int bit_pos = 0;

    size_t n;
    while ((n = fread(inbuf, 1, sizeof(inbuf), in)) > 0) {
        for (size_t i = 0; i < n; ++i) {
            if (state == 0) {
                size_t next_p;
                if (g_has_avx2) {
                    next_p = find_next_p_avx2(&inbuf[i], n - i);
                } else {
                    next_p = find_next_p_portable(&inbuf[i], n - i);
                }

                i += next_p;

                if (i >= n) break;
            }
            
            unsigned char c = inbuf[i];

            switch (state) {
                case 0:
                    state = (c == 'p') ? 1 : 0;
                    break;
                case 1:
                    state = (c == 'e') ? 2 : 0;
                    break;
                case 2:
                    state = (c == 'n') ? 3 : 0;
                    break;
                case 3:
                    state = (c == 'i') ? 4 : 0;
                    break;
                case 4:
                    if (c == 's') {
                        state = 5;
                        bangs = 0;
                    } else {
                        state = 0;
                    }
                    break;
                case 5:
                    if (c == '!') {
                        bangs++;
                    } else {
                        int bit;

                        if (first) {
                            bit = (bangs != 0);
                            first = 0;
                        } else {
                            int diff = bangs - prev;
                            if (diff == 1) {
                                bit = 0;
                            } else if (diff == 2) {
                                bit = 1;
                            } else {
                                fprintf(stderr, "Invalid jump (diff=%d)\n", diff);
                                exit(1);
                            }
                        }

                        prev = bangs;

                        byte = (byte << 1) | bit;

                        if (++bit_pos == 8) {
                            outbuf[outpos++] = byte;
                            if (outpos == sizeof(outbuf)) {
                                fwrite(outbuf, 1, outpos, out);
                                outpos = 0;
                            }

                            byte = 0;
                            bit_pos = 0;
                        }

                        state = 0;
                        i--;
                    }
                    break;
            }
        }
    }

    if (bit_pos > 0) {
        byte <<= (8 - bit_pos);
        outbuf[outpos++] = byte;
    }

    if (outpos > 0) {
        fwrite(outbuf, 1, outpos, out);
    }

    fclose(in);
    fclose(out);
    printf("Compiled successfully\n");
}

void encode_file(const char *input, const char *output) {
    FILE *in = smart_open(input, "rb");
    if (!in) {
        perror("fopen input");
        exit(1);
    }

    FILE *out = smart_open(output, "wb");
    if (!out) {
        perror("fopen output");
        fclose(in);
        exit(1);
    }

    setvbuf(in, NULL, _IOFBF, 1 << 20);
    setvbuf(out, NULL, _IOFBF, 1 << 20);

    unsigned char inbuf[1 << 15];
    unsigned char outbuf[1 << 15];
    size_t outpos = 0;

    char bangbuf[1024];
    memset(bangbuf, '!', sizeof(bangbuf));

    int prev_bangs = 0;
    int first = 1;

    size_t n;
    while ((n = fread(inbuf, 1, sizeof(inbuf), in)) > 0) {
        for (size_t k = 0; k < n; ++k) {
            unsigned char byte = inbuf[k];

            if (outpos > (sizeof(outbuf) - 2048)) {
                fwrite(outbuf, 1, outpos, out);
                outpos = 0;
            }

            for (int i = 7; i >= 0; --i) {
                int bit = (byte >> i) & 1;

                *(uint64_t *)(outbuf + outpos) = penis64;
                outpos += 5;

                int bangs;
                int delta = 1 + bit;
                bangs = first ? (bit ? 1 : 0) : (prev_bangs + delta);
                first = 0;

                memset(outbuf + outpos, '!', bangs);
                outpos += bangs;

                outbuf[outpos++] = ' ';
                prev_bangs = bangs;
            }
        }
    }

    if (outpos > 0) {
        fwrite(outbuf, 1, outpos, out);
    }

    fclose(in);
    fclose(out);
    printf("Encoded successfully\n");
}

#pragma endregion Compile & Encode

#pragma region Estimate

char *format_bigint_size(BigInt n) {
    static char buf[128];
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB", "RB", "QB"};
    int i = 0;
    double remainder = 0;

    while (i < 10) {
        if (n.length <= 18) {
            long long small_val = 0;
            long long power = 1;
            for (int j = 0; j < n.length; ++j) {
                small_val += (long long)n.digits[j] * power;
                power *= 10;
            }

            if (small_val < 1024) {
                double final_size = (double)small_val + (remainder / 1024.0);
                snprintf(buf, sizeof(buf), "%.2f %s", final_size, units[i]);
                return buf;
            }
        }

        remainder = div_bigint(&n, 1024);
        i++;
    }

    snprintf(buf, sizeof(buf), "Way too big %s", units[i]);
    return buf;
}

char *format_ll_size(long long bytes) {
    static char buf[64];
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int i = 0;

    double size = (double)bytes;
    while (size >= 1024 && i < 5) {
        size /= 1024;
        i++;
    }

    snprintf(buf, sizeof(buf), "%.2f %s", size, units[i]);
    return buf;
}

void estimate_size(const char *input, int fast) {
    FILE *in = smart_open(input, "rb");
    if (!in) {
        perror("fopen input");
        exit(1);
    }

    fseek64(in, 0, SEEK_END);
    long long total_bytes = ftell64(in);
    fseek64(in, 0, SEEK_SET);

    if (total_bytes <= 0) {
        printf("File is empty or size could not be determined.\n");
        fclose(in);
        return;
    }

    BigInt encoded_size;
    init_bigint(&encoded_size);

    if (fast) {
        long long n_bits = total_bytes * 8;

        add_long(&encoded_size, 5 * n_bits);
        multiply_long(&encoded_size, 5);

        BigInt term2;
        init_bigint(&term2);
        add_long(&term2, n_bits);
        multiply_long(&term2, n_bits + 1);
        multiply_long(&term2, 3);
        div_bigint(&term2, 4);

        add_bigint(&encoded_size, &term2);

        long double encoded_ld = bigint_to_long_double(&encoded_size);
        long double ratio = encoded_ld / (long double)total_bytes;

        printf("Original size:   %s (%lld bytes)\n", format_ll_size(total_bytes), total_bytes);
        printf("Encoded size:    %s (%s bytes)\n", format_bigint_size(encoded_size), bigint_to_str(&encoded_size));
        printf("Expansion ratio: %.2Lfx\n", ratio);
        return;
    }

    long long original_size = 0;
    long long prev_bangs = 0;

    int first = 1;
    int byte;

    time_t start_time = time(NULL);
    time_t last_update = 0;

    printf("Starting estimate...\n");

    while ((byte = fgetc(in)) != EOF) {
        original_size++;

        for (int i = 7; i >= 0; --i) {
            int bit = (byte >> i) & 1;

            long long current_bangs;
            if (first) {
                current_bangs = bit ? 1 : 0;
                first = 0;
            } else {
                current_bangs = prev_bangs + (bit ? 2 : 1);
            }

            add_long(&encoded_size, 5 + current_bangs);
            prev_bangs = current_bangs;
        }

        if (original_size % 1048576 == 0 || original_size == total_bytes) {
            time_t now = time(NULL);
            if (now > last_update) {
                double percent = ((double)original_size / (double)total_bytes) * 100.0;
                double elapsed = difftime(now, start_time);

                int remaining_s = 0;
                if (percent > 0) {
                    remaining_s = (int)((elapsed / percent) * (100.0 - percent));
                }

                int h = remaining_s / 3600;
                int m = (remaining_s % 3600) / 60;
                int s = remaining_s % 60;

                printf("\rProgress: %6.2f%% | Remaining: %02dh %02dm %02ds   ", percent, h, m, s);
                fflush(stdout);
                last_update = now;
            }
        }
    }

    fclose(in);

    printf("\rProgress: 100.00%% | Complete!                     \n\n");

    long double encoded_ld = bigint_to_long_double(&encoded_size);
    long double ratio = (original_size) > 0 ? (encoded_ld / (long double)original_size) : 0;

    printf("Bang length:     %lld\n", prev_bangs);
    printf("Original size:   %s (%lld bytes)\n", format_ll_size(original_size), original_size);
    printf("Encoded  size:   %s (%s bytes)\n", format_bigint_size(encoded_size), bigint_to_str(&encoded_size));
    printf("Expansion ratio: %.2Lfx\n", ratio);
}

#pragma endregion Estimate

#pragma region Main

int main(int argc, char **argv) {
    check_cpu_support();

    if (argc < 3) {
        printf("Usage:\n");
        printf("  %s [-c|-e|-s] <input> <output>\n", argv[0]);
        printf("AVX2 Support: %s\n", g_has_avx2 ? "Yes" : "No");
        return 1;
    }

    int mode = 0; // 0 auto, 1 comp, 2 enc
    int fast = 0;
    int argi = 1;

    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "-c") == 0) {
            mode = 1;
        } else if (strcmp(argv[argi], "-e") == 0) {
            mode = 2;
        } else if (strcmp(argv[argi], "-s") == 0) {
            mode = 3;
        } else if (strcmp(argv[argi], "--no-hints") == 0) {
            g_no_hints = 1;
        } else if (strcmp(argv[argi], "--fast") == 0) {
            fast = 1;
        } else {
            break;
        }

        argi++;
    }

    if (argi >= argc) {
        fprintf(stderr, "Error: Missing input file.\n");
        return 1;
    }

    const char *input = argv[argi++];
    const char *output = NULL;

    if (mode != 3) {
        if (argi >= argc) {
            fprintf(stderr, "Missing output file\n");
            return 1;
        }
        output = argv[argi];
    }

    if (mode == 0) {
        if (has_extension(input)) {
            mode = 1;
        } else {
            mode = 2;
        }
    }

    if (mode == 1) {
        compile_file(input, output);
    } else if (mode == 2) {
        encode_file(input, output);
    } else if (mode == 3) {
        estimate_size(input, fast);
    }

    return 0;
}

#pragma endregion Main