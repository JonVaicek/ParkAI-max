/**
 * @file    detector.cpp
 * @brief   Application that detects cars and its attributes from the images
 * @author  Jonas Vaicekauskas
 * @date    2025-07-03
 * @details
 */

#include "detector.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>
#include <mutex>





int run_detector(bool *run, int index, ImgReader *inStream, bool visualize){
    std::string ce_name = "car-" + std::to_string(index);
    OnnxDetector car_engine(ce_name.c_str(), VEHICLE_MODEL_PATH, 1, VEHICLE_DET_CONFIDENCE_THRESHOLD);
    std::string le_name = "lp-" + std::to_string(index);
    OnnxDetector lp_engine(le_name.c_str(), LPD_MODEL_PATH, 1, LPLATE_DET_CONFIDENCE_THRESHOLD);
    std::string ocr_name = "ocr-" + std::to_string(index);
    LPRNetDetector ocr_engine(ocr_name.c_str(), LPR_MODEL_PATH);

    Pipeline pipeline(index, WORK_DIR);
    pipeline.connectModel(car_engine, VEHICLE_MODEL);
    pipeline.connectModel(lp_engine, LP_MODEL);
    pipeline.connectModel(ocr_engine, OCR_MODEL);
    pipeline.connectInputStream(*inStream);
    std::cout << "pipeline initialized\n";

    while (*run == true){
        std::string im = pipeline.readInputStream();
        if(im != ""){
            //std::cout << "Running on image " << im << std::endl;
            pipeline.run(im, visualize);
        }
        else{
            //std::cout << "Empty Input" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 1;
}

void detections_task2(bool *run, int nthreads, bool visualize, uint64_t *perf_fps){
    std::vector<std::thread> lthreads;
    ImgReader reader(IMAGES_DIR);
    bool th_run = true;
    for (int i = 0; i < nthreads; i++ ){
        lthreads.emplace_back(run_detector, &th_run, i, &reader, visualize);
    }
    while (*run == true){
        *perf_fps = reader.get_perf_data();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    th_run = false;
    reader.stop();
    for (auto & th:lthreads){
        if (th.joinable()){
            th.join();
        }
    }
}


void detections_task(bool *run, int nthreads, bool visualize){
    std::vector<std::thread> lthreads;
    ImgReader reader(IMAGES_DIR);
    for (int i = 0; i < nthreads; i++ ){
        lthreads.emplace_back(run_detector, run, i, &reader, visualize);
    }

    for(auto & th:lthreads){
        th.join();
    }
}


// int main(int argc, char *argv[]) {
//     int nthreads = 1;
//     bool visualize = false;
//     for (int i=1; i<argc; i++){
//         if (i==1){
//             nthreads = std::stoi(argv[i]);
//         }
//         else if(i == 2){
//             int num = std::stoi(argv[i]);
//             if (num != 0 && num!=1){
//                 std::cout << " Visualize must be between 0-1\n";
//             }
//             visualize = (bool)num;
//         }
//     }
//     std::cout << "Thread count: " << nthreads << std::endl;
//     std::cout << "Visualize is " << visualize << std::endl;
    
//     std::vector<std::thread> lthreads;
    
//     ImgReader reader(IMAGES_DIR);
//     for (int i = 0; i < nthreads; i++ ){
//         lthreads.emplace_back(run_detector, run, i, &reader, visualize);
//     }

//     for(auto & th:lthreads){
//         th.join();
//     }

//     return 0;
// }
