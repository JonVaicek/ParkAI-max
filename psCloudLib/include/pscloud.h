#ifndef PSCLOUD_H
#define PSCLOUD_H

#include "lot.h"
#include <string>
#include <json.hpp>
#include <vector>



class Cloud {
    public:
        std::string gatedataURL;
        
        Cloud() = default;
    
        Cloud(const std::string& gatedataURL)
            : gatedataURL(gatedataURL) {}
    
        static Cloud from_json(const nlohmann::json& j) {
            return Cloud(
                j["gatedataURL"]
            );
        }
        void set_gatedataURL(const char * url){
            gatedataURL = url;
        }
};

// int get_missing_event_dates(const char *url, char *host, const char *facility, nlohmann::json resp);
int upload_function(const char *url, char *host, const char *facility, std::vector<Lot> lots);
void upload_events_threaded(int timeout,bool *run,  const char *url, char *host, const char *facility, std::vector<Lot> lots);
int send_outages_log(const char *url, char *host, const char *facility, const char *log_path, nlohmann::json& resp);
#endif