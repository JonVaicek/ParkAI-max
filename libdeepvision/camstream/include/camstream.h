#ifndef CAMSTREAM_H
#define CAMSTREAM_H
#include <iostream>
#include <vector>
#include <thread>
#include <gst/gst.h>
#include <glib.h>
#include <mutex>
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
    PNG,
    RAW
}ImgFormat;

typedef enum{
    VSTREAM_NULL = 0,
    VSTREAM_STARTUP,
    VSTREAM_RUNNING,
    VSTREAM_RELOAD
}StreamState;

typedef enum{
    STREAM_SET_PLAYING = 0,
    STREAM_SET_PAUSED,
    STREAM_SET_NULL

}StreamSetState;

struct StreamPipeline {
    GstElement *pipeline;
    GstElement *rtspsrc;
    GstElement *depay;
    GstElement *parser;
    GstElement *queue1;
    GstElement *valve;
    GstElement *decodebin;
    GstElement *videoconvert;
    GstElement *capsfilter;
    GstElement *queue2;
    GstElement *appsink;
};

struct StreamCtrl{
    std::string stream_ip;
    GMainContext *context = NULL;
    GstElement *pipeline = NULL;
    GstElement *appsink = NULL;
    GstElement *valve = NULL;
    GMainLoop *loop = NULL;
    std::mutex *lock = nullptr;
    time_t timestamp = 0;
    time_t rel_time = 0;
    uint64_t im_size = 0;
    uint32_t imgW = 0;
    uint32_t imgH = 0;
    uint32_t index;
    StreamSetState setState = STREAM_SET_NULL;
    guint bus_watch_id;
    guint sampleh_id;
    guint probe_id;
    StreamState state = VSTREAM_NULL;
    unsigned char *image = nullptr;
    bool allocated = false;
    bool run = true;
    bool paused = false;
    bool noframe = false;
    bool restart = false;
    bool frame_rd = false;
};



void init_camstream(void);
void save_jpeg_to_file(const std::vector<unsigned char>& jpeg_data, const std::string& filename);
void save_jpeg_to_file_new(const unsigned char * jpeg_data, const std::string& filename);


vstream load_manual_stream(int id, const char *rtsp_url, StreamCtrl *ctrl);
uint32_t pull_image(StreamCtrl *ctrl, ImgFormat format, unsigned char **img_buf, uint64_t *max_size);
uint32_t pull_gst_frame(StreamCtrl *ctl, unsigned char **img_buf, uint64_t *max_size);

int quit_pipeline(StreamCtrl *ctrl);



#endif