#ifndef MEASURE_TIME_H
#define MEASURE_TIME_H

#include <chrono>


// class StopWatch{
//     private:
//     std::chrono::high_resolution_clock::time_point strt;
//     bool running = false;

//     public:
//     void start(void){
//         strt = std::chrono::high_resolution_clock::now();
//         running = true;
//     }
    
//     double stop(void){
//         double ms = 0.0;
//         if (!running){
//             return ms;
//         }
//         auto end = std::chrono::high_resolution_clock::now();
//         std::chrono::duration<double, std::milli> diff = end - strt;
//         strt = end;
//         ms = diff.count();
//         return ms;
//     }
// };

#endif