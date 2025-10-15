#include "lot.h"
#include "requests.h"
#include "date/date.h"
#include "date/tz.h"
#include <fstream>
#include <ctime>
#include <thread>

std::string get_datetime(void){
    time_t now;
    time(&now);
    struct tm datetime;
    datetime = *localtime(&now);
    char output[50];
    strftime(output, sizeof(output), DATETIME_STRING_FORMAT, &datetime);
    return output;
}

std::string get_datetime_utc(void){
    time_t now = std::time(nullptr);
    std::tm* gmt = std::gmtime(&now);  // Converts to UTC
    char output[50];
    strftime(output, sizeof(output), DATETIME_STRING_FORMAT, gmt);
    return output;
}

std::string get_date(void){
    time_t now;
    time(&now);
    struct tm datetime;
    datetime = *localtime(&now);
    char output[50];
    strftime(output, sizeof(output), "%Y-%m-%d", &datetime);
    return output;
}

std::vector <Lot> ReadLotsfromJSON(json lotsData){
    std::vector <Lot> lots;
    for (const auto& lotJson : lotsData) {
        Lot lot = Lot::from_json(lotJson);
        lots.push_back(lot);
    }
    return lots;
}

json ReadGateServerSettings(std::string path){
    json data;
    // Open the file
    std::ifstream settingsFile(path);
    // Check if the file is open
    if (!settingsFile.is_open()) {
        std::cerr << "Could not open the file!" << std::endl;
        return data;
    }
    settingsFile >> data;
    // Close the file
    settingsFile.close();
    return data;
}


std::vector<Lot> read_lots_settings(const char * path){
    // Parse the JSON file
    std::vector <Lot> lots;
    json jsonGateServSettings = ReadGateServerSettings(path);
    if(jsonGateServSettings.empty()){
        return lots;
    }
    json GateServerSettings = jsonGateServSettings["PSCameraServerApp"];
    int CloudEnable = GateServerSettings["CloudEnable"];
    std::string ServerName = GateServerSettings["Description"];
    json lotsData = jsonGateServSettings["PSCameraServerApp"]["Lots"];
    lots = ReadLotsfromJSON(lotsData);

    return lots;
}

std::string getServerIp(std::vector<Server> servers, int sourceId){
    for(const auto& serv : servers){
        if (serv.sourceId == sourceId){
            return serv.IpAddress;
        }
    }
    return "";
}


// void get_lots_data(const char * gate_serv_settings_path, std::vector<Server> servers){
//     std::vector<Lot> lots = read_lots_settings(gate_serv_settings_path);
//     for (auto& lot : lots){
//         if (lot.cloudenabled == -1){
//             for (auto& gate : lot.gates){
//                 std::string ipaddr = getServerIp(servers, gate.sourceid);
//                 json gateData;
//                 //std::string date = get_date();
//                 std::string date = "2025-04-01";

//                 gateData = SendRequest(date, ipaddr, 8080, gate.gateid);
//                 if (gateData != NULL){
//                     gate.addTimestamp(gateData);
//                     std::string st_gatedata = gateData.dump(4);
//                     std::cout << "GateData: "<< st_gatedata << std::endl;
//                 }
//             }
//         }
//     }
// }

json get_lots_data(std::vector<Lot> lots, std::string date){
    json ret;
    ret["date"] = date;
    ret["lots"] = json::array();
    
    for (const auto& lot : lots){
        if (lot.cloudenabled == -1){
            json jlot;
            jlot["name"] = lot.name;
            jlot["id"] = lot.id;
            jlot["in"] = json::array();
            jlot["out"] = json::array();
            for (auto& gate : lot.gates){
                std::string ipaddr = gate.ipaddr;
                json gateData;
                //std::string date = get_date();
                gateData = SendRequest(date, ipaddr, PARKCTAPI_PORT, gate.gateid);

                if (!gateData.empty() && gateData.is_array()){
                    if (gate.add == 1) {
                        jlot["in"].insert(jlot["in"].end(), gateData.begin(), gateData.end());

                    } else if (gate.add == 0) {
                        jlot["out"].insert(jlot["out"].end(), gateData.begin(), gateData.end());
                    }
                }
            }
            if (jlot["in"].size() != 0 && jlot["out"] != 0){
                ret["lots"].push_back(jlot);
            }
        }
    }
    if(ret["lots"].size() == 0){
        ret.clear();
    }
    return ret;
}

void parkctapi_get_lot_events_async(std::string date, Lot lot){
    json events;
    std::thread th;
    for (const auto & gate:lot.gates){
        events = SendRequest(date, gate.ipaddr, PARKCTAPI_PORT, gate.gateid);
    }
    std::cout << events.dump(4) << std::endl;
}