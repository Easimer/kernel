#ifndef KERNEL_RING_BUFFER_H
#define KERNEL_RING_BUFFER_H

#include "common.h"

// Simple fixed-size ring buffer

template<typename ElemT, u32 Size>
struct Ring_Buffer {
    u32 rd;
    u32 wr;
    ElemT elems[Size];

    Ring_Buffer() : rd(0), wr(0) {}

    void reset() {
        rd = wr = 0;
    }
    
    ElemT& push(const ElemT& elem) {
        elems[wr] = elem;
        ElemT& ret = elems[wr];
        wr = (wr + 1) % Size;
        if(wr == rd) {
            rd = (rd + 1) % Size;
        }
        
        return ret;
    }

    bool pop(ElemT* out) {
        bool ret = false;

        if(rd != wr) {
            ret = true;
            *out = elems[rd];
            rd = (rd + 1) % Size;
        }

        return ret;
    }
};

#endif /* KERNEL_RING_BUFFER_H */
