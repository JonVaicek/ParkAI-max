#ifndef GTKAPP_H
#define GTKAPP_H

#include <string>
#include <vector>
#include "pscloud.h"
#include "lot.h"

#define CONSOLE_ENABLED     true
#define START_AI_ENGINE     false

#define SETTINGS_JSON_PATH  "settings.json"
#define SERVER_TYPE         "ParkAI"
#define WINDOW_TITLE        "ParkAI Server"

#define TARGET_IMGUI_FPS 30

#define HEARTBEAT_PERIOD_SEC 60 //1 minute
#define DATA_UPLOAD_CHECK_TIMEOUT 60 * 10 //10 minutes
#define UPLOAD_OUTAGES_PERIOD_SEC 60 * 60 * 2 // every two hours
#define RETRY_UPLOAD_OUTAGES_AFTER_SEC 60 // 1 minute to wait to retry uploading outages


#define TICK_INTERVAL_MS   1
#define MS_IN_DAY          (24*60*60*1000)
#define MS_IN_HOUR         (60*60*1000)
#define MS_IN_MINUTE       (60*1000)

class AppSettings{
    public:
        Cloud cloud_settings;
        std::vector<Server> servers;
        std::string facility_name; 
        std::vector<Lot> gate_server_lots;
        std::string boottime; // UTC boot time format "%m/%d/%Y %H:%M:%S"
        std::string sys_boot; // UTC system boot time "%m/%d/%Y %H:%M:%S"
        

        AppSettings() = default;
};


#endif