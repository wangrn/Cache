#ifndef BLOCK_H
#define BLOCK_H

#include <iostream>
using namespace std;

class block
{
public:
    block()
    {
        cap = 0;
        offset = 0;
        frd = 0;
    }
    
    unsigned int cap;
    unsigned int offset;
    unsigned char* frd;
    
    unsigned int usable_room()
    {
        return cap-offset;
    }
    void *apply()
    {
        return frd+offset;
    }
    void eat(unsigned int size)
    {
        if (offset + size <= cap)
            offset += size;
    }
    void clear()
    {
        offset = 0;
    }
    
    bool create(unsigned int c)
    {
        frd = (unsigned char*) malloc(c);
        if (frd == 0) return false;
        offset = 0;
        cap = c;
        return true;
    }
    void free_memory()
    {
        if (frd) free(frd);
        frd = 0;
        cap = offset = 0;
    }
};

#endif
