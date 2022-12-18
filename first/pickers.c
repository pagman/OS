
#include <stdlib.h>

#include "pickers.h"

int pickSegment(int S) {
    static int s = -1;
    
    if ((rand()%10) < 7 && s != -1) {
        return s;
    } else {
        return rand()%S;
    }
}

int pickLine(int SF) {
    return rand()%SF;
}
