#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

typedef enum {dm, fa} cache_map_t;
typedef enum {uc, sc} cache_org_t;
typedef enum {instruction, data} access_t;

typedef struct {
    uint32_t address;
    access_t accesstype;
} mem_access_t;

typedef struct {
    uint64_t accesses;
    uint64_t hits;
    // You can declare additional statistics if
    // you like, however you are now allowed to
    // remove the accesses or hits
} cache_stat_t;


// DECLARE CACHES AND COUNTERS FOR THE STATS HERE


uint32_t cache_size;
uint32_t block_size = 64;
cache_map_t cache_mapping;
cache_org_t cache_org;

// Derivated parameters:
uint32_t cache_size_single;
uint32_t number_of_blocks;
uint32_t num_bits_block_offset;
uint32_t num_bits_index;
uint32_t num_bits_tag;

// Bit masks and shifts:
uint32_t block_offset_mask;
uint32_t index_mask;
uint32_t tag_mask;

// Pointers to actual cache
uint32_t *cache_ptr;
uint32_t *tags_ptr;

// Temp variables
uint32_t accesstype_offset;
uint32_t indexx;
uint32_t tag;
uint32_t hit;

// USE THIS FOR YOUR CACHE STATISTICS
cache_stat_t cache_statistics;


/* Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access
 * 2) memory address
 */
mem_access_t read_transaction(FILE *ptr_file) {
    char buf[1000];
    char* token;
    char* string = buf;
    mem_access_t access;

    if (fgets(buf,1000, ptr_file)!=NULL) {

        /* Get the access type */
        token = strsep(&string, " \n");        
        if (strcmp(token,"I") == 0) {
            access.accesstype = instruction;
        } else if (strcmp(token,"D") == 0) {
            access.accesstype = data;
        } else {
            printf("Unkown access type\n");
            exit(0);
        }
        
        /* Get the access type */        
        token = strsep(&string, " \n");
        access.address = (uint32_t)strtol(token, NULL, 16);

        return access;
    }

    /* If there are no more entries in the file,  
     * return an address 0 that will terminate the infinite loop in main
     */
    access.address = 0;
    return access;
}


// Calculates floor(log_2(a))
uint32_t log2_floor(uint32_t a) {
    uint32_t b = 31;
    if (a == 0) return 0xffffffff;
    while (1) {
        if ((a & 0x80000000) == 0x80000000) return b;
        a = a << 1;
        b--;
    }
}


void main(int argc, char** argv)
{

    // Reset statistics:
    memset(&cache_statistics, 0, sizeof(cache_stat_t));

    /* Read command-line parameters and initialize:
     * cache_size, cache_mapping and cache_org variables
     */

    if ( argc != 4 ) { /* argc should be 2 for correct execution */
        printf("Usage: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] [cache organization: uc|sc]\n");
        exit(0);
    } else  {
        /* argv[0] is program name, parameters start with argv[1] */

        /* Set cache size */
        cache_size = atoi(argv[1]);

        /* Set Cache Mapping */
        if (strcmp(argv[2], "dm") == 0) {
            cache_mapping = dm;
        } else if (strcmp(argv[2], "fa") == 0) {
            cache_mapping = fa;
        } else {
            printf("Unknown cache mapping\n");
            exit(0);
        }

        /* Set Cache Organization */
        if (strcmp(argv[3], "uc") == 0) {
            cache_org = uc;
        } else if (strcmp(argv[3], "sc") == 0) {
            cache_org = sc;
        } else {
            printf("Unknown cache organization\n");
            exit(0);
        }
    }


    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file =fopen("trace_files/trace_3.txt","r");
    if (!ptr_file) {
        printf("Unable to open the trace file\n");
        exit(1);
    }

    // Calculate derivated parameters:
    cache_size_single      = cache_org == uc ? cache_size : cache_size / 2;
    number_of_blocks       = cache_size_single / block_size;
    num_bits_block_offset  = log2_floor(block_size);
    num_bits_index         = cache_mapping == dm ? log2_floor(number_of_blocks) : 0;
    num_bits_tag           = 32 - num_bits_block_offset - num_bits_index;

    // Calculate masks:
    block_offset_mask = 0;
    for (int i = 0; i < num_bits_block_offset; i++) block_offset_mask |= 1 << i;
    index_mask = 0;
    for (int i = num_bits_block_offset; i < num_bits_block_offset + num_bits_index; i++) index_mask |= 1 << i;
    tag_mask = 0;
    for (int i = num_bits_block_offset + num_bits_index; i < 32; i++) tag_mask |= 1 << i;

    // Allocate memory for cache:
    // cache_ptr = malloc(cache_size);
    switch (cache_org) {
    case uc:
        tags_ptr = malloc(4 * number_of_blocks);
        break;
    case sc:
        tags_ptr = malloc(2 * 4 * number_of_blocks);
        break;
    }

    printf("Cache size:                      %d\n", cache_size);
    printf("Block size:                      %d\n", block_size);
    printf("Cache mapping:                   %s\n", cache_mapping == dm ? "dm" : "fa");
    printf("Cache organization:              %s\n", cache_org == uc ? "uc" : "sc");
    printf("Cache size single:               %d\n", cache_size_single);
    printf("Number of blocks:                %d\n", number_of_blocks);
    printf("Number of bits for block offset: %d\n", num_bits_block_offset);
    printf("Number of bits for index:        %d\n", num_bits_index);
    printf("Number of bits for tag:          %d\n", num_bits_tag);
    printf("Block offset mask:               %x\n", block_offset_mask);
    printf("Index mask:                      %x\n", index_mask);
    printf("Tag mask:                        %x\n", tag_mask);
    printf("Cache pointer:                   %lx\n", (uint64_t) cache_ptr);
    printf("Tags pointer:                    %lx\n", (uint64_t) tags_ptr);
    printf("\n");
    
    /* Loop until whole trace file has been read */
    mem_access_t access;
    while(1) {
        access = read_transaction(ptr_file);
        //If no transactions left, break out of loop
        if (access.address == 0)
            break;
        // printf("%s %x\n", access.accesstype == instruction ? "I" : "D", access.address);
        /* Do a cache access */
        // ADD YOUR CODE HERE

        cache_statistics.accesses++;
        
        accesstype_offset = cache_org == uc ? 0 : (access.accesstype * number_of_blocks);
        indexx = ((access.address & index_mask) >> num_bits_block_offset);
        tag = (access.address & tag_mask); // >> (num_bits_index + number_of_blocks);
        hit = 0;

        switch (cache_mapping) {
        case dm:
            // tag: access.address & tag_mask
            // tag in cache: tags_ptr[(access.address & index_mask) >> num_bits_block_offset]
            // if tag == tag in cache: there is a hit
            if (tag == tags_ptr[accesstype_offset + indexx]) {
                cache_statistics.hits++;
                hit = 1;
            }
            // store tag in tags_ptr
            tags_ptr[accesstype_offset + indexx] = tag;
            break;
        case fa:
            for (int i = 0; i < number_of_blocks; i++) {
                // tag: access.address & tag_mask
                // tag in cache: tags_ptr[i]
                // if tag == tag in cache: there is a hit
                if (tag == tags_ptr[accesstype_offset + i]) {
                    cache_statistics.hits++;
                    hit = 1;
                    break;
                }
            }
            // push tag to tags_ptr FIFO
            for (int i = number_of_blocks - 2; i >= 0; i--) {
                tags_ptr[accesstype_offset + i + 1] = tags_ptr[accesstype_offset + i];
            }
            tags_ptr[accesstype_offset + 0] = tag;
            break;
        }

        printf("%s %x | %s\n", access.accesstype == instruction ? "I" : "D", access.address, hit ? "hit" : "miss");
    }

    /* Print the statistics */
    // DO NOT CHANGE THE FOLLOWING LINES!
    printf("\nCache Statistics\n");
    printf("-----------------\n\n");
    printf("Accesses: %ld\n", cache_statistics.accesses);
    printf("Hits:     %ld\n", cache_statistics.hits);
    printf("Hit Rate: %.4f\n", (double) cache_statistics.hits / cache_statistics.accesses);
    // You can extend the memory statistic printing if you like!

    /* Close the trace file */
    fclose(ptr_file);

}
