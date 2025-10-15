#ifndef DBMANAGE_H
#define DBMANAGE_H

#include "sqlite3.h"
#include "CamerasUI.h"

sqlite3* open_database(const char* db_path);
int create_cameras_table(const char* db_path);
void insert_camera_db(const char* db_path, Camera_t cam);
int delete_camera_db(const char* db_path, int cam_id);
int get_all_cameras_db(const char* db_path, std::vector<Camera_t> &dst);
int delete_all_cams(const char * db_path);
#endif