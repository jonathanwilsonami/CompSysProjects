//jmw323
//Jonathan Wilson
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <strings.h>
#include "cachelab.h"

#include <math.h> 
/*
*Notes:
*t = m - (s + b) This is the number of tag bits
*S = 2 ^ s This is the number of sets
*E = This is the number of lines per set
*B = 2 ^ b This is the block size in bytes
*m = This is the number of physical (mainn memory) address bits
*M = 2 ^ m This is the max number of unique memory addresses
*
*Design - 
*Use que for keeping track of what was used last
*Polocy is LRU Least Recently Used
*Cahce is a 2D array
*struct casch_line cache[S][E]
*include the valid bit and tag bit
*use getopt() for more info use "man 3 getopt()
*Make use of malloc
*Must call PrintSummary()
*
*
*Args:
*-s index size (sets)
*-E associtivity (Number of lines per set)
*-b block bits
*-v Verbose mode
*
*/
/*************************************************************************************************/
//64 bit mem address
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
    int hits;
    int misses;
    int evictions;
    int s; 
    int b; /*See Notes*/
    int E; 
    int S; 
    int B; 
    
} cache_param_t;
/*****************************************************************************************************/
cache build_cache (long long num_sets, int num_lines, long long block_size) {

    cache newCache; 
    cache_set set;
    set_line line;
    int setIndex;
    int lineIndex;
	
	//allocate space for the cache
    newCache.sets = (cache_set *) malloc(sizeof(cache_set) * num_sets);

    for (setIndex = 0; setIndex < num_sets; setIndex++) 
    {
        set.lines =  (set_line *) malloc(sizeof(set_line) * num_lines);
        newCache.sets[setIndex] = set;

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

int get_LRU (cache_set this_set, cache_param_t par, int *used_lines) {

    int num_lines = par.E;
    
    int max_used = this_set.lines[0].access_count; 
    int min_used = this_set.lines[0].access_count;
    int min_used_index = 0;
    
    int lineIndex;
    set_line line; 

    for (lineIndex = 1; lineIndex < num_lines; lineIndex ++) {
        line = this_set.lines[lineIndex];
        if (min_used > line.access_count) {
            min_used_index = lineIndex; 
            min_used = line.access_count;
        }

        if (max_used < line.access_count) {
            max_used = line.access_count;
        }
    }

    used_lines[0] = min_used;
    used_lines[1] = max_used;
    
    return min_used_index;
    
} 

cache_param_t simulate_cache (cache this_cache, cache_param_t par, mem_addr_t address) {

        int lineIndex;
        int cache_full = 1;  

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
                if (line.tag == input_tag) { 

                    line.access_count++;
                    
                    par.hits++;
                    
                    query_set.lines[lineIndex] = line;
                }

            } else if (!(line.valid) && (cache_full)) {
                cache_full = 0;     
            }
        }   

        if (prev_hits == par.hits) { 
            
            par.misses++; 
            
        } else {
            return par; 
        }


        int *used_lines = (int*) malloc(sizeof(int) * 2);
        int min_used_index = get_LRU(query_set, par, used_lines);   

        if (cache_full) { 
        
            par.evictions++;

            query_set.lines[min_used_index].tag = input_tag;
            query_set.lines[min_used_index].access_count = used_lines[1] + 1;
        } else { 

            int empty_line_index = get_empty_line(query_set, par);

            query_set.lines[empty_line_index].tag = input_tag;
            query_set.lines[empty_line_index].valid = 1;
            query_set.lines[empty_line_index].access_count = used_lines[1] + 1;
        }                       

        free(used_lines);
        return par;

} 

long long bit_pow(int power) {
    long long result = 1;
    result = result << power;
    return result;
} 

int main(int argc, char **argv)
{
    cache this_cache;
    cache_param_t par;
    bzero(&par, sizeof(par));

    char cmd; 
    mem_addr_t address;
    int size;
    long long num_sets;
    long long block_size;
    FILE *read_trace;
    char *trace_file;
    char c;
	
	//Get arguments on the command line. Use switch case to iterate through them.
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
        default:
            printf("No Commands");
            exit(1);
        }
    }

    num_sets = pow(2.0, par.s);
    block_size = bit_pow(par.b); 
    par.hits = 0;
    par.misses = 0;
    par.evictions = 0;

    this_cache = build_cache(num_sets, par.E, block_size); 

    read_trace = fopen(trace_file,"r");
	
	//Read through file and scan for char.
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

    printSummary(par.hits, par.misses, par.evictions);
    clear_cache(this_cache, num_sets, par.E, block_size);
    fclose(read_trace);

    return 0;
}