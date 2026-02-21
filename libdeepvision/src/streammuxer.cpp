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

int print_sources_table(std::vector <GstChildWorker *> &vec){
    std::cout << "---|-------------------------------------------------------|-----|-----|-----|-----\n";
    printf(" id| rtsp_url ---------------------------------------------| evfd|  sv1|  sv2|  shm\n");
    for (const auto & ch:vec){
        printf("%3d|%-55s|%5d|%5d|%5d|%5d\n", ch->get_id(), ch->rtsp_url_, ch->get_evfd(), ch->sv_[0], ch->sv_[1], ch->shmfd_);
    }
    std::cout << "---|-------------------------------------------------------|-----|-----|-----|-----\n";
    return 1;
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



uint32_t delete_from_epoll(int epfd, GstChildWorker *worker){
    if(epoll_ctl(epfd, EPOLL_CTL_DEL, worker->get_evfd(), nullptr) == 0){
        printf("[%s] fd-%d deleted from epoll\n", worker->rtsp_url_, worker->get_evfd());
        worker->set_epoll_flag(false);
        return 1;
    }
    else{
        printf("[%s] fd-%d could not delete from epoll\n", worker->rtsp_url_, worker->get_evfd());
        return 0;
    }
}


int StreamMuxer::state_machine(void){
    
    while(true){
        if(mlock.try_lock()){
            if (!sources.empty()){
                for (auto & s:sources){
                    switch(s->state){
                        case ALIVE:
                            s->is_infected();
                            break;

                        case INFECTED:
                            s->killit();
                            break;

                        case ZOMBIE:
                            if(s->reap()){
                                delete_from_epoll(epfd, s);
                            }
                            break;

                        case PURGED:
                            /* close fds */
                            s->close_sockfd();
                            s->close_evfd();
                            s->release_mem();
                            s->close_shmfd();
                            s->bury();
                            break;

                        case BURIED:
                            s->reinit();
                            break;
                    }
                }
            }
            mlock.unlock();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 1;
}


int StreamMuxer::child_epoller(void){

    uint64_t nfr=0;
    uint64_t n_restarted = 0;
    std::cout << "EPPOLLING INIT DONE\n";
    
    while (true){
        
    if(!sources.empty()){
        if(mlock.try_lock()){
            printf("Program has restarted streams - %lu times\n", n_restarted);
            bool epoll_del = false;
            //print_sources_table(sources);
            epoll_event events[MAX_EVENTS];
            int n;

            do {
                n = epoll_wait(epfd, events, MAX_EVENTS, 1000);
            } while (n < 0 && errno == EINTR);

            if(n < 0){
                std::cout << "epoll error\n";
                continue;
            }
            
            for (int e = 0; e < n; e++) {
                //size_t i = events[e].data.u32;
                auto* src = static_cast<GstChildWorker*>(events[e].data.ptr);
                std::cout << "Reading event from fd = " << src->get_evfd() << std::endl;
                printf("[%s] - evfd [%d] received new event\n", src->rtsp_url_, src->get_evfd());
                if(src->get_evfd() <= 0){
                    std::cout << "invalid evfd\n";
                    continue;
                }
                uint64_t sig;   
                ssize_t s = read(src->get_evfd(), &sig, sizeof(sig));
                if (s == -1) {
                    if (errno == EAGAIN) continue;
                    perror("read evfd failed");
                    continue;
                }
                if (s != sizeof(sig)) continue;

                /* clear the epoll events with deleted evfds */
                if(!src->is_registered()){
                    printf("[src-%s] received events after deletion\n", src->rtsp_url_);
                    continue; //
                }
                auto evt = signal_parser(sig);
                if(evt == EVT_PIPELINE_EXIT){
                    printf("[%s] adding to infected\n", src->rtsp_url_);
                    std::cout << "[" << src->rtsp_url_ << "] exited\n";
                    continue;
                }

                src->handle_event(evt); // this handles the shm_init event and deinit_
                if ((sig & EVT_FRAME_WAITING) == EVT_FRAME_WAITING) {
                    src->set_frame_waiting(true);
                }
            }
            mlock.unlock();
        }
    }
    else{
            
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 1;
}

int StreamMuxer::frame_reader(void){
    uint64_t nfr = 0;
    while(true){
        if (mlock.try_lock()){
        for (int i = 0; i < sources.size(); i++){
            if(sources[i]->is_frame_waiting() && !frames[i].ready){
                //printf("Reading Frame from [%d] in sources [%d]\n", i, sources[i]->get_id());
                if(sources[i]->read_frame(&frames[i].idata, &frames[i].nbytes)){
                    frames[i].width  = sources[i]->header()->w;
                    frames[i].height = sources[i]->header()->h;
                    frames[i].ready  = true;
                    frames[i].read   = false;
                    frames[i].fid = nfr;
                    nfr++;
                    sources[i]->set_frame_waiting(false);
                }
            }
        }
        mlock.unlock();
    }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
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
            rid = ids[i_erase];
            ids.erase(ids.begin() + i_erase);
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
        return 0;
    }
}

/**
 * @param index - camera/stream index
 * @return time_t - timestamp of last frame
 */
time_t StreamMuxer::get_stream_ts(int index){
    for(const auto & src:sources){
        if (src->get_id() == index){
            return src->get_ts();
        }
    }
    return 0;
};

time_t StreamMuxer::get_stream_td(int index){
    time_t tn = std::time(nullptr);
    time_t td;
    for (const auto & src:sources){
        if (src->get_id() == index){
            return  tn - src->get_ts();
        }
    }
    return tn;
}