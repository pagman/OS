
#ifndef SEGMENT_H
#define SEGMENT_H

#include <stdbool.h>

typedef struct Segment {
    int id;
    char * buffer;
    bool loaded;
} Segment;

void load(Segment * segment, FILE * fp, int segment_number, int SF);

void copyLine(Segment * segment, int line_number, char * out);
#endif /* SEGMENT_H */

