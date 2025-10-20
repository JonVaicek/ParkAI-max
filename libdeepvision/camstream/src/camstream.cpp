
/*File Includes*/
#include "camstream.h"
#include <stdio.h>
#include <jpeglib.h>
#include <png.h>
#include <chrono>
#include <iostream>
#include <fstream>

/*File Includes End*/

/* Definitions Start*/

/* Definitions End*/
#define N_CHANNELS 3

struct loop_ctl_struct{
    std::vector<unsigned char> img;
    GstElement *pipeline = NULL;
    GstElement *appsink = NULL;
    GMainLoop *loop = NULL;
    bool run = true;
    bool read = false;
};

struct pngStruct {
    unsigned char *buf;
    uint32_t size;
    uint32_t max_size;
};

typedef enum{
    ERROR = 0,
    SUCCESS = 1
}png_ret;


/* Global Variables*/
std::thread func;
std::vector<unsigned char> latest_frame;
/* Global Variables End*/



void save_jpeg_to_file(const std::vector<unsigned char>& jpeg_data, const std::string& filename) {
    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return;
    }
    outfile.write(reinterpret_cast<const char*>(jpeg_data.data()), jpeg_data.size());
    outfile.close();
}


void save_jpeg_to_file_new(const unsigned char * jpeg_data, const std::string& filename) {
    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return;
    }
    outfile.write(reinterpret_cast<const char*>(jpeg_data), std::streamsize(1920*1080*3));
    //outfile.write(reinterpret_cast<const char*>(jpeg_data.data()), jpeg_data.size());
    outfile.close();
}

/* This is optional but included to show how png_set_write_fn() is called */
static void png_flush(png_structp png_ptr){
 /* Nothing to do*/
}

static void png_write_buf(png_structp png_ptr, png_bytep data, png_size_t length){
    pngStruct *dstruct = (pngStruct *)png_get_io_ptr(png_ptr);
    if (dstruct->size + length > dstruct->max_size){
        png_error(png_ptr, "PNG buffer overflow");
        return;
    }
    memcpy(dstruct->buf + dstruct->size, data, length);
    dstruct->size += length;
}

png_ret encode_png(unsigned char *srcbuf, unsigned char *dest, int width, int height, uint32_t max_size){
    png_voidp user_error_ptr;
    png_error_ptr user_error_fn;
    png_error_ptr user_warning_fn;
    pngStruct dstruct = {dest, 0, max_size};

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)user_error_ptr, user_error_fn, user_warning_fn);
    if (! png_ptr){
        return ERROR;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr){
        png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
        return ERROR;
    }
    if(setjmp(png_jmpbuf(png_ptr))){
        png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
        return ERROR;
    }
    //png_set_write_fn(png_ptr, dest, )
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_compression_level(png_ptr, 1);
    png_set_write_fn(png_ptr, &dstruct, png_write_buf, png_flush);
    png_write_info(png_ptr, info_ptr);
    for(int i=0; i<height; i++){
        png_write_row(png_ptr, srcbuf + i*width*N_CHANNELS);
    }
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return SUCCESS;
}


size_t encode_jpeg_to_buffer(unsigned char *raw_data, int width, int height, unsigned char *output_buffer, size_t max_size) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    
    if (!raw_data || !output_buffer) {
        std::cerr << "raw_data or output_buffer is NULL in encode_jpeg_to_buffer\n";
        return 0;
    }
    if (width <= 0 || height <= 0) {
        std::cerr << "Invalid dimensions in encode_jpeg_to_buffer: " << width << "x" << height << "\n";
        return 0;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    
    // Set up the destination buffer
    unsigned char *temp_buffer = NULL;
    unsigned long temp_size = 0;
    jpeg_mem_dest(&cinfo, &temp_buffer, &temp_size);

    // Set compression parameters
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3; // RGB has 3 components
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 85, TRUE); // Set JPEG quality (0-100)
    
    // Start compression
    jpeg_start_compress(&cinfo, TRUE);
    
    // Write scanlines
    JSAMPROW row_pointer[1];
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &raw_data[cinfo.next_scanline * width * 3];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    
    // Finish compression
    jpeg_finish_compress(&cinfo);
    
    // Copy to output buffer if it fits
    size_t actual_size = temp_size;
    if (actual_size <= max_size) {
        memcpy(output_buffer, temp_buffer, actual_size);
    } else {
        std::cerr << "Output buffer too small: need " << actual_size << " bytes, have " << max_size << "\n";
        actual_size = 0;
    }
    
    // Clean up
    jpeg_destroy_compress(&cinfo);
    free(temp_buffer);
    
    return actual_size;  // Returns actual size written, or 0 on error
}



std::vector<unsigned char> encode_jpeg(unsigned char *raw_data, int width, int height) {
    std::vector<unsigned char> jpeg_data;
    
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    if (!raw_data) {
        std::cerr << "raw_data is NULL in encode_jpeg\n";
        return {};
    }
    if (width <= 0 || height <= 0) {
        std::cerr << "Invalid dimensions in encode_jpeg: " << width << "x" << height << "\n";
        return {};
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    
    // Set up the destination buffer
    unsigned char *buffer = NULL;
    unsigned long size = 0;
    jpeg_mem_dest(&cinfo, &buffer, &size);

    // Set compression parameters
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3; // RGB has 3 components
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 85, TRUE); // Set JPEG quality (0-100)
    
    // Start compression
    jpeg_start_compress(&cinfo, TRUE);
    
    // Write scanlines
    JSAMPROW row_pointer[1];
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &raw_data[cinfo.next_scanline * width * 3];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    // Finish compression
    jpeg_finish_compress(&cinfo);
    
    // Copy the compressed data to the vector
    jpeg_data.assign(buffer, buffer + size);
    
    // Clean up
    jpeg_destroy_compress(&cinfo);
    free(buffer);
    
    return jpeg_data;
}


uint32_t pull_image(StreamCtrl *ctrl, ImgFormat format, unsigned char **img_buf, uint64_t *max_size){
    GstStateChangeReturn ret;
    ctrl->frame_rd = false;
    GstState current_state, pending_state;
    if (GST_IS_ELEMENT(ctrl->pipeline) && GST_IS_ELEMENT(ctrl->appsink)){
        gst_element_set_state(ctrl->pipeline, GST_STATE_PLAYING);
    }
    else{
        return 0;
    }
    
    GstSample* sample = nullptr;

    // gst_element_get_state(ctrl->pipeline, &current_state, &pending_state, 10*GST_MSECOND);

    // if (current_state == GST_STATE_PAUSED){
    //     std::cout << "State is Paused\n";
    // }
    // else if(current_state == GST_STATE_NULL){
    //     std::cout << "State is Null\n";
    // }
    // else if (current_state == GST_STATE_READY){
    //     std::cout << "State is ready\n";
    // }
    // else if(current_state == GST_STATE_VOID_PENDING){
    //     std::cout << "State is void\n";
    // }

    int n = 0;
    while(ctrl->frame_rd != true){
        if (n>= 10){
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        n++;
    }
    g_signal_emit_by_name(ctrl->appsink, "pull-sample", &sample);
    if (sample) {
        
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_READ);

        unsigned char *raw_data = map.data;
        if (!raw_data) {
            g_printerr("raw_data is NULL\n");
            gst_buffer_unmap(buffer, &map);
            gst_sample_unref(sample);
            return 0;
        }

        GstCaps *caps = gst_sample_get_caps(sample);
        GstStructure *structure = gst_caps_get_structure(caps, 0);
        int width = 0, height = 0;
        gst_structure_get_int(structure, "width", &width);
        gst_structure_get_int(structure, "height", &height);
        //std::cout << "Image WH = " << width << "x" << height <<std::endl;
        
        *img_buf = (unsigned char*)malloc(map.size);
        *max_size = (uint64_t) map.size;
        ctrl->imgW = width;
        ctrl->imgH = height;

        switch (format){
            case JPEG:{
                uint32_t size = encode_jpeg_to_buffer(raw_data, width, height , *img_buf, *max_size);
                if(size != 0){
                    //std::cout << "JPEG Encoded succesfully\n";
                }
                break;
            }
            case PNG:{
                png_ret ret = encode_png(raw_data, *img_buf, width, height, *max_size);
                if (ret == SUCCESS){
                    //std::cout << "PNG Encoded succesfully\n";
                }
                break;
            }
            default:{
                break;
            }
        }
        
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);

        gst_element_set_state(ctrl->pipeline, GST_STATE_PAUSED);
        ctrl->frame_rd = false;
        return 1;
    }
    return 0;
}


// Callback to process new samples from appsink
GstFlowReturn frame_received_pipeline_callback(GstElement *sink, gpointer user_data) {
    loop_ctl_struct *lpctl = static_cast<loop_ctl_struct*> (user_data);
    //std::vector<unsigned char>* img = static_cast<std::vector<unsigned char>*>(user_data);
    GstSample *sample = NULL;
    int width = 640;
    int height = 480;
    std::cout << "new sample ready\n";
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (sample) {
        
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_READ);

        unsigned char *raw_data = map.data;
        if (!raw_data) {
            g_printerr("raw_data is NULL\n");
            return GST_FLOW_ERROR;
        }

        GstCaps *caps = gst_sample_get_caps(sample);
        GstStructure *structure = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(structure, "width", &width);
        gst_structure_get_int(structure, "height", &height);
        //*img = encode_jpeg(raw_data, width, height);
        lpctl->img = encode_jpeg(raw_data, width, height);
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);

        lpctl->run = false; //to kill the thread
        //save_jpeg_to_file(lpctl->img, "pic.jpeg");

        return GST_FLOW_OK;
    }
    return GST_FLOW_ERROR;
}


// Callback to process new samples from appsink
GstFlowReturn new_sample_pipeline_callback(GstElement *sink, gpointer user_data) {
    //loop_ctl_struct *lpctl = static_cast<loop_ctl_struct*> (user_data);
    unsigned char* img = static_cast<unsigned char*>(user_data);
    std::cout << "New Sample ptr: " << (void *)img << std::endl;
    GstSample *sample = NULL;
    int width = 640;
    int height = 480;
    //std::cout << "new sample ready\n";
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (sample) {
        
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_READ);

        unsigned char *raw_data = map.data;
        if (!raw_data) {
            g_printerr("raw_data is NULL\n");
            return GST_FLOW_ERROR;
        }

        GstCaps *caps = gst_sample_get_caps(sample);
        GstStructure *structure = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(structure, "width", &width);
        gst_structure_get_int(structure, "height", &height);
        size_t jpeg_size = encode_jpeg_to_buffer(raw_data, width, height, img, 1920*1080);
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);

        //lpctl->run = false; //to kill the thread
        //save_jpeg_to_file(lpctl->img, "pic.jpeg");

        return GST_FLOW_OK;
    }
    return GST_FLOW_ERROR;
}


// Callback to process new samples from appsink
GstFlowReturn sample_ready_callback(GstElement *sink, gpointer user_data) {

    StreamCtrl* ctl = static_cast<StreamCtrl*>(user_data);
    GstSample *sample = NULL;
    int width = 640;
    int height = 480;
    ctl->frame_rd = true;
    ctl->n_ftim = 0;
    if (true) {
        return GST_FLOW_OK;
    }
    return GST_FLOW_ERROR;
}

// Bus message handler
static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer user_data) {
    StreamCtrl *ctrl = static_cast<StreamCtrl*> (user_data);
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug;
            gst_message_parse_error(msg, &err, &debug);
            std::cerr << "Error from element " << GST_OBJECT_NAME(msg->src)
                      << ": " << err->message << std::endl;
            // Print detailed error information
            // std::cerr << "=== GStreamer Error ===" << std::endl;
            // std::cerr << "Element: " << GST_OBJECT_NAME(msg->src) << std::endl;
            // std::cerr << "Error: " << err->message << std::endl;
            // std::cerr << "Debug: " << (debug ? debug : "No debug info") << std::endl;
            // std::cerr << "Error Domain: " << g_quark_to_string(err->domain) << std::endl;
            // std::cerr << "Error Code: " << err->code << std::endl;
            // std::cerr << "======================" << std::endl;
            g_error_free(err);
            g_free(debug);
            break;
        }

        case GST_MESSAGE_EOS:
            std::cout << "End of Stream received — restarting pipeline" << std::endl;
            //gst_element_set_state(ctrl->pipeline, GST_STATE_NULL);
            //g_main_loop_quit(ctrl->loop);
            //return FALSE;
            break;

        default:
            break;
    }

    return TRUE;
}


static gboolean periodic_tick(gpointer user_data){
  loop_ctl_struct *lpctl = static_cast<loop_ctl_struct*> (user_data);
  //std::cout << "Pipeline Running\n";
  //std::cout << "run val = " << lpctl->run <<  std::endl;;
  if (! (lpctl->run)) {
        std::cout << "Stopping Cam Preview!\n";
        //gst_element_set_state(lpctl->pipeline, GST_STATE_NULL);
        g_main_loop_quit(lpctl->loop);
        // Returning FALSE removes this timeout
        return false;
    }
    // Returning TRUE keeps the timeout active
    return true;
}

static gboolean periodic_tick_continious(gpointer user_data){
  StreamCtrl *ctrl = static_cast<StreamCtrl*> (user_data);
  static int no_frame_cnt = 0;
  const int THRESHOLD_NOT_RECEIVED = 100;
  if (ctrl->noframe ){
    no_frame_cnt++;
    if (no_frame_cnt >= THRESHOLD_NOT_RECEIVED){
        std::cout << "RESTARTING COUSE OF NOFRAMECNT\n";
        ctrl->restart = true;
        no_frame_cnt = 0;
    }
  }
  else{
    no_frame_cnt = 0;
  }
  ctrl->n_ftim ++;
  if (ctrl->n_ftim > THRESHOLD_NOT_RECEIVED){
    std::cout << "RESTARTING COUSE OF N_FTIM\n";
    ctrl->restart = true;
  }
  // std::cout << "Running: " << *(ctrl->run) << std::endl;
  if (! ctrl->run || ctrl->restart == true) {
        //std::cout << "Cam " << ctrl->ip << "Playback is closing!\n";
        gst_element_set_state(ctrl->pipeline, GST_STATE_NULL);
        g_main_loop_quit(ctrl->loop);
        ctrl->restart = false;
        // Returning FALSE removes this timeout
        return false;
    }
    // Returning TRUE keeps the timeout active
    return true;
}


int create_pipeline_single_frame(std::string rtsp_url, loop_ctl_struct *loop_ctl){
    std::cout << "Creating Cam Preview Pipeline\n";
    gst_init(NULL, NULL);
    std::string gst_launch =
        "rtspsrc location=" + rtsp_url + " latency=0 "
        "! rtph264depay ! h264parse ! decodebin "
        "! videoconvert ! video/x-raw,format=RGB ! appsink name=sink emit-signals=true sync=false";
        //"! nvvideoconvert ! video/x-raw,format=RGB ! appsink name=sink emit-signals=true sync=false";

    loop_ctl->pipeline = gst_parse_launch(gst_launch.c_str(), NULL);
    //GstElement *pipeline = gst_parse_launch(gst_launch.c_str(), NULL);

    loop_ctl->appsink = gst_bin_get_by_name(GST_BIN(loop_ctl->pipeline), "sink");
    //GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    //GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    loop_ctl->loop = g_main_loop_new(NULL, FALSE);
    
    if (!loop_ctl->pipeline || !loop_ctl->appsink) {
        g_printerr("Failed to create pipeline\n");
        return -1;
    }
    std::cout << "Pipeline created!\n";
    g_signal_connect(loop_ctl->appsink, "new-sample", G_CALLBACK(frame_received_pipeline_callback), loop_ctl);

    gst_element_set_state(loop_ctl->pipeline, GST_STATE_PLAYING);
    g_timeout_add(100, periodic_tick, loop_ctl);
    //GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop_ctl->loop);

    gst_element_set_state(loop_ctl->pipeline, GST_STATE_NULL);
    gst_object_unref(loop_ctl->pipeline);

    g_main_loop_unref(loop_ctl->loop);
    return 0;
}

void init_camstream(void){
    gst_init(NULL, NULL);
}

/***
 * @brief Creates a gstreamer pipeline for h264 rtsp stream parse. Pipeline needs to be manually preloaded for frame pulls
 * @param rtsp_url rtsp stream url
 * @param ctrl reference to pipeline control structure
 */
void create_pipeline_multi_frame_manual(std::string rtsp_url, StreamCtrl *ctrl){
    std::cout << "Creating Cam Preview Pipeline\n";
    //gst_init(NULL, NULL);
    std::string gst_launch =
        //"rtspsrc location=" + rtsp_url + " protocols=tcp latency=2000 retry=3 "
        "rtspsrc location=" + rtsp_url + " protocols=tcp "
        "! rtph264depay ! h264parse ! decodebin "
        "! videoconvert ! video/x-raw,format=RGB ! queue leaky=2 max-size-buffers=1 ! appsink name=sink emit-signals=true sync=false";

    ctrl->pipeline = gst_parse_launch(gst_launch.c_str(), NULL);
    ctrl->appsink = gst_bin_get_by_name(GST_BIN(ctrl->pipeline), "sink");
    ctrl->loop = g_main_loop_new(NULL, FALSE);
    g_object_set(ctrl->appsink, "drop", true, "max-buffers", 1, NULL);

    if (!ctrl->pipeline || !ctrl->appsink) {
        g_printerr("Failed to create pipeline\n");
        return;
    }
    std::cout << "Pipeline created!\n";
    g_signal_connect(ctrl->appsink, "new-sample", G_CALLBACK(sample_ready_callback), ctrl);

    /* Uncomment if preroll to be done before the first pull*/
    // gst_element_set_state(ctrl->pipeline, GST_STATE_PLAYING);
    // gst_element_get_state(ctrl->pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);
    gst_element_set_state(ctrl->pipeline, GST_STATE_PAUSED);
    g_timeout_add(100, periodic_tick_continious, ctrl);

    GstBus *bus = gst_element_get_bus(ctrl->pipeline);
    guint bus_watch_id = gst_bus_add_watch(bus, bus_call, ctrl); 
    gst_object_unref(bus);

    g_main_loop_run(ctrl->loop);
    
    //cleanup
    g_source_remove(bus_watch_id);
    gst_element_set_state(ctrl->pipeline, GST_STATE_NULL);

    gst_object_unref(ctrl->pipeline);
    gst_object_unref(ctrl->appsink);
    g_main_loop_unref(ctrl->loop);
    return;
}


void create_pipeline_multi_frame(std::string rtsp_url, unsigned char *img, StreamCtrl *ctrl){
    std::cout << "Creating Cam Preview Pipeline\n";
    std::cout << "img ptr at pipeline creation: " << (void *)img << std::endl;
    gst_init(NULL, NULL);
    std::string gst_launch =
        "rtspsrc location=" + rtsp_url + " latency=0 "
        "! rtph264depay ! h264parse ! decodebin "
        "! videoconvert ! video/x-raw,format=RGB ! appsink name=sink emit-signals=true sync=false";

    ctrl->pipeline = gst_parse_launch(gst_launch.c_str(), NULL);
    ctrl->appsink = gst_bin_get_by_name(GST_BIN(ctrl->pipeline), "sink");
    ctrl->loop = g_main_loop_new(NULL, FALSE);
    if (!ctrl->pipeline || !ctrl->appsink) {
        g_printerr("Failed to create pipeline\n");
        return;
    }
    std::cout << "Pipeline created!\n";
    g_signal_connect(ctrl->appsink, "new-sample", G_CALLBACK(new_sample_pipeline_callback), img);

    gst_element_set_state(ctrl->pipeline, GST_STATE_PLAYING);
    g_timeout_add(100, periodic_tick_continious, ctrl);
    //GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(ctrl->loop);

    gst_element_set_state(ctrl->pipeline, GST_STATE_NULL);
    gst_object_unref(ctrl->pipeline);
    g_main_loop_unref(ctrl->loop);
    return;
}

int stream(const char *rtsp_url, loop_ctl_struct *lctl){
    while (lctl->run){
        if(lctl->run){
            create_pipeline_single_frame(rtsp_url, lctl);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 1;
}


int pipeline(const char *rtsp_url, unsigned char *img, bool *run, StreamCtrl *ctrl){
    ctrl->run = run;
    while(*run){
        create_pipeline_multi_frame(rtsp_url, img, ctrl);
    }
    return 1;
}
vstream load_stream(const char *rtsp_url, unsigned char *img, bool *run, StreamCtrl *ctrl){
    return vstream(pipeline, rtsp_url, img, run, ctrl);
} 

/**
 * @brief Manual Pipeline loop
 * @param rtsp_url rtsp stream url
 * @param ctrl stream control struct
 */
int pipeline_manual(const char *rtsp_url, StreamCtrl *ctrl){
    while(ctrl->run){
        create_pipeline_multi_frame_manual(rtsp_url, ctrl);
        std::cout << "Playback ended. Closing...\n";
        std::this_thread::sleep_for(std::chrono::seconds(10));
        ctrl->restart = false;
        ctrl->n_ftim = 0;
    }
    return 1;
}

/**
 * @brief Manual frame pulling gstreamer pipeline in a thread
 * @param rtsp_url rtsp stream url
 * @param ctrl stream control struct
 * @return vstream thread
 */
vstream load_manual_stream(const char *rtsp_url, StreamCtrl *ctrl){
    return vstream(pipeline_manual, rtsp_url, ctrl);
}



int load_image(const char *rtsp_url, std::vector<unsigned char> &img){
    loop_ctl_struct lctl;
    lctl.run = true;
    std::thread pipe(stream, rtsp_url, &lctl);
    //std::this_thread::sleep_for(std::chrono::seconds(10));
    //lctl.run = false;
    pipe.join();
    if (lctl.img.empty())
        return 0;
    img = lctl.img;
    return 1;
}