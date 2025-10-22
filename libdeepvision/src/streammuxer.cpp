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