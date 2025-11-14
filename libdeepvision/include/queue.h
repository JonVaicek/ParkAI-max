#ifndef DVQUEUE_H
#define DVQUEUE_H


#include <vector>

class FrameQueue{
    std::vector<unsigned char *> images;
    
    public:
    FrameQueue(){};
};


#endif