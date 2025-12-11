#include "logs.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <ctime>
#include <limits>

#ifdef _WIN32
#include "windows.h"
#elif __linux__
#include <sys/sysinfo.h>
#endif






void strptime(std::string time_str, struct tm *tim, const char *format){
    std::istringstream ss(time_str);
    ss >> std::get_time(tim, format);
}


static std::string get_datetime(void){
    time_t now;
    time(&now);
    struct tm datetime;
    datetime = *localtime(&now);
    char output[50];
    strftime(output, sizeof(output), DATETIME_STRING_FORMAT, &datetime);
    return output;
}

static std::string get_datetime_utc(void){
    time_t now = std::time(nullptr);
    std::tm* gmt = std::gmtime(&now);  // Converts to UTC
    char output[50];
    strftime(output, sizeof(output), DATETIME_STRING_FORMAT, gmt);
    return output;
}


int write_timestamp(const char *path, std::string ts){
    std::ofstream outfile(path);
    if(!outfile.is_open()){
        std::cerr << "Could not open the file!" << std::endl;
        return -1;
    }
    outfile << ts;
    outfile.close();
    std::cout << "Timestamp written to: " << path << std::endl;
    return 1; // Return success
}

/* Read Timestamp from file in mm/dd/yyyy HH:MM:SS format
   Params: filepath, time struct for output
   Return: date string read from file */
std::string read_timestamp(const char *path, tm *tim){
    std::ifstream file(path);
    std::string ts_string;
    struct tm ts_time = {};
    if(!file.is_open()){
        std::cerr << "Could not open the file!" << std::endl;
        return ts_string;
    }
    std::getline(file, ts_string);
    file.close();
    if (ts_string.length() == 0){
        std::cerr << "File is empty" << std::endl;
        return ts_string;
    }
    std::istringstream ss(ts_string);
    ss >> std::get_time(&ts_time, DATETIME_STRING_FORMAT);
    *tim = ts_time;
    return ts_string;
}

/* Cross-Compile function to get operating system boottime UTC in wanted time format
   Params: writefile - bool value to write timestamp to file or not
           filepath - pointer to string containg filepath
   Return: string value of bootime formated in DATETIME_STRING_FORMAT*/
std::string get_sys_boottime_s(bool writefile, const char * filepath){
    char output[50];
#ifdef _WIN32
    time_t current_time = std::time(nullptr);
    // Uptime in milliseconds
    uint64_t uptime_ms = GetTickCount64();
    // Subtract uptime
    time_t boot_time = current_time - (uptime_ms / 1000);
    std::tm* gmt = std::gmtime(&boot_time);
    
    size_t str_size = strftime(output, sizeof(output), DATETIME_STRING_FORMAT, gmt);
    if(str_size != 0 && writefile){
        write_timestamp(filepath, output);
    }
    std::cout << "System boot time (UTC): " << output << "\n";
    
#elif __linux__

    time_t bsec = 0;
    std::ifstream stat("/proc/stat");
    std::string key;

    while(stat >> key){
        if (key == "btime"){
            stat >> bsec;
            break;
        }
        else{
            stat.ignore(std::numeric_limits<std::streamsize>::max());
        }
    }
    std::string out;
    if(bsec > 0){
        std:tm gmtr;
        gmtime_r(&bsec, &gmtr);
        char buf[64];
        strftime(buf, sizeof(buf), DATETIME_STRING_FORMAT, &gmtr);
        out.assign(buf);
        std::cout << "Ateina\n";
    }
    else{
        timespec bts{}, tsn{};
        clock_gettime(CLOCK_BOOTTIME, &bts);
        clock_gettime(CLOCK_REALTIME, &tsn);
        time_t btime = tsn.tv_sec - bts.tv_sec;
        if (tsn.tv_nsec < bts.tv_nsec) btime -= 1;
        std::tm gmtr;
        gmtime_r(&btime, &gmtr);
        char buf[64];
        strftime(buf, sizeof(buf), DATETIME_STRING_FORMAT, &gmtr);
        out.assign(buf);
    }

    std::cout << "System boot time (UTC): "
              << out << "\n";
              exit(1);
#else
    std::cout << "Unknown Operating System\n"
#endif
    return out;
}

nlohmann::json clear_old_events(nlohmann::json events){
    nlohmann::json ret;
    for (const auto & event:events){
        time_t diff_s;
        std::string stime = event["ts"];
        tm tmtime = {};
        time_t now = std::time(nullptr);
        tm *gmt = std::gmtime(&now);  // Converts to UTC
        strptime(stime, &tmtime, DATETIME_STRING_FORMAT);
        diff_s = std::mktime(gmt) - std::mktime(&tmtime);
        if (diff_s < STORE_EVENTS_IN_SECONDS){
            std::cout << "Keeping: " << event["name"] <<" : " << diff_s << std::endl;
            ret.push_back(event);
        }
        else{
            std::cout << "Discarding: " << event["name"] <<" : " << diff_s << std::endl;
        }
    }
    return ret;
}


nlohmann::json read_logged_events(const char *log_path){
    nlohmann::json events;
    std::ifstream ilog(log_path);
    if(!ilog.is_open()){
        std::cerr << "Could not open logs file!\n";
    }

    try {
        ilog >> events;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << '\n';
    }
    ilog.close();
    return events;
}

void log_restart(const char *log_file_path, std::string ts){
    nlohmann::json events;
    nlohmann::json clean;
    nlohmann::json new_event;
    std::string content;
    events = read_logged_events(log_file_path);
    clean = clear_old_events(events);
    new_event["name"] = "Reboot";
    new_event["ts"] = ts;
    clean.push_back(new_event);
    std::ofstream olog(log_file_path);
    if (olog.is_open()){
        olog << clean.dump();
    }
    olog.close();
}


void get_restart(void){
    tm prev_sys_boot = {};
    tm sys_boot = {};
    time_t diff_s;
    std::string prev_sys_boot_s = read_timestamp(SYSTEM_BOOT_TIME_FILEPATH, &prev_sys_boot);
    std::string sys_boot_s =  get_sys_boottime_s(1, SYSTEM_BOOT_TIME_FILEPATH); //overwrite the file with new value
    // std::string sys_boot_s =  get_datetime_utc(); //overwrite the file with new value
    strptime(sys_boot_s, &sys_boot, DATETIME_STRING_FORMAT);
    diff_s = std::mktime(&sys_boot) - std::mktime(&prev_sys_boot);
    //log_restart(SYS_BOOT_LOG_PATH, sys_boot_s);
    if (diff_s > 1){
        std::cout << "Restart Detected!!!\n";
        log_restart(SYS_BOOT_LOG_PATH, sys_boot_s);
    }
}

/* Periodic Thread for runtime timestamp used for finding outage durations */
void runtime_ts(bool *run){
    static int tick = RUNTIME_TS_INTERVAL_S * 10;
    static int repeat_in = RUNTIME_TS_INTERVAL_S * 10;
    while (*run){
        if (tick > repeat_in){
            std::string ts = get_datetime_utc();
            int ret = write_timestamp(RUNTIME_TS_PATH, ts);
            if (!ret){
                std::cerr << "Failed to write Runtime Timestamp!" << std::endl;
            }
            tick = 0;
        }
        tick++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}




