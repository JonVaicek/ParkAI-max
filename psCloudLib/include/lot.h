#ifndef LOT_HPP
#define LOT_HPP

#include <iostream>
#include <string>
#include <vector>
#include <json.hpp>


using json = nlohmann::json;

#define GATE_SERVER_SETTINGS_PATH "C:/ProgramData/ParksolUSA/Parksol Gate Server/settings.cams"
#define PARKCTAPI_PORT 8080

class Gate {
private:
    static const unsigned long ch_buflen = 18;
    std::vector <std::string> timestamps;
public:
    int add;
    int gateid;
    int gatetype;
    int sourceid;
    char ipaddr[ch_buflen];
    
    Gate(int add, int gateid, int gatetype, int sourceid)
        : add(add), gateid(gateid), gatetype(gatetype), sourceid(sourceid) {
            memset(ipaddr, 0, ch_buflen);
        }
    
    void addTimestamp(const nlohmann::json& eventlist) {
        if ( eventlist.is_array()){
            for (const auto& event : eventlist) {
                std::string timestamp = event["eventTS"];
                timestamps.push_back(timestamp);
            }
        }
    }
    const std::vector<std::string>& getTimestamps() const {
        return timestamps;
    }

    void attach_ip_addr(char * ip_addr){
        strcpy(ipaddr, ip_addr);
    }
};

class Lot {
public:
    int id;
    std::string name;
    int total;
    int cloudenabled;
    std::vector<Gate> gates;

    Lot(int id, const std::string& name, int total, int cloudenabled)
        : id(id), name(name), total(total), cloudenabled(cloudenabled) {}

    void addGate(const Gate& gate) {
        gates.push_back(gate);
    }

    static Lot from_json(const nlohmann::json& j) {
        Lot lot(j["id"], j["name"], j["total"], j["cloudenabled"]);
        if (j.contains("Gates") && j["Gates"].is_array()) {
            for (const auto& gate : j["Gates"]) {
                lot.addGate(Gate(gate["add"], gate["gateid"], gate["gatetype"], gate["sourceid"]));
            }
        }
        return lot;
    }
};


class Server {
public:
    int sourceId;
    std::string IpAddress;

    Server(int SourceId, const std::string& IpAddress)
        : sourceId(SourceId), IpAddress(IpAddress) {}

    static Server from_json(const nlohmann::json& j) {
        return Server(
            j["SourceId"],
            j["IpAddress"]
        );
    }
    json to_json(void)const{
        json j;
        j["SourceId"] = sourceId;
        j["IpAddress"] = IpAddress;
        return j;
    }
};

std::vector <Lot> ReadLotsfromJSON(json lotsData);
std::vector<Lot> read_lots_settings(const char * path);
std::string get_datetime(void);
std::string get_datetime_utc(void);
std::string get_date(void);
// void get_lots_data(const char * gate_serv_settings_path, std::vector<Server> servers);
json get_lots_data(std::vector<Lot> lots, std::string date);
json ReadGateServerSettings(std::string path);

#endif // LOT_HPP