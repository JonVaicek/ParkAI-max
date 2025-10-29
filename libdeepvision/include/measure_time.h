#ifndef MEASURE_TIME_H
#define MEASURE_TIME_H

#include <chrono>


class StopWatch{
    private:
    using clock = std::chrono::high_resolution_clock;
    std::chrono::time_point<clock> start_time;
    std::chrono::time_point<clock> end_time;
    bool running = false;

    public:
    StopWatch(void){
        start();
    }
    void start(void){
        start_time = clock::now();
        running = true;
    }
    double stop(void){
        if (running){
            end_time = clock::now();
            running = false;
            return std::chrono::duration<double, std::milli>(end_time - start_time).count();
        }
        else{
            std::cout << "StopWatch must be started first\n";
            return 0.0;
        }
    }
};

#endif