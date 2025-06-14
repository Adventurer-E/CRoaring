#define _GNU_SOURCE
#include <roaring/misc/configreport.h>
#include <roaring/roaring.h>

#include "benchmark.h"
#include "numbersfromtextfiles.h"
#include <x86intrin.h>
#include <time.h>

/**
 * Once you have collected all the integers, build the bitmaps.
 */
static roaring_bitmap_t **create_all_bitmaps(size_t *howmany,
                                             uint32_t **numbers, size_t count,
                                             bool runoptimize,
                                             bool copy_on_write) {
    if (numbers == NULL) return NULL;
    // printf("Constructing %d  bitmaps.\n", (int)count);
    roaring_bitmap_t **answer = malloc(sizeof(roaring_bitmap_t *) * count);
    for (size_t i = 0; i < count; i++) {
        // printf(".");
        fflush(stdout);
        answer[i] = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
        if (runoptimize) roaring_bitmap_run_optimize(answer[i]);
        roaring_bitmap_shrink_to_fit(answer[i]);
        roaring_bitmap_set_copy_on_write(answer[i], copy_on_write);
    }
    // printf("\n");
    return answer;
}

static void printusage(char *command) {
    // printf(" Try %s directory \n where directory could be benchmarks/realdata/census1881\n", command);
}

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

int main(int argc, char **argv) {
    tellmeall();
    int c;
    const char *extension = ".txt";
    bool copy_on_write = false;
    bool runoptimize = true;
    while ((c = getopt(argc, argv, "e:h")) != -1) switch (c) {
            case 'e':
                extension = optarg;
                break;
            case 'h':
                printusage(argv[0]);
                return 0;
            default:
                abort();
        }
    if (optind >= argc) {
        printusage(argv[0]);
        return -1;
    }
    char *dirname = argv[optind];
    size_t count;

    size_t *howmany = NULL;
    uint32_t **numbers =
        read_all_integer_files(dirname, extension, &howmany, &count);
    if (numbers == NULL) {
        // printf("I could not find or load any data file with extension %s in directory %s.\n", extension, dirname);
        return -1;
    }

    uint64_t cycles_start = 0, cycles_final = 0;

    RDTSC_START(cycles_start);
    roaring_bitmap_t **bitmaps =
        create_all_bitmaps(howmany, numbers, count, runoptimize, copy_on_write);
    RDTSC_FINAL(cycles_final);
    if (bitmaps == NULL) return -1;
    // printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);
    // printf("Creating %zu bitmaps took %" PRIu64 " cycles\n", count, cycles_final - cycles_start);
    
    size_t total_values = 0;
    for (int i = 0; i < (int)count; i++) {
        total_values += roaring_bitmap_get_cardinality(bitmaps[i]);
    }


    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count; i += 2) {
        roaring_bitmap_t *CI = roaring_bitmap_copy(bitmaps[i]);
        roaring_bitmap_free(CI);
    }
    RDTSC_FINAL(cycles_final);
    // printf("Copying and freeing %zu bitmaps took %" PRIu64 " cycles\n", count, cycles_final - cycles_start);

    uint64_t successive_and = 0;
    uint64_t successive_or = 0;
    for (int i = 0; i < (int)count - 1; ++i) {
        uint32_t c1 = (uint32_t)roaring_bitmap_get_cardinality(bitmaps[i]);
        uint32_t c2 = (uint32_t)roaring_bitmap_get_cardinality(bitmaps[i + 1]);
        RDTSC_START(cycles_start);
        roaring_bitmap_t *tempand = roaring_bitmap_and(bitmaps[i], bitmaps[i + 1]);
        RDTSC_FINAL(cycles_final);
        successive_and += cycles_final - cycles_start;

        uint32_t ci = (uint32_t)roaring_bitmap_get_cardinality(tempand);
        roaring_bitmap_free(tempand);
        RDTSC_START(cycles_start);
        roaring_bitmap_t *tempor = roaring_bitmap_or(bitmaps[i], bitmaps[i + 1]);
        RDTSC_FINAL(cycles_final);
        successive_or += cycles_final - cycles_start;

        uint32_t co = (uint32_t)roaring_bitmap_get_cardinality(tempor);
        roaring_bitmap_free(tempor);

        if (c1 + c2 != co + ci) {
            // printf(KRED "cardinalities are wrong somehow\n");
            // printf("c1 = %d, c2 = %d, co = %d, ci = %d\n", c1, c2, co, ci);
            return -1;
        }
    }
    // printf(" %zu successive bitmaps intersections took %" PRIu64 " cycles\n", count - 1, successive_and);
    // printf(" %zu successive bitmaps unions took %" PRIu64 " cycles\n", count - 1, successive_or);

    roaring_bitmap_t **copyofr = malloc(sizeof(roaring_bitmap_t *) * count);
    for (int i = 0; i < (int)count; i++) {
        copyofr[i] = roaring_bitmap_copy(bitmaps[i]);
    }
    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; i++) {
        roaring_bitmap_and_inplace(copyofr[i], bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    // printf(" %zu successive in-place bitmaps intersections took %" PRIu64 " cycles\n", count - 1, cycles_final - cycles_start);

    free(copyofr);
    copyofr = malloc(sizeof(roaring_bitmap_t *) * count);
    for (int i = 0; i < (int)count; i++) {
        copyofr[i] = roaring_bitmap_copy(bitmaps[i]);
    }
    RDTSC_START(cycles_start);
    for (int i = 0; i < (int)count - 1; i++) {
        roaring_bitmap_or_inplace(copyofr[i], bitmaps[i + 1]);
    }
    RDTSC_FINAL(cycles_final);
    // printf(" %zu successive in-place bitmaps unions took %" PRIu64 " cycles\n", count - 1, cycles_final - cycles_start);

    size_t total_count = 0;
    RDTSC_START(cycles_start);
    for (size_t i = 0; i < count; ++i) {
        roaring_bitmap_t *r = bitmaps[i];
        roaring_uint32_iterator_t j;
        roaring_iterator_init(r, &j);
        while (j.has_value) {
            total_count++;
            roaring_uint32_iterator_advance(&j);
        }
    }
    RDTSC_FINAL(cycles_final);
    // printf("Iterating over %zu bitmaps and %zu values took %" PRIu64 " cycles\n", count, total_count, cycles_final - cycles_start);

    size_t total_compressed_size = 0;
    uint64_t total_compression_cycles = 0;
    uint64_t total_decompression_cycles = 0;
    for (int i = 0; i < (int)count; i++) {
        size_t max_size = roaring_bitmap_portable_size_in_bytes(bitmaps[i]);
        char *buf = malloc(max_size);

        uint64_t start = __rdtsc();
        size_t compressed_size = roaring_bitmap_portable_serialize(bitmaps[i], buf);
        uint64_t end = __rdtsc();
        total_compression_cycles += (end - start);
        total_compressed_size += compressed_size;

        start = __rdtsc();
        roaring_bitmap_t *bm = roaring_bitmap_portable_deserialize(buf);
        end = __rdtsc();
        total_decompression_cycles += (end - start);

        roaring_bitmap_free(bm);
        free(buf);
    }

    size_t estimated_original_size = 0;
    for (int i = 0; i < (int)count; i++) {
        estimated_original_size += roaring_bitmap_get_cardinality(bitmaps[i]) * sizeof(uint32_t);
    }

    printf("Compression_ratio: %.4f\n",
           (double)total_compressed_size / estimated_original_size);
    printf("Compression_time: %.2f cycles per value\n",
           (double)total_compression_cycles / total_values);
    printf("Decompression_time: %.2f cycles per value\n",
           (double)total_decompression_cycles / total_values);

    uint64_t portable_cycle_count = 0, portable_frozen_cycle_count = 0,
             frozen_cycle_count = 0;
    for (int i = 0; i < (int)count; i++) {
        int size = roaring_bitmap_portable_size_in_bytes(bitmaps[i]);
        char *buf = malloc(size);
        roaring_bitmap_portable_serialize(bitmaps[i], buf);

        int frozen_size = roaring_bitmap_frozen_size_in_bytes(bitmaps[i]);
        char *frozen_buf = roaring_aligned_malloc(32, frozen_size);
        roaring_bitmap_frozen_serialize(bitmaps[i], frozen_buf);

        RDTSC_START(cycles_start);
        roaring_bitmap_t *r1 = roaring_bitmap_portable_deserialize(buf);
        RDTSC_FINAL(cycles_final);
        portable_cycle_count += cycles_final - cycles_start;

        RDTSC_START(cycles_start);
        roaring_bitmap_t *r2 = roaring_bitmap_portable_deserialize_frozen(buf);
        RDTSC_FINAL(cycles_final);
        portable_frozen_cycle_count += cycles_final - cycles_start;

        RDTSC_START(cycles_start);
        const roaring_bitmap_t *r3 =
            roaring_bitmap_frozen_view(frozen_buf, frozen_size);
        RDTSC_FINAL(cycles_final);
        frozen_cycle_count += cycles_final - cycles_start;

        roaring_bitmap_free(r1);
        roaring_bitmap_free(r2);
        roaring_bitmap_free(r3);
        free(buf);
        roaring_aligned_free(frozen_buf);
    }

    // printf("Deserializing %zu bitmaps took %" PRIu64 " cycles for portable format\n", count, portable_cycle_count);
    // printf("Deserializing %zu bitmaps took %" PRIu64 " cycles for portable frozen format\n", count, portable_frozen_cycle_count);
    // printf("Deserializing %zu bitmaps took %" PRIu64 " cycles for frozen format\n", count, frozen_cycle_count);

    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;
        roaring_bitmap_free(bitmaps[i]);
        bitmaps[i] = NULL;
    }
    free(bitmaps);
    free(howmany);
    free(numbers);

    return 0;
}

