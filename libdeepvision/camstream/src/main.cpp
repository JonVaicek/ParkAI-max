#include "camstream.h"
#include <iostream>
#include <vector>

#define DEFAULT_STREAMS 1
#define DEFAULT_IMG_W 1920
#define DEFAULT_IMG_H 1080


int main(int argc, char* argv[]){
    std::cout << "Starting CamStream Application\n";
    int nstreams = 1;
    if (argc > 1 ){
        nstreams = std::atoi(argv[1]);
        if (nstreams < 1){
            std::cout << "Streams must be > 0\nSetting to 1\n";
        }
    }
    std::cout << "Attempting to create " << nstreams << " streams\n";


    

    // vstream vid = load_stream(TEST_RTSP_URL, img, &run);
    uint32_t max_size = DEFAULT_IMG_H*DEFAULT_IMG_W*3;
    unsigned char *image_buf = (unsigned char *)malloc(max_size);
    unsigned char *image = (unsigned char *)malloc(max_size);
    StreamCtrl ctrl;
    bool run = true;
    std::cout << "img ptr at creation " << (void*)image_buf <<std::endl;

    vstream stream = load_manual_stream(TEST_RTSP_URL, &ctrl);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    auto start = std::chrono::high_resolution_clock::now();
    pull_image(&ctrl, PNG, image_buf, max_size);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "load_image took " << elapsed.count() << " seconds\n";
    save_jpeg_to_file_new(image_buf, "pulledimg1.png");
    free(image_buf);
    std::cout << "Saving Pulled Image\n";

    std::this_thread::sleep_for(std::chrono::seconds(5));
    image_buf = (unsigned char *)malloc(DEFAULT_IMG_H*DEFAULT_IMG_W);
    start = std::chrono::high_resolution_clock::now();
    pull_image(&ctrl, JPEG, image_buf, max_size);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "load_image took " << elapsed.count() << " seconds\n";
    save_jpeg_to_file_new(image_buf, "pulledimg2.jpeg");
    std::cout << "Saving Pulled Image\n";
    free(image_buf);

    std::this_thread::sleep_for(std::chrono::seconds(15));
    image_buf = (unsigned char *)malloc(DEFAULT_IMG_H*DEFAULT_IMG_W);
    start = std::chrono::high_resolution_clock::now();
    pull_image(&ctrl, PNG, image_buf, max_size);
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "load_image took " << elapsed.count() << " seconds\n";
    save_jpeg_to_file_new(image_buf, "pulledimg3.png");
    std::cout << "Saving Pulled Image\n";
    free(image_buf);

    run = false;
    ctrl.run = false;
    stream.join();
    free(image);
    


    //
    return 0;
}