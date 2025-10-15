#include "requests.h"
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <thread>
#include <chrono>

using json = nlohmann::json;

// Callback function to handle the response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
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
    // struct tm datetime;
    // datetime = *localtime(&now);
    char output[50];
    strftime(output, sizeof(output), DATETIME_STRING_FORMAT, gmt);
    return output;
}

json SendRequest(std::string date, std::string ip, int port, int gateId){
    // Initialize CURL
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    json RetData;
    json error = {{"error", "Failed to initialize CURL"}};

    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return error;
    }
    
    std::string url = "http://" + ip + ":"+ std::to_string(port) + "/events?date=" + date + "&gateId=" + std::to_string(gateId); 
    std::cout << "Sending Request to: " << url << std::endl;

    // Set the URL for the GET request
    res = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_setopt(CURLOPT_URL) failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return error;
    }
    // Set the callback function to handle the response data
    res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_setopt(CURLOPT_WRITEFUNCTION) failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return error;
    }

    res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_setopt(CURLOPT_WRITEDATA) failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return error;
    }

    // Perform the request
    res = curl_easy_perform(curl);


    // Check for errors and return
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        json empty;
        curl_easy_cleanup(curl);
        return empty;
    } else {
        // Parse the response data as JSON
        try {
            json jsonData = json::parse(readBuffer);
            RetData = jsonData;
            // std::cout << jsonData.dump(4) << std::endl;
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }
    }
    // Clean up curl
    curl_easy_cleanup(curl);
    return RetData;
}


json SendRequest(const char * url, json data){
    // Initialize CURL
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    json RetData;
    json error = {{"error", "Failed to initialize CURL"}};

    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return error;
    }

    // std::cout << "Sending Request to: " << url << std::endl;

    std::string jsonData = data.dump();

    res = curl_easy_setopt(curl, CURLOPT_URL, url);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_setopt(CURLOPT_URL) failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return error;
    }

    // Set the POST data
    res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_setopt(CURLOPT_POSTFIELDS) failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return error;
    }

    // Set the Content-Type header to application/json
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_setopt(CURLOPT_HTTPHEADER) failed: " << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return error;
    }

    // Set the callback function to handle the response data
    res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_setopt(CURLOPT_WRITEFUNCTION) failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return error;
    }

    res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_setopt(CURLOPT_WRITEDATA) failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return error;
    }

    // Perform the request
    res = curl_easy_perform(curl);


    // Check for errors and return
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    } else {
        // Parse the response data as JSON
        try {
            json jsonData = json::parse(readBuffer);
            RetData = jsonData;
            // std::cout << jsonData.dump(4) << std::endl;
        } catch (const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }
    }
    // Clean up curl
    curl_easy_cleanup(curl);
    return RetData;
}


json send_heartbeat(const char * url, char *host, const char *facility, const char* servertype, const char * boottime){
    json request;
    json heartbeat;
    json response;
    heartbeat["servername"] = host;
    heartbeat["facility"] = facility;
    heartbeat["servertype"] = servertype;
    heartbeat["timestamp"] = get_datetime();
    heartbeat["boottime"] = boottime;
    request["heartbeat"] = heartbeat;
    response = SendRequest(url, request);
    if (response != NULL){
        std::string resp = response.dump();
        std::cout << resp << std::endl;
    }
    return response;
}

json send_heartbeat_ai(const char *dest_url, server_data_t data, json perf_data){
    json request;
    json heartbeat;
    json response;
    heartbeat["servername"] = data.host;
    heartbeat["facility"] = data.facility;
    heartbeat["servertype"] = data.servertype;
    heartbeat["timestamp"] = get_datetime();
    heartbeat["boottime"] = data.boottime;
    //request["displays"] = disp_data; // attach led displays data to request
    request["ai-perf"] = perf_data;
    request["heartbeat"] = heartbeat;
    response = SendRequest(dest_url, request);
    if (response != NULL){
        std::string resp = response.dump();
        std::cout << resp << std::endl;
    }
    return response;
}

/**
 * @brief Send request with led display data attached
 */
json send_heartbeat_leds(const char *dest_url, server_data_t data, json disp_data){
    json request;
    json heartbeat;
    json response;
    heartbeat["servername"] = data.host;
    heartbeat["facility"] = data.facility;
    heartbeat["servertype"] = data.servertype;
    heartbeat["timestamp"] = get_datetime();
    heartbeat["boottime"] = data.boottime;
    request["displays"] = disp_data; // attach led displays data to request
    request["heartbeat"] = heartbeat;
    response = SendRequest(dest_url, request);
    if (response != NULL){
        std::string resp = response.dump();
        std::cout << resp << std::endl;
    }
    return response;
}

void periodic_heartbeat(const char *url, char *host, const char *facility, 
                        const char *servertype, int timeout_s, bool *run, const char * boottime){
    static int tick = timeout_s * 10;
    std::string route;
    route = "/heartbeat";
    std::string hb_url;
    hb_url = url + route;
    while (*run){
        if (tick * 100 >= timeout_s*1000){
            send_heartbeat(hb_url.c_str(), host, facility, servertype, boottime);
            tick = 0;
        }
        tick ++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
