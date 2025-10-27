/** Main.cpp
 *
 * @author Jonas Vaicekauskas
 *
 * See License.md for licensing.
 */

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"
#include "rlImGuiColors.h"
#include "app_settings.h"
#include "requests.h"
#include "hostname.h"
#include "logs.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include "json.hpp"
#include "detector.h"
#include "CamerasUI.h"
#include "db_manage.h"
#include "camstream.h"

char url_buf[100];
char facility_name_buf[100];
bool enable_url_edit;
const unsigned long maxlen = 128;
char hostname[maxlen];
AppSettings app_settings; //global var for app_settings

bool run_heartbeat = true;

std::thread hb_thread;
std::thread get_gates_thread;
std::vector<Lot> gateserver_lots;


int save_app_settings(const char * path, AppSettings settings){
    std::ofstream outFile(path);
    if (!outFile.is_open()) {
        std::cerr << "Could not open the file!" << std::endl;
        return -1;
    }
    nlohmann::json settingstofile;
    // Populate the JSON object with data
    settingstofile["gatedataURL"] = app_settings.cloud_settings.gatedataURL;
    settingstofile["Servers"] = nlohmann::json::array();
    for (const auto& server : app_settings.servers) {
        settingstofile["Servers"].push_back(server.to_json());
    }
    settingstofile["facility_name"] = app_settings.facility_name;
    

    // Write the JSON object to the file
    outFile << settingstofile.dump(4); // Pretty-print with 4 spaces of indentation
    outFile.close();

    std::cout << "Settings saved to: " << path << std::endl;
    return 1; // Return success
}



AppSettings load_app_settings(const char * settings_json_path){
    AppSettings settings;
    std::ifstream sourcesFile(settings_json_path);
    if (!sourcesFile.is_open()) {
        std::cerr << "Could not open the file!" << std::endl;
        return settings;
    }
    nlohmann::json jsonSources;
    sourcesFile >> jsonSources;
    sourcesFile.close();
    std::cout << jsonSources.dump(4) << std::endl;
    settings.cloud_settings = Cloud::from_json(jsonSources);
    if (jsonSources.contains("Servers") && jsonSources["Servers"].is_array()){ 
        for (const auto& server : jsonSources["Servers"]) {
            Server serv = Server::from_json(server);
            settings.servers.push_back(serv);
            std::cout << server.dump(4) << std::endl;
        }
    }
    if(jsonSources.contains("facility_name") && jsonSources["facility_name"].is_string()){
        settings.facility_name = jsonSources["facility_name"];
    }
    if(jsonSources.contains("gatedataURL")&& jsonSources["gatedataURL"].is_string()){
        settings.cloud_settings.gatedataURL = jsonSources["gatedataURL"];
    }
    printf("url: %s\n", settings.cloud_settings.gatedataURL.c_str());
    printf("facility: %s\n", settings.facility_name.c_str());
    return settings;
}

int init_application(void){

    if(!get_host_name(hostname, maxlen)){
        std::cout << "Error Reading HOSTNAME\n Not Sending Heartbeat\n";
        return -1;
    }
    app_settings = load_app_settings(SETTINGS_JSON_PATH); // load contents from sources.json
    // Clear URL Buffer
    memset(url_buf, 0, sizeof(url_buf));
    memset(facility_name_buf, 0, sizeof(facility_name_buf));
    strcpy(url_buf, app_settings.cloud_settings.gatedataURL.c_str());
    strcpy(facility_name_buf, app_settings.facility_name.c_str());
    gateserver_lots = read_lots_settings(GATE_SERVER_SETTINGS_PATH);
    if(gateserver_lots.empty()){
            for (auto &lot:gateserver_lots){
                for(auto &gate:lot.gates){
                    for (const auto & server:app_settings.servers){
                        if (server.sourceId == gate.sourceid){
                            gate.attach_ip_addr((char *)server.IpAddress.c_str());
                            std::cout << gate.ipaddr << std::endl;
                        }
                    }
                }
        }
    }
    app_settings.gate_server_lots = gateserver_lots;
    app_settings.boottime = get_datetime_utc();
    app_settings.sys_boot = get_sys_boottime_s(0, nullptr);
    get_restart();
    // disable user editing url
    enable_url_edit = false;
    return 1;
}


int url_text_input_box(bool enabled){
    
    if (!enabled){
        ImGui::BeginDisabled();
    }
    ImGui::SetNextItemWidth(250.0f); // Adjust the width as needed
    if (ImGui::InputText("Cloud URL", url_buf, IM_ARRAYSIZE(url_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        // Handle the Enter key press
        enable_url_edit = false;
        printf("Entered facility: %s\n", facility_name_buf);
        printf("URL Entered: %s\n", url_buf);
        app_settings.cloud_settings.set_gatedataURL(url_buf);
        app_settings.facility_name = facility_name_buf;
        if(!save_app_settings(SETTINGS_JSON_PATH, app_settings)){
            printf("Failed to save settings");
            return -1;
        }
        // Optionally save the text or perform other actions here
    }
    if (!enabled){
        ImGui::EndDisabled();
    }
    return 1;
}

int facility_text_input_box(bool enabled){
    
    if (!enabled){
        ImGui::BeginDisabled();
    }
    ImGui::SetNextItemWidth(250.0f); // Adjust the width as needed
    if (ImGui::InputText("Facility Name", facility_name_buf, IM_ARRAYSIZE(facility_name_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        // Handle the Enter key press
        enable_url_edit = false;
        printf("Entered facility: %s\n", facility_name_buf);
        printf("URL Entered: %s\n", url_buf);
        app_settings.cloud_settings.set_gatedataURL(url_buf);
        app_settings.facility_name = facility_name_buf;
        if(!save_app_settings(SETTINGS_JSON_PATH, app_settings)){
            printf("Failed to save settings");
            return -1;
        }
        // Optionally save the text or perform other actions here
    }
    if (!enabled){
        ImGui::EndDisabled();
    }
    return 1;
}

int edit_settings_button(void){
    if (enable_url_edit == true){
        if(ImGui::Button("Save Settings")){
            enable_url_edit = false;
            printf("URL: %s\n", url_buf);
            printf("FACILITY: %s\n", facility_name_buf);
            app_settings.cloud_settings.set_gatedataURL(url_buf);
            app_settings.facility_name = facility_name_buf;
            if(!save_app_settings(SETTINGS_JSON_PATH, app_settings)){
                printf("Failed to save settings");
                return -1;
            }
        }
    }
    else{
        if(ImGui::Button("Change Settings")){
            enable_url_edit = true;
        }
    }
    return 1;
}

void display_app_uptime(uint64_t uptime_ms){
    const uint32_t timestr_len = 128;
    char timestr[timestr_len];
    int days, hours, minutes;
    days = uptime_ms / MS_IN_DAY;
    if (days == 0){
        hours = uptime_ms / MS_IN_HOUR;
        if (hours == 0){
            minutes = uptime_ms / MS_IN_MINUTE;
            snprintf(timestr, timestr_len, "Uptime: %d min", minutes);
        }
        else{
            minutes = (uptime_ms % MS_IN_HOUR) / MS_IN_MINUTE;
            snprintf(timestr, timestr_len, "Uptime: %d h, %d min", hours, minutes);
        }
    }
    else{
        hours = (uptime_ms % MS_IN_DAY) / MS_IN_HOUR;
        snprintf(timestr, timestr_len, "Uptime: %d d, %d h", days, hours);
    }
    ImGui::Text("%s",timestr);
}

void display_fps_data(bool is_running, float fps_data){
    const uint32_t strlen = 128;
    char str[strlen];
    if (is_running)
        snprintf(str, strlen, "Detections running at: %.1f fps", fps_data);
    else
        snprintf(str, strlen, "Detections not running");

    ImGui::Text("%s", str);
}

void get_events_button(void){
    if(ImGui::Button("Get Events")){
        upload_function(app_settings.cloud_settings.gatedataURL.c_str(),
                                 hostname, app_settings.facility_name.c_str(), app_settings.gate_server_lots);
        //get_lots_data(GATE_SERVER_SETTINGS_PATH, app_settings.servers);
        }
}



void generate_tick(bool *run, int interval_ms, uint64_t *tick){
    auto next_tick_time = std::chrono::steady_clock::now();
    while (*run) {
        (*tick) ++;
        next_tick_time += std::chrono::milliseconds(interval_ms);
        std::this_thread::sleep_until(next_tick_time);
    } 
}

void periodic_upload_outages(bool *run){
    static int tick = UPLOAD_OUTAGES_PERIOD_SEC * 10; //send on boot
    static int repeat_in = UPLOAD_OUTAGES_PERIOD_SEC * 10;
    while(*run){
        /* Send Sys restarts*/
        nlohmann::json resp;
        bool ret = false;
        if(tick >= repeat_in){
            std::cout << "Uploading system outages \n";
            tick = 0;
            ret = send_outages_log(app_settings.cloud_settings.gatedataURL.c_str(), hostname, 
                        app_settings.facility_name.c_str(),SYS_BOOT_LOG_PATH, resp);
            if(!ret){
                std::cout << "Failed to upload server outages\n";
                std::cout << "Retrying in "<< RETRY_UPLOAD_OUTAGES_AFTER_SEC << "s...\n";
                repeat_in = RETRY_UPLOAD_OUTAGES_AFTER_SEC * 10;
            }
            else{
                std::cout << "Outages Uploaded Succesfully!";
                repeat_in = UPLOAD_OUTAGES_PERIOD_SEC * 10;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        tick++;
    }
}

void send_periodic_hb(AppSettings *app_settings, Detector *detector, char *host, int timeout_s, bool *run){
    static int tick = timeout_s * 10;
    std::string route;
    route = "/heartbeat";
    std::string hb_url;
    json perf_data;
    hb_url = app_settings->cloud_settings.gatedataURL + route;
    while (*run){
        if (tick >= timeout_s * 10){
            //send_heartbeat(hb_url.c_str(), host, app_settings->facility_name.c_str(), SERVER_TYPE, app_settings->sys_boot.c_str());
            server_data_t s_data = {host, app_settings->facility_name.c_str(), SERVER_TYPE, app_settings->sys_boot.c_str()};
            perf_data["running"] = detector->is_running();
            perf_data["fps"] = detector->get_fps();
            send_heartbeat_ai(hb_url.c_str(), s_data, perf_data);
            tick = 0;
        }
        tick ++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


void send_heartbeat_debug(AppSettings *app_settings, char *host, Detector *detector){
    server_data_t s_data = {host, app_settings->facility_name.c_str(), SERVER_TYPE, app_settings->sys_boot.c_str()};
    nlohmann::json perf_data;
    std::string hb_url;
    std::string route = "/heartbeat";
    hb_url = app_settings->cloud_settings.gatedataURL + route;
    perf_data["running"] = detector->is_running();
    perf_data["fps"] = detector->get_fps();
    send_heartbeat_ai(hb_url.c_str(), s_data, perf_data);
}


#if !CONSOLE_ENABLED
#ifdef _WIN32
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif
#endif
int main(int argc, char* argv[]) {

    uint64_t uptime_ms = 0;
    int show = 0;
    float fps_perf = 0.0;
    if (!init_application()){
        return 0;
    }
    if(!create_cameras_table("cams.db")){
        return 0;
    }


    uint64_t app_tick = 0;
    uint64_t *tick_ptr = &app_tick;
    std::vector<Camera_t> camList;
    std::vector<Pod> pods;
    get_all_cameras_db("cams.db", camList);
    bool run = true;
    bool run_heartbeat = true;

    std::thread tick_generator = std::thread(generate_tick, &run, TICK_INTERVAL_MS, tick_ptr);
    std::thread runtime_ts_thread = std::thread(runtime_ts, &run);


    // std::thread th_heartbeat = std::thread(periodic_heartbeat, app_settings.cloud_settings.gatedataURL.c_str(),
    //                                     hostname, app_settings.facility_name.c_str(), 
    //                                     SERVER_TYPE, HEARTBEAT_PERIOD_SEC, &run_heartbeat, app_settings.sys_boot.c_str());


    std::thread uploadData = std::thread(upload_events_threaded, DATA_UPLOAD_CHECK_TIMEOUT, &run,
                                                app_settings.cloud_settings.gatedataURL.c_str(),
                                                hostname, app_settings.facility_name.c_str(), 
                                                app_settings.gate_server_lots);

    std::thread th_upload_outages= std::thread(periodic_upload_outages, &run);

    //std::thread detections = std::thread(detections_task, &run, 4, 0);
    //ImageDetector detector(4, 0);
    // if(!START_AI_ENGINE){
    //    detector.stop();
    // }
    std::vector <stream_info> streams;
    for (const auto & cam:camList){
        std::string ip = cam.ipaddr;
        if(ip.find('X') != std::string::npos){
            std::cout << "Skipping " << cam.ipaddr <<std::endl;
            continue;
        }
        else{
            stream_info str = {cam.rtsp, cam.index};
            streams.push_back(str);
        }
    }
    uchar *img = nullptr;
    uint64_t im_size = 0;


    Detector det(4, VISUALIZE_DETECTIONS, streams);
    det.start();

    std::thread th_heartbeat = std::thread(send_periodic_hb, &app_settings, &det,
                                        hostname, HEARTBEAT_PERIOD_SEC, &run_heartbeat);
    // Initialization
    int screenWidth = 450;
    int screenHeight = 128;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, WINDOW_TITLE);
    SetTraceLogLevel(LOG_ERROR);
    std::cout << "Loading image from memory\n";
    Image rim = LoadImageFromMemory(".jpg", img, 1920*1080*3);

    Texture2D tex = LoadTextureFromImage(rim);
    std::cout << "Unloading the image memory\n";
    UnloadImage(rim);  // You can free CPU memory now
    SetTargetFPS(TARGET_IMGUI_FPS);
    int monitor = GetCurrentMonitor();
    int iDisplayWidth = GetMonitorWidth(monitor);
    int iDisplayHeight = GetMonitorHeight(monitor);
    std::cout << "Display size: " << iDisplayWidth << "x" << iDisplayHeight << std::endl;
    screenWidth = iDisplayWidth >> 2;
    screenHeight = iDisplayHeight >> 2;
    SetWindowSize(screenWidth, screenHeight);
    int nframe = 0;

    rlImGuiSetup(true);

    // Main game loop
    bool running = true;
    bool showDemoWindow = false;
    
    while (running && !WindowShouldClose()) {
        BeginDrawing();
        // start ImGui Content
        rlImGuiBegin();

        // Our menu bar
        if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Quit"))
					running = false;

				ImGui::EndMenu();
            }

			if (ImGui::BeginMenu("Window")) {
                // if (ImGui::MenuItem("Demo Window", nullptr, showDemoWindow))
				// 	showDemoWindow = !showDemoWindow;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Debug Tools")){
                if (ImGui::MenuItem("Send Heartbeat")){
                    send_heartbeat_debug(&app_settings, hostname, &det);
                }
                ImGui::EndMenu();
            }
			ImGui::EndMainMenuBar();
		}



        // This is the main display area, that fills the app's window
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        if (ImGui::Begin("Main Window", nullptr, 
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            // The toolbar
            const float minScale = 0.1f;
            const float maxScale = 4.0f;
            float toolbarWidth = std::max(450.0f, ImGui::GetContentRegionAvail().x);
            ImGui::BeginChild("Toolbar", ImVec2(toolbarWidth, 0.0f));
            ImGui::NewLine();
            //Input box for facility input
            facility_text_input_box(enable_url_edit);
            ImGui::NewLine();
            // Input box for url input
            url_text_input_box(enable_url_edit);
            //ImGui::SameLine();
            edit_settings_button();
            display_app_uptime(app_tick);
            display_fps_data(det.is_running(), fps_perf );
            button_add_camera(camList);
            ImGui::SameLine();
            imgui_AddCamerasFromFacilityPopup(pods);
            if (nframe == 0){
                // load camera list here
                get_all_cameras_db("cams.db",camList);
            }
            ImGui::Text("Cameras in list: %ld", camList.size());
            imgui_cams_table(camList);
            
            rlImGuiImageSize(&tex, tex.width /4, tex.height/4);

            
            // if (tex.id > 0) {
            //     UnloadTexture(tex);
            // }
            //ImGui::Image((ImTextureID)(intptr_t)tex.id, ImVec2((float)tex.width, (float)tex.height));
            //std::cout << "Camera list len: " << camList.size() << std::endl;
            //ImGui::NewLine();
            //get_events_button();
            
            ImGui::EndChild();
            //ImGui::SameLine();
        }
        ImGui::End();
        
        if(showDemoWindow) {        
            ImGui::ShowDemoWindow(&showDemoWindow);
        }

        // End ImGui Content
        rlImGuiEnd();

        // ##### INSERT ANY ADDITIONAL RENDERING YOU WANT TO DO OVER THE GUI HERE #####
        nframe ++;
        if(nframe >= TARGET_IMGUI_FPS){ // update this every 1s
            fps_perf = det.get_fps();
            nframe = 0;
        }
        EndDrawing();
    }
    rlImGuiShutdown();

    // Cleanup
    rlImGuiShutdown();
    //UnloadTexture(image);
    if (tex.id > 0) {
        UnloadTexture(tex);
    }
    CloseWindow();

    /* Close running threads*/
    run = false;
    run_heartbeat = false;

    if(uploadData.joinable()){
        uploadData.join();
        std::cout << "Upload Events Thread Closed\n";
    }

    if (th_heartbeat.joinable()){
        th_heartbeat.join();
        std::cout << "HeartBeat Thread Closed\n";
    }

    if(tick_generator.joinable()){
        tick_generator.join();
        std::cout << "Tick Generator Thread Closed\n";
    }

    if(runtime_ts_thread.joinable()){
        runtime_ts_thread.join();
        std::cout << "Runtime TS Thread Closed\n";
    }
    
    if (th_upload_outages.joinable()){
        th_upload_outages.join();
        std::cout << "Upload Outages Thread Closed\n";
    }
    det.stop();

    // if(detections.joinable()){
    //     detections.join();
    //     std::cout << "Detections thread Closed\n";
    // }
    //free(img);

    return EXIT_SUCCESS;
} 