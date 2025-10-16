#ifndef STREAMMUXER_H
#define STREAMMUXER_H

#include "camstream.h"
#include "detector.h"

struct FrameInfo {
    uchar *idata = nullptr;
    uint64_t nbytes = 0;
    uint32_t age = 0;
    uint32_t nfailed = 0;
    bool ready = false;
    bool read=true;
};

class StreamMuxer{
    // std::vector<Pipeline *> pipes;
    const static int MAX_STREAMS = 128;
    std::vector<vstream *> sources;
    std::vector<StreamCtrl *> src_handles;
    std::vector<FrameInfo> frames;
    std::thread mux_thread;

    int muxer_thread(void){
        int ret = 0;
        while (true){
            for (int i = 0; i < src_handles.size(); i++){
                if(frames[i].ready == false && frames[i].read == true){
                    ret = update_frame(i);
                    if(ret){
                        //src_handles[i]->noframe = false;
                        frames[i].nfailed = 0;
                        std::cout << i <<" - New Frame\n";
                        frames[i].ready = true;
                        frames[i].read = false;
                        frames[i].age = 0;
                    }
                    else{
                        frames[i].nfailed++;
                        if(frames[i].nfailed >= 500){
                            //src_handles[i]->noframe = true;
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    public:
    StreamMuxer(void){
        mux_thread = std::thread([this](){muxer_thread();});
    };

    int link_stream(vstream *src, StreamCtrl *src_handle){
        if(sources.size() >= MAX_STREAMS){
            std::cerr << "Maximal amount of streams reached\n";
            return 0;
        }
        frames.push_back(FrameInfo{});
        sources.push_back(src);
        src_handles.push_back(src_handle);
        return 1;
    }

    int update_frame(uint32_t id){
        uchar *img = nullptr;
        uint64_t nbytes;
        uint32_t ret = pull_image(src_handles[id], JPEG, &img, &nbytes);
        if (!ret){
            return 0;
        }
        // if (frames[id].idata != nullptr){

        //     free(img);
        //     nbytes = 0;
        //     return 0;
        // }
        std::cout << "ID - " << id << " Frame Pulled Successfully\n";
        frames[id].nbytes = nbytes;
        frames[id].idata = (uchar *)malloc(nbytes);
        memcpy(frames[id].idata, img, nbytes);
        free(img);
        nbytes = 0;
        std::cout << "ID - " << id << " Frame Copied Successfully\n";
        return 1;
    }

    uint32_t pull_valid_frame(uchar **data, uint64_t *nbytes){
        uint32_t id=128; // will be 0 by default
        //priority for oldest frames
        uint32_t age = 0;
        for (int i = 0; i < frames.size(); i++){
            if (frames[i].ready == true && frames[i].read == false){
                if (frames[i].age > age){
                    id = i;
                    age = frames[i].age;
                }
                frames[i].age++; //Increment the age
            }
        }
        if (id >= frames.size()){
            return 128;
        }
        //logic to return frames
        *data = frames[id].idata;
        *nbytes = frames[id].nbytes;
        //frames[id].read = true;
        return id;
    }

    int clear_frame_buffers(uint32_t id){
        std::cout << "ID - " << id << " Cleaning up.\n";
        std::cout << "idata = " <<  (uint8_t*)frames[id].idata << std::endl;
        std::cout << "nbytes = " << frames[id].nbytes << std::endl;
        if (frames[id].idata != nullptr){
            //std::cout << "ID = "<< id<< " Clearing Buffers\n";
            free(frames[id].idata);
            frames[id].idata = nullptr;
            frames[id].nbytes = 0;
            frames[id].ready = false;
            frames[id].read = true;
            std::cout << "Cleanup Success\n";
            return 1;
        }
        // else{
        //     //std::cout << "ID = "<< id<< " idata is unallocated\n";
        //     free(frames[id].idata);
        //     frames[id].idata = nullptr;
        //     frames[id].nbytes = 0;
        //     frames[id].ready = false;
        // }
        std::cout << "Cleanup Skipped\n";
        return 0;
    }
    
    int pull_frame(unsigned char **img_buf, uint32_t id, uint64_t *max_size){
        std::cout << "Streammux Pulling frame...\n";
        if (!src_handles[id]){
            std::cout << "no handle\n"; 
        }
        else{
            std::cout << "Handle is ok\n";
        }
        uint32_t ret = pull_image(src_handles[id], JPEG, img_buf, max_size);
        return 1;
    }
};


#endif