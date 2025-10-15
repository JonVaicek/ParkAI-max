#ifndef REQUESTS_H
#define REQUESTS_H


#include "json.hpp"

#define PARCKTAPI_PORT 8080
#define DATETIME_STRING_FORMAT "%m/%d/%Y %H:%M:%S"

using json = nlohmann::json;

struct server_data_t {
    char *host;
    const char *facility;
    const char *servertype;
    const char *boottime;
};

json SendRequest(std::string date, std::string ip, int port, int gateId);
json SendRequest(const char * url, json data);
json send_heartbeat(const char * url, char *host, const char *facility, const char* servertype, const char * boottime);

json send_heartbeat_leds(const char *dest_url, server_data_t data, json disp_data);
json send_heartbeat_ai(const char *dest_url, server_data_t data, json perf_data);

void periodic_heartbeat(const char *url, char *host, const char *facility, 
    const char *servertype, int timeout_s, bool *run, const char * boottime);




#endif