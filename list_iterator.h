#ifndef LIST_ITERATOR_H
#define LIST_ITERATOR_H

#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <set>
#include <tr1/unordered_set>
#include <string>

using namespace std;

class list_iterator
{
public:
    list_iterator(unsigned char* _d, unsigned _bytes):data(_d),bytes(_bytes)
    {
        offset = 0;
    }
    unsigned char* data;
    unsigned offset;
    unsigned bytes;

    unsigned char* next(unsigned& uin, unsigned& len)
    {
        uin = 0;
        len = 0;

        if (offset >= bytes || bytes - offset < sizeof(unsigned)*2)
            return 0;

        uin = *((unsigned*)(data + offset));
        offset += sizeof(unsigned);
        len = *((unsigned*)(data + offset));
        offset += sizeof(unsigned);

        //overfloat?
        if (offset + len > bytes)
            return 0;

        unsigned char * ret = data + offset;
        offset += len;

        return ret;
    }
private:
    list_iterator() {}
};
#endif
