#ifndef LOGS_H
#define LOGS_H

#include <iostream>
#include "json.hpp"

#define DATETIME_STRING_FORMAT "%m/%d/%Y %H:%M:%S"

#define RUNTIME_TS_PATH "runtime_ts.txt"
#define SYSTEM_BOOT_TIME_FILEPATH "sys_boot.txt"
#define SYS_BOOT_LOG_PATH "boot_log.txt"
#define RUNTIME_TS_INTERVAL_S 30


#define DAYS_STORE_EVENTS        90
#define STORE_EVENTS_IN_SECONDS (DAYS_STORE_EVENTS*24*60*60)     


void runtime_ts(bool *run);
std::string get_sys_boottime_s(bool writefile, const char * filepath);
std::string read_timestamp(const char *path, tm *tim);
void strptime(std::string time_str, struct tm *tim, const char *format);
void get_restart(void);
nlohmann::json read_logged_events(const char *log_path);
#endif