
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "segment.h"
#include "defines.h"


void load(Segment * segment, FILE * fp, int segment_number, int SF) {
    int L = 0;
    char ch;

    rewind(fp);
    
    if (segment->loaded) {
        printf("### SEGMENT REMOVED: %d \n", segment->id);
    }

    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\n') {
            L++;
        }
        if (L == segment_number * SF) {
            break;
        }
    }

    char * iter = segment->buffer;

    memset(segment->buffer, 0, MAX_LINE_SIZE);

    L = 0;

    while ((ch = fgetc(fp)) != EOF && iter - segment->buffer < MAX_LINE_SIZE) {
        *iter = ch;
        iter++;

        if (ch == '\0') {
            break;
        }

        if (ch == '\n') {
            L++;

            if (L == SF) {
                break;
            }
        }
    }
    
    *iter = '\0';
    
    printf("### SEGMENT LOADED: %d \n", segment_number);
    
    segment->loaded = true;
    segment->id = segment_number;
}

void copyLine(Segment * segment, int line_number, char * out) {
    int L = 0;
    char * ch = segment->buffer;
    char * iter = out;
    
    while (*ch != '\0' && line_number > 0) {
        if (*ch == '\n') {
            L++;
        }
        if (L == line_number) {
            ch++;
            break;
        }
        ch++;
    }
        
   
    while (*ch != '\0' && *ch != '\n') {
        *iter = *ch;
        ch++;
        iter++;
    }    
    
    *iter = '\0';    
}