#ifndef CAMERASUI_H
#define CAMERASUI_H

#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"
#include "rlImGuiColors.h"
#include <vector>
#include <iostream>
#include "detector.h"

#define CAMERAS_LIST_DB "cams.db"

typedef enum {
 HIKVISION = 0,
 DAHUA,
 UNIVIEW,
 PARKSOL,
 UNDEFINED = -1
}CamType;

extern std::vector<std::string> CamTypeNames;

CamType get_by_val(int val);


struct Camera_t{
    char ipaddr[17];
    char rtsp[128];
    char img_pull_url[128];
    time_t ts=0;
    int index;
    int id;
    CamType type;
};



struct Pod{
    int id;
    int cid1;
    int cid2;
    char ip1[17];
    char ip2[17];
};

int button_add_camera(std::vector<Camera_t> &camList);
int imgui_cams_table(std::vector<Camera_t> clist, std::vector<stream_info> &str_data);


int load_facility_file(std::string path, std::vector<Pod> &dest);
void imgui_AddCamerasFromFacilityPopup(std::vector<Pod> &dst);
#endif