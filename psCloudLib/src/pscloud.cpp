#include "pscloud.h"
#include "requests.h"
#include "lot.h"
#include "logs.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>


#define API_APP_ROUTE "/api"




/* FUNCTIONS FOR PARKSOL CLOUD INTERACTIONS*/
int get_missing_event_dates(const char *url, char *host, const char *facility, nlohmann::json& resp){
    nlohmann::json request, response;
    json method;
    std::string dest = url;
    dest = dest + API_APP_ROUTE;
    method["command"] = "GET_MISSING_EVENT_DATES";
    method["params"] = json::array();
    method["params"].push_back(facility);
    request["method"] = method;

    std::cout << dest << " GET_MISSING_EVENT_DATES\n";
    response = SendRequest(dest.c_str(), request);
    
    if (response != NULL){
        //std::cout<< response.dump() << std::endl;
        resp = response["resp"];
        //std::cout << resp << std::endl;
        return 1;
    }
    else{
        return -1;
    }
}

int upload_lots_events(const char *url, char *host, const char *facility, nlohmann::json events, nlohmann::json& resp){
    nlohmann::json request, response;
    json method;
    std::string dest = url;
    dest = dest + API_APP_ROUTE;
    method["command"] = "UPLOAD_EVENTS";
    method["params"] = json::array();
    method["params"].push_back(facility);
    method["events"] = events;
    request["method"] = method;
    std::cout << dest << " UPLOAD_EVENTS\n";
    response = SendRequest(dest.c_str(), request);
    if (response != NULL){
        //std::cout<< response.dump() << std::endl;
        resp = response["resp"];
        //std::cout << resp << std::endl;
        return 1;
    }
    else{
        return -1;
    }
}

void upload_events_threaded(int timeout, bool *run, const char *url, char *host, const char *facility, std::vector<Lot> lots){
    nlohmann::json dates;
    nlohmann:: json lotsdata;
    nlohmann::json response;
    int tick = timeout * 10;
    int success;

    while (*run){
        if (tick >= timeout*10){
            dates.clear();
            lotsdata.clear();
            response.clear();
            success = 0;
            //Check if need to update:
            success = get_missing_event_dates(url, host, facility, dates);
            if (dates.is_array() && dates.size() != 0){
                for(size_t i=0; i < dates.size(); i++ ){
                    std::cout << "Date: " << dates[i] << std::endl;
                    lotsdata = get_lots_data(lots, dates[i]);
                    if (!lotsdata.empty()){
                        success = upload_lots_events(url, host, facility, lotsdata, response);
                    }
                }
            }

            tick = 0;
        }
        tick ++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int upload_function(const char *url, char *host, const char *facility, std::vector<Lot> lots){
    int success = 0;
    nlohmann::json dates;
    json lotsdata;
    nlohmann::json response;
    success = get_missing_event_dates(url, host, facility, dates);
    if (dates.is_array()){
        for(size_t i=0; i < dates.size(); i++ ){
            std::cout << "Date: " << dates[i] << std::endl;
            lotsdata = get_lots_data(lots, dates[i]);
            success = upload_lots_events(url, host, facility, lotsdata, response);
        }
    }
    return 1;
}

/**
 * @brief Send log of server outages to the parksol cloud api
 * @return returns true on success and false on failure
 */
int send_outages_log(const char *url, char *host, const char *facility, const char *log_path, nlohmann::json& resp){
    nlohmann::json request, response;
    json method;
    std::string dest = url;
    dest = dest + API_APP_ROUTE;
    method["command"] = "UPLOAD_OUTAGES";
    method["params"] = json::array();
    method["params"].push_back(facility);
    method["params"].push_back(host);
    method["events"] = read_logged_events(log_path);
    request["method"] = method;
    std::cout << dest << " UPLOAD_EVENTS\n";
    response = SendRequest(dest.c_str(), request);
    if (!response["resp"].empty()){
        resp = response["resp"];
        std::cout << "Response from Send Outages Log: " << resp << std::endl;
        return true;
    }
    else{
        return false;
    }
}