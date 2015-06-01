#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include "heap.h"
#include "msg.h"

static char * BASE;
static char * FREE;
static size_t total_bytes;
static size_t used_bytes;

static struct Node {
    void * ptr;
    size_t bytes;
    char file[256];
    int line;
} NODES[1024];

static int top;

static size_t max_usage = 0;

int heap_init(size_t allocation) {
    if(allocation == 0) {
        BASE = NULL;
    } else {
        BASE = malloc(allocation);
        total_bytes = allocation;
        if(BASE == NULL) {
            msg_abort(9999, "Failed to reserve %td bytes\n", allocation);
        } 
        FREE = BASE;
        used_bytes = 0;
        top = 0;
    }
    return 0;
}

void * heap_allocate0(size_t bytes, char * filename, int lineno) {
    /* align to 4K boundary */
    bytes = 4096 * (size_t)((bytes + 4096 - 1) / 4096);
    if(BASE != NULL && used_bytes + bytes > total_bytes) {
        int i = 0;
        for(i = 0; i < top; i ++) {
            printf("%p : %td : %s:%d \n", 
                NODES[i].ptr, NODES[i].bytes, NODES[i].file, NODES[i].line);
        }
        msg_abort(9999, "Failed to allocated %td bytes, free=%td %s:%d\n", bytes, total_bytes - used_bytes, filename, lineno);
        return NULL;
    } else {
        void * ptr;
        if(BASE != NULL) {
            ptr = FREE;
            FREE += bytes;
        } else {
            ptr = malloc(bytes);
        }
        used_bytes += bytes;
        if(used_bytes > max_usage) {
            max_usage = used_bytes;
        }
        NODES[top].ptr = ptr;
        NODES[top].bytes = bytes;
        strcpy(NODES[top].file, filename);
        NODES[top].line = lineno;
        top ++;
        return ptr;
    }
}

size_t heap_get_max_usage() {
    return max_usage;
}

size_t heap_get_total_bytes() {
    return total_bytes;
}

size_t heap_get_free_bytes() {
    return 4096 * ((total_bytes - used_bytes) / 4096 - 1);
}

int heap_return0(void * ptr) {
    top --;
    if (NODES[top].ptr != ptr) {
        msg_abort(9999, "Ptr %p is not last allocated block (%p of size %td)\n", 
                ptr, NODES[top].ptr, NODES[top].bytes);
    } else{
        size_t bytes = NODES[top].bytes;
        if(BASE != NULL) {
            FREE -= bytes;
        } else {
            free(ptr);
        }
        used_bytes -= bytes;
    }
    return 0;
}

