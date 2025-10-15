#include "CamerasUI.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "db_manage.h"

std::vector <std::string> CamTypeNames = {"Hikvision", "Dahua", "Uniview", "Parksol"};

int load_facility_file(std::string path, std::vector<Pod> &dest){
    const int N_HEADER_ROWS = 12;
    std::ifstream file(path); // or .csv if tab-delimited
    if (!file.is_open()) {
        std::cerr << "Error: could not open file\n";
        return 0;
    }

    std::string line;
    //skip header:
    for (int i = 0; i<N_HEADER_ROWS; i++){
        std::getline(file, line);
    }
    std::vector<std::string> ips;
    std::vector<Pod> pods;
    //read cameras
    int cid = 0;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;
        
        std::string level, pid, ip1, s1, s2, s3, ip2, s4, s5, s6;
        
        int col = 0;
        while (std::getline(ss, cell, '\t')) {  // tab-delimited
            switch(col){
                case 0:
                level = cell;
                break;
                case 1:
                pid = cell;
                break;
                case 2:
                ip1 = cell;
                break;
                case 3:
                s1 = cell;
                break;
                case 4:
                s2 = cell;
                break;
                case 5:
                s3 = cell;
                break;
                case 6:
                ip2 = cell;
                break;
                case 7:
                s4 = cell;
                break;
                case 8:
                s5 = cell;
                break;
                case 9:
                s6 = cell;
                break;
            }
            col++;
            if (col == 10){
                col = 0;
            }
        }
        Pod pod;
        memset(pod.ip1, 0, sizeof(pod.ip1));
        memset(pod.ip2, 0, sizeof(pod.ip2));
        pod.id = std::atoi(pid.c_str());
        if (ip1.size()){
            ips.push_back(ip1);
            strcpy(pod.ip1, ip1.c_str());
            pod.cid1 = cid;
            cid++;
        }
        if (ip2.size()){
            ips.push_back(ip2);
            strcpy(pod.ip2, ip2.c_str());
            pod.cid2 = cid;
            cid++;
        }
        pods.push_back(pod);
    }
    
    for (const auto & ip:ips){
        std::cout << ip << std::endl;
    }
    file.close();
    dest = pods;
    return 1;
}


CamType get_by_val(int val){
    switch(val){
        case 0:{
            return HIKVISION;
            break;
        }
        case 1:{
            return DAHUA;
            break;
        }
        case 2:{
            return UNIVIEW;
            break;
        }
        case 3:{
            return PARKSOL;
            break;
        }
        default:{
            return UNDEFINED;
            break;
        }
    }
}


int button_add_camera(std::vector<Camera_t> &camList){
    static int selected = 0;
    static Camera_t cam;
    if(ImGui::Button("Add Camera")){
        selected = 0;
        memset(cam.ipaddr, 0, sizeof(cam.ipaddr));
        memset(cam.rtsp, 0, sizeof(cam.rtsp));
        ImGui::OpenPopup("Add Camera");
    }
    if (ImGui::BeginPopupModal("Add Camera", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter Camera Details:");
        ImGui::InputText("Camera IP", cam.ipaddr, IM_ARRAYSIZE(cam.ipaddr), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::InputText("RTSP url", cam.rtsp, IM_ARRAYSIZE(cam.rtsp), ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::RadioButton("Hikvision", selected == HIKVISION)){
            selected = HIKVISION;
            cam.type = HIKVISION;
        }
        if (ImGui::RadioButton("Dahua", selected == DAHUA)){
            selected = DAHUA;
            cam.type = DAHUA;
        }
        if (ImGui::RadioButton("Uniview", selected == UNIVIEW)){
            selected = UNIVIEW;
            cam.type = UNIVIEW;
        }
        if (ImGui::RadioButton("Parksol", selected == PARKSOL)){
            selected = PARKSOL;
            cam.type = PARKSOL;
        }
        ImGui::Text("You picked: %d", cam.type);
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Save")) {
            std::cout << "Saving Camera "<< std::endl;
            std::cout << "IP: " << cam.ipaddr << "\n rtsp: " << cam.rtsp  << "\n type: " << cam.type << std::endl;
            open_database("cams.db");
            insert_camera_db("cams.db", cam);
            get_all_cameras_db("cams.db", camList);
            
            ImGui::CloseCurrentPopup();
        }
    ImGui::EndPopup();
    }
    return 1;
}



int imgui_cams_table(std::vector<Camera_t> clist){
    int rows = clist.size();
    int cols = 4;
    const char * tb_name = "Cameras";

    if(ImGui::BeginTable(tb_name, cols, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)){
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("IP");
        ImGui::TableSetupColumn("TYPE");
        ImGui::TableSetupColumn("ACTION");
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for (int column = 0; column < cols; column++)
        {
                ImGui::TableSetColumnIndex(column);
                const char* column_name = ImGui::TableGetColumnName(column); // Retrieve name passed to TableSetupColumn()
                ImGui::PushID(column);
                //ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                //ImGui::PopStyleVar();
                //ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::TableHeader(column_name);
                ImGui::PopID();
        }
        for (int row = 0; row < rows; row ++ ){
            ImGui::TableNextRow();
            for(int col = 0; col < cols; col++){
                ImGui::TableSetColumnIndex(col);
                
                switch(col){
                    case 0:{
                        ImGui::Text("%d", clist[row].index);
                        break;
                    }
                    case 1:{
                        ImGui::Text("%s", clist[row].ipaddr);
                        break;
                    }
                    case 2:{
                        ImGui::Text("%s", CamTypeNames[clist[row].type].c_str());
                        break;
                    }
                    case 3:{
                        ImGui::PushID(row); // makes IDs unique per row
                        if(ImGui::Button("View")){
                            ImGui::OpenPopup("Camera View");
                        }
                        if (ImGui::BeginPopupModal("Camera View", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                            
                            if (ImGui::Button("Close")) {
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                        
                        
                        if(ImGui::Button("Delete")){
                            std::cout << "Deleting camera " << clist[row].id << std::endl;
                            ImGui::OpenPopup("Really?");
                            //int ret = delete_camera_db("cams.db", clist[row].id );
                        }
                        if (ImGui::BeginPopupModal("Really?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                        ImGui::Text("Are you sure u want to delete this camera-%d?", clist[row].id);
                        if (ImGui::Button("Cancel")) {
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::Button("Yes")) {
                            int ret = delete_camera_db("cams.db", clist[row].id );
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }
                        ImGui::PopID();
                        break;
                    }
                }
                
            }
        }
        ImGui::EndTable();
    }


    return 1;
}

void imgui_popup_enter_pods_interval(std::vector<Pod> pods, std::vector<Pod> &loaded){
    if(ImGui::BeginPopupModal("Enter Interval", NULL, ImGuiWindowFlags_AlwaysAutoResize)){
        static int startIndex = 0;
        static int endIndex = pods.size()-1;
        ImGui::InputInt("Pod Start Index", &startIndex, 1, 10, 0);
        ImGui::InputInt("Pod End Index", &endIndex, 1, 10, 0);
        if(ImGui::Button("Load")){
            if (startIndex <= endIndex && startIndex>=0 && endIndex<pods.size()){
                delete_all_cams(CAMERAS_LIST_DB);
                std::cout << "Loading Pods from " << startIndex << " to " << endIndex << std::endl;
                loaded.assign(pods.begin()+startIndex, pods.begin()+endIndex+1);
                for (const auto & pd: loaded){
                    std::cout << "pod " << pd.ip1 << ", " << pd.ip2 << std::endl;
                    Camera_t cam;
                    strcpy(cam.ipaddr, pd.ip1);
                    cam.type = PARKSOL;
                    sprintf(cam.rtsp, "rtsp://admin:1234@%s:554/h.264", cam.ipaddr);
                    cam.index = pd.cid1;
                    insert_camera_db(CAMERAS_LIST_DB, cam);
                    if(pd.ip2[0]){
                        Camera_t cam;
                        strcpy(cam.ipaddr, pd.ip2);
                        cam.type = PARKSOL;
                        sprintf(cam.rtsp, "rtsp://admin:1234@%s:554/h.264", cam.ipaddr);
                        cam.index = pd.cid2;
                        insert_camera_db(CAMERAS_LIST_DB, cam);
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            else if(startIndex > endIndex){
                    std::cout << "StartIndex must be less than EndIndex\n";
                    
            }
            else if(endIndex >= pods.size()){
                std::cout << "End Index is > pods.size\n";
            }
        }
        if(ImGui::Button("Cancel")){
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void imgui_AddCamerasFromFacilityPopup(std::vector<Pod> &dst){
    static char path_buf[254];
    static std::vector<Pod> pods;
    static int ret = 0;
    
    //memset(path_buf, 0, sizeof(path_buf));
    if(ImGui::Button("Add Camera From Facility File")){
        ImGui::OpenPopup("Facility File Path");
        dst.clear();
    }
    if (ImGui::BeginPopupModal("Facility File Path", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("It Will Delete All Existing Cameras\n");
        if(ImGui::InputText("Facility File Path", path_buf, IM_ARRAYSIZE(path_buf), ImGuiInputTextFlags_EnterReturnsTrue)){
            ret = load_facility_file(path_buf, pods);
            if (ret){
                std::cout << "Pods Loaded: " << pods.size() << std::endl;
            }
            //ImGui::CloseCurrentPopup();
        }

        if (ret){
            ImGui::Text("Pods on file: %ld\n", pods.size());
        }
        else{
            ImGui::Text("Facility file not loaded\n");
        }
        if (ImGui::Button("Load All")) {
            //ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(ImGui::Button("Load Some")){
            ImGui::OpenPopup("Enter Interval");
        }
        imgui_popup_enter_pods_interval(pods, dst);
        if (dst.size()){
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}