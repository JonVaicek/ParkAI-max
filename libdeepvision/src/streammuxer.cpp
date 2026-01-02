#include "streammuxer.h"
#include <fstream>
#include <string>
#include <iostream>



/* Stream Muxer Helper Functions Begin*/
size_t current_rss_kb() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            size_t kb = 0;
            std::sscanf(line.c_str(), "VmRSS: %zu kB", &kb);
            return kb;
        }
    }
    return 0;
}

void log_mem(const char* tag) {
    std::cout << tag << " VmRSS: " << current_rss_kb()/1024.0 << " MB\n";
}


/* Stream Muxer Helper Functions End*/


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
    try{
    while(run){
        tick++;
        if (tick >= 1000){
            //std::cout << "STREAMMUX: Tick Reached 1000\n";
            tick = 0;
            update_fd();
            log_mem("prog-mem:");
            //std::cout << "Streammux Running at: " << get_fps() << " fps" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(period_ms));
    }
    } catch (const std::exception &e){
        std::cerr << "periodic_tick exception: " << e.what() << '\n';
    } catch (...){
        std::cerr << "periodic_tick unknown exception\n";
    }
    return 1;
}

int check_stream_states(StreamCtrl * ctrl){
    std::lock_guard<std::mutex> lock(*(ctrl->lock));

    //Dont restart if already restarting;
    if(ctrl->restart == true){
        return 0;
    }
    if (std::time(nullptr) - ctrl->timestamp > RECONNECT_TIME_SECONDS && ctrl->state == VSTREAM_RUNNING){ // restart stream after 3 minutes of failure to receive frame
        ctrl->restart = true;
        ctrl->state = VSTREAM_RELOAD;
        std::cout << ctrl->stream_ip <<" Stream stopped playing\n";
        std::cout << ctrl->stream_ip << " Reloading stream" << std::endl;
        quit_pipeline(ctrl);
        return 0;
    }

    if (ctrl->state == VSTREAM_STARTUP || ctrl->state == VSTREAM_RELOAD){ // try restarting stream after 5 minutes of failed loading
        if (std::time(nullptr) - ctrl->rel_time > RECONNECT_TIME_SECONDS){
            ctrl->rel_time = std::time(nullptr);
            ctrl->restart = true;
            ctrl->state = VSTREAM_RELOAD;
            std::cout << ctrl->stream_ip << " Stream failed to start\n";
            std::cout << ctrl->stream_ip << " Reloading stream" <<  std::endl;
            quit_pipeline(ctrl);
            return 0;
        }
    }
    return 1;
}

int StreamMuxer::muxer_thread(void){
    try{
    uint64_t nfr = 0;
    int ret = 0;
    while (true){
        // mutex lock
        for (int i = 0; i < src_handles.size(); i++){
            if (mlock.try_lock()){
                if(frames[i].ready == false && frames[i].read == true){
                    ret = update_frame(i);
                    if(ret){
                        frames[i].ready = true; // if frame.ready == true there is allocated memory;
                        frames[i].read = false;
                        frames[i].fid = nfr;
                        nfr ++;
                    }
                }
                check_stream_states(src_handles[i]);
                mlock.unlock();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //std::cout << "STREAMMUX: Iterated through all streams\n";
    }
    } catch (const std::exception &e){
        std::cerr << "muxer_thread exception: " << e.what() << '\n';
    } catch (...){
        std::cerr << "muxer_thread unknown exception\n";
    }
    return 1;
}

int StreamMuxer::copy_frame(int id, uchar **data, uint64_t *nbytes, uint32_t *w, uint32_t *h){
    if (id >= frames.size()){
        return 0;
    }
    if (frames[id].nbytes == 0 || frames[id].idata == nullptr){
        //std::cout << id << " - Frame Buffers empty\n";
        return 0;
    }

    *nbytes = frames[id].nbytes;
    *data = frames[id].idata;
    *w = frames[id].width;
    *h = frames[id].height;

    //int ret = clear_frame_buffers(id);
    int ret = true;
    if (ret){
        frames_returned++;
        return 1;
    }
    else{
        return 0;
    }
}




uint32_t StreamMuxer::pull_frames_batch(std::vector<ImgData> &batch_data,  uint32_t batch_size){

    std::lock_guard<std::mutex> lock(mlock);
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
        //std::cout << "Not enough frames ready\n";
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
        uint32_t w, h;

        int ret = copy_frame(rid, &pdata, &size, &w, &h);
        if (ret){
            uint32_t index = get_src_index(rid);
            ImgData data = {pdata, size, w, h, rid, index};
            batch_data.push_back(data);
        }
        else{
            continue;
        }
    }

    if (batch_data.size() == batch_size)
        return 1;
    else{
        //std::cout << "Failed to load batch\n";
        return 0;
    }
}

/**
 * @param index - camera/stream index
 * @return time_t - timestamp of last frame
 */
time_t StreamMuxer::get_stream_ts(int index){
    for(const auto & src:src_handles){
        if (src->index == index){
            return src->timestamp;
        }
    }
    return 0;
};

time_t StreamMuxer::get_stream_td(int index){
    time_t tn = std::time(nullptr);
    time_t td;
    for (const auto & src:src_handles){
        if (src->index == index){
            return  tn - src->timestamp;
        }
    }
    return tn;
}