#include "streammuxer.h"


float StreamMuxer::get_fps(void){
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

int StreamMuxer::update_fd(void){
    static int bi = 0;
    fd[bi] = frames_returned;
    bi++;
    if (bi>=sizeof(fd)/sizeof(fd[0])){
        bi=0;
    }
    return bi;
}

int StreamMuxer::periodic_tick(uint32_t period_ms){
    static uint32_t tick = 0;
    while(run){
        tick++;
        if (tick == 1000){
            tick = 0;
            update_fd();
            //std::cout << "Streammux Running at: " << get_fps() << " fps" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }
    return 1;
}

int StreamMuxer::muxer_thread(void){
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
                    frames[i].fid = nfr;
                    nfr ++ ;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        n++;
        if (n == 1000){
            n=0;
        }
    }
}

int StreamMuxer::copy_frame(int id, uchar **data, uint64_t *nbytes){
    if (id >= frames.size()){
        return 0;
    }
    if (frames[id].nbytes == 0 || frames[id].idata == nullptr){
        //std::cout << id << " - Frame Buffers empty\n";
        return 0;
    }

    *nbytes = frames[id].nbytes;
    *data = frames[id].idata;

    //int ret = clear_frame_buffers(id);
    int ret = true;
    if (ret){
        //frames_returned++;
        return 1;
    }
    else{
        return 0;
    }
}




uint32_t StreamMuxer::pull_frames_batch(std::vector<ImgData> &batch_data,  uint32_t batch_size){
    
    /* Find frames with smallest nfr */
    uint32_t nready = 0;
    std::vector<uint32_t> ids;
    //std::cout << "Available ids:\n[";
    for (int i = 0; i < frames.size(); i++){
        if (frames[i].ready == true && frames[i].read == false){
            nready++;
            ids.push_back(i);
            //std::cout << i << ", ";
        }
    }
    //std::cout << "]\n";

    if (nready < batch_size){
        return 0; //Not enough frames are ready 
    }

    /* pull oldest frames */
    for (int b = 0; b < batch_size; b++){
        uint64_t nfr_min = (uint64_t)-1;
        uint32_t i_erase = (uint32_t)-1;
        uint32_t rid = (uint32_t)-1;
        //std::cout << "ids_size = " << i_erase << std::endl;
        for (int i = 0; i < ids.size(); i++){
            if(frames[ids[i]].fid <= nfr_min){
                nfr_min = frames[ids[i]].fid;
                i_erase = i;
            }
        }
        if(i_erase < ids.size()){
            //std::cout << "Erasing " << ids[i_erase] << std::endl;
            rid = ids[i_erase];
            ids.erase(ids.begin() + i_erase);
            //std::cout << "After erase ids: \n[";
            // for (const auto & id:ids){
            //     std::cout << id << ", ";
            // }
            // std::cout << "]\n";
        }
        else{
            std::cout << "Failed to find oldest frames\n";
            return 0;
        }
        /* @todo: copy the frame to return vector */
        uint64_t size;
        uchar *pdata = nullptr;

        int ret = copy_frame(rid, &pdata, &size);
        if (ret){
            uint32_t index = get_src_index(i_erase);
            ImgData data = {pdata, size, i_erase, index};
            batch_data.push_back(data);
        }
        else{
            continue;
        }
    }

    if (batch_data.size() == batch_size)
        return 1;
    else
        return 0;
}