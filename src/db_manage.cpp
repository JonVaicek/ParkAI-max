#include "db_manage.h"
#include "sqlite3.h"
#include "CamerasUI.h"
#include <iostream>
#include <vector>



// Function to open the database
sqlite3* open_database(const char* db_path) {
    sqlite3* db;
    if (sqlite3_open(db_path, &db)) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        return nullptr;
    }
    return db;
}

int create_cameras_table(const char* db_path){
    sqlite3* db = open_database(db_path);
    if (!db){
        return 0;
    }
    const char* create_cameras_table_sql = R"(
        CREATE TABLE IF NOT EXISTS cameras (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            cindex INTEGER NOT NULL,
            ip TEXT NOT NULL,
            rtsp_url TEXT,
            type INTEGER NOT NULL,
            serialno TEXT
        );
    )";
    char* err_msg = nullptr;

    if (sqlite3_exec(db, create_cameras_table_sql, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "Error creating cameras table: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    } else {
        std::cout << "Cameras table created successfully." << std::endl;
    }
    sqlite3_close(db);
    return 1;
}

// Function to insert a camera into the database
void insert_camera_db(const char* db_path, Camera_t cam) {
    sqlite3* db = open_database(db_path);
    if (!db){
        return;
    }
    const char* insert_camera_sql = R"(
        INSERT INTO cameras (ip, rtsp_url, type, cindex) VALUES (?, ?, ?, ?);
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, insert_camera_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, cam.ipaddr, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, cam.rtsp, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 3, (int)cam.type);
        sqlite3_bind_int(stmt, 4, cam.index);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            std::cout << "Camera inserted successfully: IP = " << cam.ipaddr << ", RTSP URL = " << cam.rtsp << std::endl;
        } else {
            std::cerr << "Error inserting camera: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Error preparing insert statement: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_close(db);
}

int delete_camera_db(const char* db_path, int cam_id){
    const char * prompt = R"(DELETE FROM cameras WHERE id = ?;)";
    sqlite3* db = open_database(db_path);
    if (!db){
        return 0;
    }
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, prompt, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, cam_id);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            std::cout << "Camera deleted successfully: ID = " << cam_id << std::endl;
        } else {
            std::cerr << "Error deleting camera: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Error preparing delete statement: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_close(db);
    return 1;
}

int delete_all_cams(const char * db_path){
    char* err = nullptr;
    sqlite3* db = open_database(db_path);
    if (!db){
        return 0;
    }
    const char* prompt_sql = R"(
        DELETE FROM cameras;
    )";
    if (sqlite3_exec(db, prompt_sql, nullptr, nullptr, &err) != SQLITE_OK){
        std::cerr << "Error deleting all cameras: " << err << std::endl;
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }
    int rc = sqlite3_exec(db, "DELETE FROM sqlite_sequence WHERE name='cameras';", nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "Warning resetting sequence: " << err << std::endl;
        sqlite3_free(err);
        // continue
    }
    int removed = sqlite3_changes(db);
    std::cout << "Deleted " << removed << " cameras." << std::endl;
    sqlite3_close(db);
    return removed;
}

// Function to get all cameras from the database
int get_all_cameras_db(const char* db_path, std::vector<Camera_t> &dst) {
    std::vector <Camera_t> cams;
    sqlite3* db = open_database(db_path);
    if (!db){
        return 0;
    }

    const char* select_cameras_sql = R"(
        SELECT id, ip, rtsp_url, type, cindex FROM cameras;
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, select_cameras_sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char* ip = sqlite3_column_text(stmt, 1);
            const unsigned char* rtsp_url = sqlite3_column_text(stmt, 2);
            int camt = sqlite3_column_int(stmt, 3);
            int index = sqlite3_column_int(stmt, 4);
            Camera_t cam;
            strcpy(cam.ipaddr, (const char*)ip);
            strcpy(cam.rtsp, (const char*)rtsp_url);
            cam.id = id;
            cam.type = get_by_val(camt);
            cam.index = index;
            cams.push_back(cam);
        }
        sqlite3_finalize(stmt);
    } else {
        std::cerr << "Error preparing select statement: " << sqlite3_errmsg(db) << std::endl;
    }
    dst = cams;
    sqlite3_close(db);
    return 1;
}