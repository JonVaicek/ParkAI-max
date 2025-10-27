#ifndef STREAMMUXER_H
#define STREAMMUXER_H

#include "camstream.h"
#include <iostream>
#include <opencv2/opencv.hpp>

typedef unsigned char uchar;

#define STREAMMUX_MS 1
#define FRAME_NOT_RECEIVED_THRESHOLD_MS 10000

#define STREAMMUX_RET_ERROR 0xFFFFFFFF


struct ImgData{
    //cv::Mat data;
    uchar *data;
    uint64_t nbytes = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t id = STREAMMUX_RET_ERROR;
    uint32_t index = STREAMMUX_RET_ERROR;
};


struct FrameInfo {
    uchar *idata = nullptr;
    uint64_t nbytes = 0;
    uint64_t fid = (uint64_t)-1;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t age = 0;
    uint32_t nfailed = 0;
    bool ready = false;
    bool read=true;
};

class StreamMuxer{

    const static int MAX_STREAMS = 128;
    std::vector<vstream *> sources;
    std::vector<StreamCtrl *> src_handles;
    std::vector<FrameInfo> frames;
    std::thread mux_thread;
    std::thread tick_thread;
    uint64_t frames_returned = 0;
    uint64_t fd[10];
    bool run=true;

    /* Private Methods */
    int update_fd(void);
    int periodic_tick(uint32_t period_ms);
    int muxer_thread(void);

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
        //uint32_t ret = pull_image(src_handles[id], PNG, &img, &nbytes);
        uint32_t ret = pull_image(src_handles[id], RAW, &img, &nbytes);
        if (!ret){
            return 0;
        }

        frames[id].nbytes = nbytes;
        frames[id].idata = (uchar *)malloc(nbytes);
        memcpy(frames[id].idata, img, nbytes);
        frames[id].width = src_handles[id]->imgW;
        frames[id].height = src_handles[id]->imgH;
        free(img);
        nbytes = 0;
        //std::cout << "ID - " << id << " Frame Copied Successfully\n";
        return 1;
    }

    uint32_t pull_valid_frame(uchar **data, uint64_t *nbytes){
        uint32_t id=STREAMMUX_RET_ERROR;
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
            return STREAMMUX_RET_ERROR;
        }
        //logic to return frames
        *data = frames[id].idata;
        *nbytes = frames[id].nbytes;
        frames_returned++;
        //ret_i = src_handles[id]->index;
        //frames[id].read = true;
        return id;
    }


    uint32_t pull_frames_batch(std::vector<ImgData> &batch_data, uint32_t batch_size);


    //int copy_frame(int id, cv::Mat &img, uint64_t *size);
    int copy_frame(int id, uchar **data, uint64_t *nbytes, uint32_t *w, uint32_t *h);

    int clear_frame_buffers(uint32_t id){
        //std::cout << "ID - " << id << " Cleaning up.\n";
        //std::cout << "idata = " <<  (uint8_t*)frames[id].idata << std::endl;
        //std::cout << "nbytes = " << frames[id].nbytes << std::endl;
        if (frames[id].idata != nullptr){
            std::cout << "Clearing buffers - " << id << std::endl;
            //std::cout << "ID = "<< id<< " Clearing Buffers\n";
            free(frames[id].idata);
            frames[id].idata = nullptr;
            frames[id].nbytes = 0;
            frames[id].fid = (uint64_t)-1;
            frames[id].ready = false;
            frames[id].read = true;
            std::cout << "Cleanup Success\n";
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
    
    float get_fps(void);
};


#endif