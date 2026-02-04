#include "camstream.h"
#include "gst_parent.h"
#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <thread>

#define RTSP_URL1 "rtsp://admin:Mordoras512_@192.168.0.13:554/live0"
#define RTSP_URL2 "rtsp://admin:1234@192.168.0.20:554/h.264"
#define RTSP_URL3 "rtsp://admin:1234@192.168.0.21:554/h.264"

static std::atomic<bool> g_run{true};

static void on_signal(int) {
    g_run = false;
}


int main(int argc, char* argv[]){
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    std::vector<std::string> urls;
    
    urls.push_back(RTSP_URL1);
    urls.push_back(RTSP_URL2);
    urls.push_back(RTSP_URL3);
    std::vector<GstChildWorker> workers;
    workers.reserve(urls.size());
    for (int i = 0; i < urls.size(); i++){
        uint64_t val;
        workers.emplace_back(i, "./gst_worker", urls[i].c_str());
    }
    std::cout << " [main] attempting image reading\n";
    int n = 0;
    /* store image pointers, free memory on exit*/
    uint8_t *images[workers.size()] = {nullptr, nullptr, nullptr, nullptr};
    while (g_run){
        for (int i = 0; i < workers.size(); i++){
            //uint8_t *img;
            uint64_t nbytes;
            //int ret = workers[i].pull_frame(&img, &nbytes);
            int ret = workers[i].pull_frame(&images[i], &nbytes);
            if (ret){
                std::string fn = std::string("image") + std::to_string(i) + std::string(".jpeg");
                std::vector<unsigned char> jpg = encode_jpeg(images[i], workers[i].header()->w, workers[i].header()->h);
                save_jpeg_to_file(jpg, fn);
                //free(img);
                //img = nullptr;
            }
        }
        n++;
        std::cout << "********* RUN NUMBER - " << n << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    std::cout << "images size: " << sizeof(images) << std::endl;
    for(int i = 0; i < workers.size(); i++){
        if(images[i] != nullptr){ 
            free(images[i]);
        }
    }

    for (auto & worker:workers){
        worker.terminate_and_wait();
    }

    return 0;
}