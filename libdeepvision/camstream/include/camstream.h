#ifndef CAMSTREAM_H
#define CAMSTREAM_H

#include <vector>
#include <thread>
#include <gst/gst.h>
#include <glib.h>
#define TEST_RTSP_URL "rtsp://admin:Mordoras512_@192.168.0.13:554/live0"
#define TEST_PARKSOL_RTSP_URL "rtsp://admin:1234@192.168.0.20:554/h.264"


typedef std::thread vstream;


typedef enum{
    MANUAL = 0,
    AUTO,
    SINGLE   
}PipelineType;

typedef enum{
    JPEG = 0,
    PNG
}ImgFormat;

struct StreamCtrl{
    GstElement *pipeline = NULL;
    GstElement *appsink = NULL;
    GMainLoop *loop = NULL;
    uint64_t n_ftim = 0; //fail timer. to time how long pipeline did not produce new frames
    uint32_t imgW = 0;
    uint32_t imgH = 0;
    uint32_t index;
    bool run = true;
    bool noframe = false;
    bool restart = false;
    bool frame_rd = false;
};


void init_camstream(void);
void save_jpeg_to_file(const std::vector<unsigned char>& jpeg_data, const std::string& filename);
void save_jpeg_to_file_new(const unsigned char * jpeg_data, const std::string& filename);
int load_image(const char *rtsp_url, std::vector<unsigned char> &img);
vstream load_stream(const char *rtsp_url, unsigned char *img, bool *run, StreamCtrl *ctrl);

vstream load_manual_stream(const char *rtsp_url, StreamCtrl *ctrl);
uint32_t pull_image(StreamCtrl *ctrl, ImgFormat format, unsigned char **img_buf, uint64_t *max_size);


#endif