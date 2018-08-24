
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <strings.h>
#include "cachelab.h"
#include <math.h> 

typedef unsigned long long int mem_addr_t;

typedef struct {
    int valid; 
    int access_count; 
    mem_addr_t tag;
    char *block;
} set_line;

typedef struct {
    set_line *lines;
} cache_set;

typedef struct {
    cache_set *sets;
} cache;

typedef struct {
    int s; 
    int b; 
    int E; 
    int S; 
    int B; 
    
    int hits;
    int misses;
    int evictions;
} cache_param_t;




******************************************************************************
cache build_cache (long long num_sets, int num_lines, long long block_size) {

    cache newCache; 
    cache_set set;
    set_line line;

    int setIndex;
    int lineIndex;

    /* from lab instructions, use malloc function to allocate storage */
    newCache.sets = (cache_set *) malloc(sizeof(cache_set) * num_sets);

    /* cache is an array of s sets with index 0 to s-1 */
    for (setIndex = 0; setIndex < num_sets; setIndex++) 
    {
        /* build E lines for each set */
        set.lines =  (set_line *) malloc(sizeof(set_line) * num_lines);
        newCache.sets[setIndex] = set;

        /* build b block offset bits for each line */
        for (lineIndex = 0; lineIndex < num_lines; lineIndex ++) 
        {
            line.access_count = 0;
            line.valid = 0;
            line.tag = 0; 
            set.lines[lineIndex] = line;    
        }
    } 

    return newCache;

} 

************************************************************************************************

/* call free function to clean up cache after main simulation is run */
void clear_cache(cache this_cache, long long num_sets, int num_lines, long long block_size) 
{
    int setIndex;

    for (setIndex = 0; setIndex < num_sets; setIndex ++) {
        cache_set set = this_cache.sets[setIndex];
        if (set.lines != NULL) {    
            free(set.lines);
        }
    } 

    if (this_cache.sets != NULL) {
        free(this_cache.sets);
    }
} 

*******************************************************************

/* find an empty line in a set by checking if the valid tag is set to 0 or 1 
 * if the valid tag is 0, then the line is empty
 */
int get_empty_line(cache_set set, cache_param_t par) {

    set_line line;
    int num_lines = par.E;
    int i;

    for (i = 0; i < num_lines; i ++) {
        line = set.lines[i];
        if (line.valid == 0) {
            return i;
        }
    }
    
    return 0;
} 

***********************************************************************************

/* get_LRU finds and returns the index of LRU line */ 
int get_LRU (cache_set this_set, cache_param_t par, int *used_lines) {

    int num_lines = par.E;
    
    /* initialize both the most and least frequently used lines 
     * equal to the given set's last used line
     */
    int max_used = this_set.lines[0].access_count; 
    int min_used = this_set.lines[0].access_count;
    
    /* initialize and keep track of the LRU to return */
    int min_used_index = 0;
    
    int lineIndex;
    set_line line; 

    for (lineIndex = 1; lineIndex < num_lines; lineIndex ++) {
    
        /* we start searching through all the lines in the set */
        line = this_set.lines[lineIndex];

        /* if the current min used line is greater than the number of times this line has been accessed,
         * store the index of this line to be returned later 
         * and update value of min used line *
         */
        if (min_used > line.access_count) {
            min_used_index = lineIndex; 
            min_used = line.access_count;
        }

        /* otherwise, if the current max used line is less than the current line,
         * update the value of the max used line 
         */
        if (max_used < line.access_count) {
            max_used = line.access_count;
        }
    }

    /* used_lines[0] is LRU 
     * used_lines[1] is current LRU counter or most recently used 
     */
    used_lines[0] = min_used;
    used_lines[1] = max_used;
    
    /* return the index of the LRU line from the inputted set */ 
    return min_used_index;
    
} 

******************************************************************

/* runs the trace simulation */
cache_param_t simulate_cache (cache this_cache, cache_param_t par, mem_addr_t address) {

        int lineIndex;
        int cache_full = 1;  /* flag if cache does not have empty lines */

        int num_lines = par.E;
        int prev_hits = par.hits;

        int tag_size = (64 - (par.s + par.b));

        unsigned long long temp = address << (tag_size);
        unsigned long long setIndex = temp >> (tag_size + par.b);

        mem_addr_t input_tag = address >> (par.s + par.b);

        cache_set query_set = this_cache.sets[setIndex];

        for (lineIndex = 0; lineIndex < num_lines; lineIndex ++) {

            set_line line = query_set.lines[lineIndex];

            if (line.valid) {
                if (line.tag == input_tag) { /* found the right tag - cache hit */

                    line.access_count++;
                    
                    par.hits++;
                    
                    query_set.lines[lineIndex] = line;
                }

            } else if (!(line.valid) && (cache_full)) {
                cache_full = 0;     
            }
        }   

        if (prev_hits == par.hits) { /* par.hits wasn't incremented, so hit wasn't found - cache miss */
            
            par.misses++; 
            
        } else {
            return par; /* data was hit and is already in cache, exit function */
        }

        /* we didn't find a hit, so continue with cache miss
         * evict by getting LRU/ writing to first empty line in this set
         */

        int *used_lines = (int*) malloc(sizeof(int) * 2);
        int min_used_index = get_LRU(query_set, par, used_lines);   

        if (cache_full) { /* if there are no empty lines in cache, must overwrite */
        
            par.evictions++;

            /* write and replace LRU */
            query_set.lines[min_used_index].tag = input_tag;
            query_set.lines[min_used_index].access_count = used_lines[1] + 1;
        } else { /* there is at least one empty line we can write to */

            int empty_line_index = get_empty_line(query_set, par);

            // update valid/ tag bits with the input cache's at the empty line 
            query_set.lines[empty_line_index].tag = input_tag;
            query_set.lines[empty_line_index].valid = 1;
            query_set.lines[empty_line_index].access_count = used_lines[1] + 1;
        }                       

        free(used_lines);
        return par;

}

****************************************************************************

/* instead of explicitly defining similar to pow(2.0, exp), just bit shift */
long long bit_pow(int power) {
    long long result = 1;
    result = result << power;
    return result;
} 
********************************************************************************
/* main takes commands as input and prints the cache hits, misses, and evictions */
int main(int argc, char **argv)
{
    cache this_cache;
    cache_param_t par;
    bzero(&par, sizeof(par));

    long long num_sets;
    long long block_size;

    FILE *read_trace;

    char cmd; /* cmd will take in the operation address I, L, S, or M */
    mem_addr_t address;
    int size;

    char *trace_file;
    char c;
    while( (c=getopt(argc,argv,"s:E:b:t:vh")) != -1){
        switch(c){
        case 's':
            par.s = atoi(optarg);
            break;
        case 'E':
            par.E = atoi(optarg);
            break;
        case 'b':
            par.b = atoi(optarg);
            break;
        case 't':
            trace_file = optarg;
            break;
        case 'v':
            verbosity = 1;
            break;
        case 'h':
            printUsage(argv);
            exit(0);
        default:
            printUsage(argv);
            exit(1);
        }
    }
    if (par.s == 0 || par.E == 0 || par.b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        printUsage(argv);
        exit(1);
    }

    /* compute S and B based on information passed in; S = 2^s and B = 2^b */
    num_sets = pow(2.0, par.s);
    block_size = bit_pow(par.b); 
    par.hits = 0;
    par.misses = 0;
    par.evictions = 0;

    this_cache = build_cache(num_sets, par.E, block_size); /* build_cache takes as input sets, lines, and blocks */

    read_trace = fopen(trace_file,"r");

    /* rest of simulator routine reads commands in */
   	if (read_trace != NULL) {
        while (fscanf(read_trace, " %c %llx,%d", &cmd, &address, &size) == 3) {
            switch(cmd) {
                case 'I':
                break;
                case 'L':
                    par = simulate_cache(this_cache, par, address);
                break;
                case 'S':
                    par = simulate_cache(this_cache, par, address);
                break;
                case 'M':
                    par = simulate_cache(this_cache, par, address);
                    par = simulate_cache(this_cache, par, address);	
                break;
                default:
                break;
            }
        }
    }

    /* print out real results */
    printSummary(par.hits, par.misses, par.evictions);

    /* clean up cache resources */
    clear_cache(this_cache, num_sets, par.E, block_size);
    fclose(read_trace);

    return 0;
}

