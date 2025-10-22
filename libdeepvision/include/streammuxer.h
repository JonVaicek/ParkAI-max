#ifndef STREAMMUXER_H
#define STREAMMUXER_H

#include "camstream.h"
#include "detector.h"


#define STREAMMUX_MS 1
#define FRAME_NOT_RECEIVED_THRESHOLD_MS 10000
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
    std::thread tick_thread;
    uint64_t frames_returned = 0;
    uint64_t fd[10];

    bool run=true;

    int update_fd(void){
        static int bi = 0;
        fd[bi] = frames_returned;
        bi++;
        if (bi>=sizeof(fd)/sizeof(fd[0])){
            bi=0;
        }
        return bi;
    }

    float get_fps(void){
        float fps = 0.0;
        int fpsa = 0;
        int len = sizeof(fd)/sizeof(fd[0]);
        uint64_t max  = 0;

        int im;

        for(int i = 0; i < len; i++){
            if(fd[i] > max){
                max = fd[i];
                im = i;
            }
            //std::cout << "i - " << i << ", val = " << fd[i] << std::endl;
        }
        if (max == 0){
            return 0;
        }
        
        int is = len - (im+1);
        uint64_t val = 0;
        int n = 0;
        int start = im;
        for (int i = 0; i < len; i++){

            im = start - i;
            if (im < 0){
                im = len + (start-i);
            }
            is = im - 1;
            if(is < 0){
                is = len - 1;
            }
            if (fd[im] < fd[is])
                continue;
            val = fd[im] - fd[is];
            if(val >= 0){
                fpsa += val;
                n++;
            }
        }
        fps = fpsa/(float)n;
        if (fps < 0.0){
            std::cout << "PRINTING FPS FRAMES BUF\n";
            for (int i = 0; i < len; i++){
                std::cout << "i-" << i << " = " << fd[i] << std::endl;
            }
        }
        return fps;
    }

    int periodic_tick(uint32_t period_ms){
        static uint32_t tick = 0;
        while(run){
            tick++;
            if (tick == 1000){
                tick = 0;
                update_fd();
                std::cout << "Streammux Running at: " << get_fps() << " fps" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
        }
        return 1;
    }

    int muxer_thread(void){
        uint64_t n=0;
        uint64_t nfr = 0;
        int ret = 0;
        while (true){
            for (int i = 0; i < src_handles.size(); i++){
                if(frames[i].ready == false && frames[i].read == true){
                    ret = update_frame(i);
                    if(ret){
                        //src_handles[i]->noframe = false;
                        frames[i].nfailed = 0;
                        //std::cout << i <<" - New Frame\n";
                        frames[i].ready = true;
                        frames[i].read = false;
                        frames[i].age = 0;
                        nfr ++ ;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            n++;
            if (n == 1000){
                n=0;
                //std::cout << "Muxer running. Frames read: "<< nfr <<std::endl;
            }
        }
    }
    public:
    StreamMuxer(void){
        memset(fd, 0, sizeof(fd));
        mux_thread = std::thread([this](){muxer_thread();});
        tick_thread =std::thread([this](){periodic_tick(STREAMMUX_MS);});
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
        uint32_t ret = pull_image(src_handles[id], PNG, &img, &nbytes);
        if (!ret){
            return 0;
        }
        // if (frames[id].idata != nullptr){

        //     free(img);
        //     nbytes = 0;
        //     return 0;
        // }
        //std::cout << "ID - " << id << " Frame Pulled Successfully\n";
        frames[id].nbytes = nbytes;
        frames[id].idata = (uchar *)malloc(nbytes);
        memcpy(frames[id].idata, img, nbytes);
        free(img);
        nbytes = 0;
        //std::cout << "ID - " << id << " Frame Copied Successfully\n";
        return 1;
    }

    uint32_t pull_valid_frame(uchar **data, uint64_t *nbytes){
        uint32_t id=0xFFFFFFFF;
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
            return 0xFFFFFFFF;
        }
        //logic to return frames
        *data = frames[id].idata;
        *nbytes = frames[id].nbytes;
        frames_returned++;
        //ret_i = src_handles[id]->index;
        //frames[id].read = true;
        return id;
    }

    int clear_frame_buffers(uint32_t id){
        //std::cout << "ID - " << id << " Cleaning up.\n";
        //std::cout << "idata = " <<  (uint8_t*)frames[id].idata << std::endl;
        //std::cout << "nbytes = " << frames[id].nbytes << std::endl;
        if (frames[id].idata != nullptr){
            //std::cout << "ID = "<< id<< " Clearing Buffers\n";
            free(frames[id].idata);
            frames[id].idata = nullptr;
            frames[id].nbytes = 0;
            frames[id].ready = false;
            frames[id].read = true;
            //std::cout << "Cleanup Success\n";
            return 1;
        }
        // else{
        //     //std::cout << "ID = "<< id << " idata is unallocated\n";
        //     free(frames[id].idata);
        //     frames[id].idata = nullptr;
        //     frames[id].nbytes = 0;
        //     frames[id].ready = false;
        // }
        //std::cout << "Cleanup Skipped\n";
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

    uint32_t get_src_index(uint32_t src_id){
        if (src_id < src_handles.size()){
            return src_handles[src_id]->index;
        }
        else{
            return 0xFFFFFFFF;
        }
    }
};


#endif