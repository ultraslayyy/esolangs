#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int has_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return 0;

    if (strcmp(dot, ".penis") == 0) {
        return 1;
    } else if (strcmp(dot, ".peen") == 0) {
        return 1;
    } else if (strcmp(dot, ".peenar")) {
        return 1;
    }

    return 0;
}

int read_token(FILE *in) {
    int c;

    while ((c = fgetc(in)) != EOF) {
        if (c != 'p') continue;

        if (fgetc(in) != 'e') continue;
        if (fgetc(in) != 'n') continue;
        if (fgetc(in) != 'i') continue;
        if (fgetc(in) != 's') continue;

        int bangs = 0;
        while ((c = fgetc(in)) == '!') {
            bangs++;
        }

        if (c != EOF) {
            ungetc(c, in);
        }

        return bangs;
    }

    return -1;
}

void compile_file(const char *input, const char *output) {
    FILE *in = fopen(input, "r");
    if (!in) {
        perror("fopen input");
        exit(1);
    }

    FILE *out = fopen(output, "wb");
    if (!out) {
        perror("fopen output");
        fclose(in);
        exit(1);
    }

    int prev = -1;
    int first = 1;

    unsigned char byte = 0;
    int bit_pos = 0;

    while (1) {
        int bangs = read_token(in);
        if (bangs < 0) break;

        int bit;

        if (first) {
            bit = (bangs == 0) ? 0 : 1;
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
        bit_pos++;

        if (bit_pos == 8) {
            fwrite(&byte, 1, 1, out);
            byte = 0;
            bit_pos = 0;
        }
    }

    if (bit_pos > 0) {
        byte <<= (8 - bit_pos);
        fwrite(&byte, 1, 1, out);
    }

    fclose(in);
    fclose(out);
    printf("Compiled successfully\n");
}

void encode_file(const char *input, const char *output) {
    FILE *in = fopen(input, "rb");
    if (!in) {
        perror("fopen input");
        exit(1);
    }

    FILE *out = fopen(output, "w");
    if (!out) {
        perror("fopen output");
        fclose(in);
        exit(1);
    }

    int prev_bangs = 0;
    int first = 1;

    int byte;
    while ((byte = fgetc(in)) != EOF) {
        for (int i = 7; i >= 0; --i) {
            int bit = (byte >> i) & 1;

            int bangs;
            if (first) {
                bangs = bit ? 1 : 0;
                first = 0;
            } else {
                bangs = prev_bangs + (bit ? 2 : 1);
            }

            fprintf(out, "penis");
            for (int j = 0; j < bangs; ++j) {
                fputc('!', out);
            }
            fputc(' ', out);

            prev_bangs = bangs;
        }
    }

    fclose(in);
    fclose(out);
    printf("Encoded successfully\n");
}

char *format_size(long long bytes) {
    static char buf[64];

    const char *units[] = {"B", "KB", "MB", "GB"};
    int i = 0;
    double size = (double)bytes;

    while (size > 1024 && i < 3) {
        size /= 1024;
        i++;
    }

    snprintf(buf, sizeof(buf), "%.2f %s", size, units[i]);
    return buf;
}

void estimate_size(const char *input) {
    FILE *in = fopen(input, "rb");
    if (!in) {
        perror("fopen input");
        exit(1);
    }

    long long original_size = 0;
    long long encoded_size = 0;
    long long prev_bangs = 0;

    int first = 1;
    int byte;

    while ((byte = fgetc(in)) != EOF) {
        original_size++;

        for (int i = 7; i >= 0; --i) {
            int bit = (byte >> i) & 1;

            long long bangs;
            if (first) {
                bangs = bit ? 1 : 0;
                first = 0;
            } else {
                bangs = prev_bangs + (bit ? 2 : 1);
            }

            encoded_size += 5 + bangs;
            prev_bangs = bangs;
        }
    }

    fclose(in);

    double ratio = 0.0;
    if (original_size > 0) {
        ratio = (double)encoded_size / (double)original_size;
    }

    printf("Original size:   %s (%lld bytes)\n", format_size(original_size), original_size);
    printf("Encoded  size:   %s (%lld bytes)\n", format_size(encoded_size), encoded_size);
    printf("Expansion ratio: %.2fx\n", ratio);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage:\n");
        printf("  %s [-c|-e] <input> <output>\n", argv[0]);
        return 1;
    }

    int mode = 0; // 0 auto, 1 comp, 2 enc
    int argi = 1;

    if (strcmp(argv[1], "-c") == 0) {
        mode = 1;
        argi++;
    } else if (strcmp(argv[1], "-e") == 0) {
        mode = 2;
        argi++;
    } else if (strcmp(argv[1], "-s") == 0) {
        mode = 3;
        argi++;
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
        estimate_size(input);
    }

    return 0;
}