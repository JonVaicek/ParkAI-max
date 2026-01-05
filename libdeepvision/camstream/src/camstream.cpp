
/*File Includes*/
#include "camstream.h"
#include <stdio.h>
#include <jpeglib.h>
#include <png.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <gst/app/gstappsink.h>  // add if not included
#include <gst/rtsp/rtsp.h>
#include <malloc.h>
/*File Includes End*/

/* Definitions Start*/

/* Definitions End*/
#define N_CHANNELS 3


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

void reset_stream_control(StreamCtrl *ctrl){
    std::cout << "Reset Stream Control\n";
    ctrl->restart = false;
    std::cout << "Reset timestamp\n";
    ctrl->timestamp = 0;
    std::cout << "Reset rel time\n";
    ctrl->rel_time = std::time(nullptr);
    std::cout << "Reset state\n";
    ctrl->state = VSTREAM_STARTUP;

    if(ctrl->image){
        std::cout << "free image\n";
        free(ctrl->image);
        ctrl->imgH = 0;
        ctrl->imgW = 0;
        std::cout << "reset image pointer\n";
        ctrl->image = nullptr;
    }
    std::cout << "reset frame ready\n";
    ctrl->frame_rd = false;
};

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


// idle handlers: do blocking state changes on main loop thread
static gboolean pause_pipeline_idle(gpointer user_data){
    StreamCtrl* ctl = static_cast<StreamCtrl*>(user_data);
    if (!ctl || !GST_IS_ELEMENT(ctl->pipeline)) return G_SOURCE_REMOVE;
    gst_element_set_state(ctl->pipeline, GST_STATE_PAUSED);
    return G_SOURCE_REMOVE; // run once
}

static gboolean start_pipeline_idle(gpointer user_data){
    g_usleep(30 * 1000* 1000); //30 seconds
    StreamCtrl* ctl = static_cast<StreamCtrl*>(user_data);
    if (!ctl || !GST_IS_ELEMENT(ctl->pipeline)) return G_SOURCE_REMOVE;
    gst_element_set_state(ctl->pipeline, GST_STATE_PLAYING);
    return G_SOURCE_REMOVE; // run once
}

static gboolean stop_pipeline_idle(gpointer user_data){
    StreamCtrl* ctl = static_cast<StreamCtrl*>(user_data);
    if (!ctl || !GST_IS_ELEMENT(ctl->pipeline))
        return G_SOURCE_REMOVE;
    gst_element_set_state(ctl->pipeline, GST_STATE_NULL);

    //gst_element_get_state(ctl->pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);
    //gst_element_get_state(ctl->pipeline, NULL, NULL, GST_SECOND * 5);
    if(!ctl->loop) {
        std::cout << "NOT LOOP\n";
        return G_SOURCE_REMOVE;
    };
        
    g_main_loop_quit(ctl->loop);
    return G_SOURCE_REMOVE; // run once
    
}



int quit_pipeline(StreamCtrl *ctrl){
    std::cout << ctrl->stream_ip <<" Stopping and Quitting the pipeline\n";
    if (!ctrl) {
        std::cout << ctrl->stream_ip << " quit_pipeline: no loop for index " << ctrl->index << "\n";
        return 0;
    }
    if (!ctrl->pipeline){
        std::cout << ctrl->stream_ip << " Not Pipeline\n";
        return 0;
    }
    g_object_set(ctrl->valve, "drop", TRUE, NULL);
    std::cout << ctrl->stream_ip <<" Valve Set to drop=TRUE\n";
    gboolean sent = gst_element_send_event(ctrl->pipeline, gst_event_new_eos());
    std::cout << ctrl->stream_ip <<" - Main Loop Quit\n";
    return 1;
}

int pipeline_teardown(GstElement *pipeline){
    GstIterator *it = gst_bin_iterate_recurse(GST_BIN(pipeline));
    GValue item = G_VALUE_INIT;

    while (gst_iterator_next(it, &item) == GST_ITERATOR_OK) {
        GstElement *elem = GST_ELEMENT(g_value_get_object(&item));

        // Force the element directly to NULL
        gst_element_set_state(elem, GST_STATE_NULL);

        // Remove element from any running scheduling
        gst_element_abort_state(elem);

        g_value_reset(&item);
    }

    g_value_unset(&item);
    gst_iterator_free(it);
    return 1;
}


// Callback to process new samples from appsink
GstFlowReturn sample_ready_callback(GstAppSink *appsink, gpointer user_data) {

    StreamCtrl* ctl = static_cast<StreamCtrl*>(user_data);
    std::mutex *mutex = (ctl->lock);

    if (ctl->restart){
        // pull frames and discard
        // GstSample* sample = nullptr;
        // g_signal_emit_by_name(ctl->appsink, "pull-sample", &sample);
        // if (sample)
        //     gst_sample_unref(sample);
        return GST_FLOW_OK;
    }


    if (!mutex->try_lock()) {
        // Another thread owns the buffer → drop frame
        // if (sample)
        //     gst_sample_unref(sample);
        return GST_FLOW_OK;
    }
    std::lock_guard<std::mutex> lock(*mutex, std::adopt_lock);
    GstSample* sample = nullptr;
    //g_signal_emit_by_name(ctl->appsink, "pull-sample", &sample); // pull sample
    sample = gst_app_sink_try_pull_sample(appsink, 0);
    if (sample){
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_READ);
        //get raw data
        unsigned char *raw_data = map.data;
        if (!raw_data) {
            gst_buffer_unmap(buffer, &map);
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }
        GstCaps *caps = gst_sample_get_caps(sample);
        GstStructure *structure = gst_caps_get_structure(caps, 0);
        int width = 0, height = 0;
        uint64_t max_size = map.size;
        ctl->im_size = map.size;


        gst_structure_get_int(structure, "width", &width);
        gst_structure_get_int(structure, "height", &height);
        

        //allocate memory
        if (ctl->image == nullptr){
            ctl->image = (unsigned char *) malloc(max_size);
            ctl->imgW = width;
            ctl->imgH = height;
        }
        else{
            if (width != ctl->imgW || height != ctl->imgH){
                std::cout << ctl->stream_ip << " Image Resolution Changed. Reallocating..\n";
                free(ctl->image);
                ctl->image = (unsigned char *) malloc(max_size);
                ctl->imgW = width;
                ctl->imgH = height;
            }
        }
        // copy the frame
        memcpy(ctl->image, raw_data, map.size);
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);

        ctl->frame_rd = true;
        ctl->timestamp = std::time(nullptr);
        ctl->state = VSTREAM_RUNNING;
        if(GST_IS_OBJECT(ctl->valve))
            g_object_set(ctl->valve, "drop", TRUE, NULL);
        return GST_FLOW_OK;
    }

    return GST_FLOW_ERROR;
}


uint32_t pull_gst_frame(StreamCtrl *ctl, unsigned char **img_buf, uint64_t *max_size){


    if (!ctl->lock->try_lock())
        return 0;
    std::lock_guard<std::mutex> lock(*ctl->lock, std::adopt_lock);

    if (ctl->frame_rd != TRUE){
        return 0;
    }
    if(ctl->restart == true){
        return 0;
    }
    //uint64_t buf_size = ctl->imgH * ctl->imgW * 3;
    uint64_t buf_size = ctl->im_size;
    if (*img_buf == nullptr){
        *img_buf = (unsigned char*)malloc(buf_size);
    }
    else{
        if (*max_size != buf_size){
            std::cout << "Reallocating\n";
            free(*img_buf);
            *img_buf = (unsigned char*)malloc(buf_size);
        }
    }
    *max_size = buf_size;
    //std::cout << ctl->stream_ip << " Copying frame\n";
    memcpy(*img_buf, ctl->image, *max_size);
    if (GST_IS_OBJECT(ctl->valve))
        g_object_set(ctl->valve, "drop", FALSE, NULL);
    return 1;
}

uint32_t pull_image(StreamCtrl *ctrl, ImgFormat format, unsigned char **img_buf, uint64_t *max_size){
    std::lock_guard<std::mutex> lock(*(ctrl->lock));
    GstStateChangeReturn ret;
    GstState current_state, pending_state;
    if (ctrl->restart == true){
        return 0;
    }

    if (ctrl->frame_rd == false){
        if (GST_IS_ELEMENT(ctrl->pipeline)){
            //g_idle_add(start_pipeline_idle, ctrl);
            // GMainContext* context = g_main_loop_get_context(ctrl->loop);
            // g_main_context_invoke(context, start_pipeline_idle, ctrl);
            if (ctrl->setState != STREAM_SET_PLAYING){
                gst_element_set_state(ctrl->pipeline, GST_STATE_PLAYING);
                ctrl->setState = STREAM_SET_PLAYING;
            }
            //g_object_set(ctrl->valve, "drop", FALSE, NULL);
        }
        else{
            return 0;
        }
        return 0;
    }

    GstSample* sample = nullptr;
    int n = 0;

    //g_signal_emit_by_name(ctrl->appsink, "pull-sample", &sample);

    sample = gst_app_sink_try_pull_sample(GST_APP_SINK(ctrl->appsink), 10 * GST_MSECOND);
    //g_object_set(ctrl->valve, "drop", TRUE, NULL);
    if (sample) {

        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_READ);
        unsigned char *raw_data = map.data;
        if (!raw_data) {
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
            case RAW:{
                memcpy(*img_buf, raw_data, *max_size);
                break;
            }
            default:{
                break;
            }
        }
        
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);

        //setting pipeline to paused
        gst_element_set_state(ctrl->pipeline, GST_STATE_PAUSED);
        ctrl->setState = STREAM_SET_PAUSED;

        // GMainContext* context = g_main_loop_get_context(ctrl->loop);
        // g_main_context_invoke(context, pause_pipeline_idle, ctrl);
        //g_object_set(ctrl->valve, "drop", TRUE, NULL);
        //g_idle_add(pause_pipeline_idle, ctrl);
        return 1;
    }
    ctrl->frame_rd = false;
    return 0;
}

static GstPadProbeReturn frame_probe_cb(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {

    StreamCtrl* ctl = static_cast<StreamCtrl*>(user_data);

    if (!ctl || !(info->type & GST_PAD_PROBE_TYPE_BUFFER)) return GST_PAD_PROBE_OK;

    if (ctl->restart) return GST_PAD_PROBE_OK;
    
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_OK;

    // Avoid blocking the streaming thread
    if (!ctl->lock->try_lock()) return GST_PAD_PROBE_OK;
    std::lock_guard<std::mutex> lock(*ctl->lock, std::adopt_lock);
    std::cout << ctl->stream_ip << " Reading the new frame\n";
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) return GST_PAD_PROBE_OK;

    int width = ctl->imgW;
    int height = ctl->imgH;
    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (caps) {
        GstStructure *s = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(s, "width", &width);
        gst_structure_get_int(s, "height", &height);
        gst_caps_unref(caps);
    }

    ctl->im_size = map.size;
    if (!ctl->image || width != ctl->imgW || height != ctl->imgH) {
        if (ctl->image){
            std::cout << ctl->stream_ip << " Allocating memory\n";
            free(ctl->image);
        }
        ctl->image = (unsigned char*)malloc(map.size);
        ctl->imgW = width;
        ctl->imgH = height;
    }
    std::cout << ctl->stream_ip << " Copying image\n";
    memcpy(ctl->image, map.data, map.size);
    std::cout << ctl->stream_ip << " Image copied\n";
    ctl->frame_rd = true;
    ctl->timestamp = std::time(nullptr);
    ctl->state = VSTREAM_RUNNING;

    gst_buffer_unmap(buffer, &map);
    if(GST_IS_OBJECT(ctl->valve))
        g_object_set(ctl->valve, "drop", TRUE, NULL);

    std::cout << ctl->stream_ip << " Image prepared\n";
    return GST_PAD_PROBE_OK;
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
            g_error_free(err);
            g_free(debug);
            ctrl->lock->lock();
            ctrl->restart = true;
            ctrl->lock->unlock();
            //gst_element_set_state(ctrl->pipeline, GST_STATE_NULL);
            g_main_loop_quit(ctrl->loop);
            break;
        }

        case GST_MESSAGE_EOS:
            std::cout << "End of Stream received — restarting pipeline" << std::endl;
            //gst_element_set_state(ctrl->pipeline, GST_STATE_NULL);
            //gst_element_set_state(ctrl->pipeline, GST_STATE_NULL);
            //gst_element_get_state(ctrl->pipeline, NULL, NULL, 0);
            g_main_loop_quit(ctrl->loop);
            break;

        default:
            break;
    }

    return TRUE;
}


static gboolean periodic_tick_continious(gpointer user_data){
  StreamCtrl *ctrl = static_cast<StreamCtrl*> (user_data);

  if (! ctrl->run || ctrl->restart == true) {
        std::lock_guard<std::mutex> lock(*(ctrl->lock));
        std::cout << "Cam " << ctrl->index << " Playback is closing!\n";
        return false;
    }
    return true;
}

void init_camstream(void){
    gst_init(NULL, NULL);
}

uint32_t create_gst_pipeline(uint32_t id, std::string url, StreamPipeline *p){
    std::cout << url << " Creating Streamer Pipeline\n";
    std::string name = "rtsp-" + url;
    p->pipeline     = gst_pipeline_new(name.c_str());
    p->rtspsrc      = gst_element_factory_make("rtspsrc", "src");
    p->depay        = gst_element_factory_make("rtph264depay", nullptr);
    p->parser       = gst_element_factory_make("h264parse", nullptr);
    p->queue1       = gst_element_factory_make("queue", nullptr);
    p->valve        = gst_element_factory_make("valve", "valve");
    //p->decodebin    = gst_element_factory_make("decodebin", nullptr);
    p->decodebin    = gst_element_factory_make("avdec_h264", nullptr);
    p->videoconvert = gst_element_factory_make("videoconvert", nullptr);
    p->capsfilter   = gst_element_factory_make("capsfilter", nullptr);
    p->queue2       = gst_element_factory_make("queue", nullptr);
    //p->appsink      = gst_element_factory_make("appsink", "sink");
    p->appsink      = gst_element_factory_make("fakesink", "sink");

    if(!p->pipeline){
        std::cout << "Could not Create pipeline for - " << url << std::endl;
        return 0;
    }
    if(!p->rtspsrc){
        std::cout << "Could not Create rtspsrc for - " << url << std::endl;
        return 0;
    }
    if(!p->depay){
        std::cout << "Could not Create depay for - " << url << std::endl;
        return 0;
    }
    if(!p->parser){
        std::cout << "Could not Create parser for - " << url << std::endl;
        return 0;
    }
    if(!p->queue1){
        std::cout << "Could not Create queue1 for - " << url << std::endl;
        return 0;
    }
    if(!p->valve){
        std::cout << "Could not Create valve for - " << url << std::endl;
        return 0;
    }
    if(!p->decodebin){
        std::cout << "Could not Create decodebin for - " << url << std::endl;
        return 0;
    }
    if(!p->videoconvert){
        std::cout << "Could not Create videoconvert for - " << url << std::endl;
        return 0;
    }
    if(!p->capsfilter){
        std::cout << "Could not Create capsfilter for - " << url << std::endl;
        return 0;
    }
    if(!p->queue2){
        std::cout << "Could not Create queue2 for - " << url << std::endl;
        return 0;
    }
    if(!p->appsink){
        std::cout << "Could not Create appsink for - " << url << std::endl;
        return 0;
    }

    g_object_set(p->rtspsrc, "location", url.c_str(), "protocols", GST_RTSP_LOWER_TRANS_TCP,
        "latency", 200, NULL);
    g_object_set(p->rtspsrc, "drop-on-latency", TRUE, NULL);

    g_object_set(p->queue1, "leaky", 2, "max-size-buffers", 1, "max-size-time", 
        (guint64)0, "max-size-bytes", (guint)0, NULL);
    g_object_set(p->queue2, "leaky", 2, "max-size-buffers", 1, "max-size-time", 
        (guint64)0, "max-size-bytes", (guint)0, NULL);

    g_object_set(p->valve, "drop", FALSE, NULL);

    GstCaps *caps = gst_caps_from_string("video/x-raw,format=RGB");
    g_object_set(p->capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    // g_object_set(p->appsink, "emit-signals", TRUE,
    //     "sync", FALSE, "max-buffers", 1, "drop", TRUE, NULL);
    
    g_object_set(p->appsink, "sync", FALSE, "qos", TRUE, "enable-last-sample", FALSE, NULL);
    
    gst_bin_add_many(GST_BIN(p->pipeline), p->rtspsrc, p->depay, p->parser,
        p->queue1, p->valve, p->decodebin, p->videoconvert, p->capsfilter,
        p->queue2, p->appsink, NULL);
    
    gst_element_link_many( p->depay, p->parser,
            p->queue1, p->valve, p->decodebin, p->videoconvert, p->capsfilter,
        p->queue2, p->appsink, NULL);
    
    //static links
    //rtspsrc -> depay linked dinamically
    // gst_element_link_many( p->depay, p->parser,
    //     p->queue1, p->valve, p->decodebin, NULL);
    // //decodebin -> videconvert linked dinamically
    // gst_element_link_many(p->videoconvert, p->capsfilter,
    //     p->queue2, p->appsink, NULL);

    return 1;
}

static void on_decode_pad_added(GstElement *src, GstPad *pad, gpointer data) {
    GstElement *videoconvert = GST_ELEMENT(data);
    GstPad *sinkpad = gst_element_get_static_pad(videoconvert, "sink");

    GstCaps *caps = gst_pad_get_current_caps(pad);
    const GstStructure *str = gst_caps_get_structure(caps, 0);
    const gchar *name = gst_structure_get_name(str);

    if (g_str_has_prefix(name, "video/")) {
        if (!gst_pad_is_linked(sinkpad)) {
            gst_pad_link(pad, sinkpad);
        }
    }

    gst_caps_unref(caps);
    gst_object_unref(sinkpad);
}

static void on_rtsp_pad_added(GstElement *src, GstPad *pad, gpointer data) {
    GstElement *depay = GST_ELEMENT(data);
    GstPad *sinkpad = gst_element_get_static_pad(depay, "sink");

    if (!gst_pad_is_linked(sinkpad)) {
        gst_pad_link(pad, sinkpad);
    }
    gst_object_unref(sinkpad);
}

static gboolean bus_cb(GstBus *bus, GstMessage *msg, gpointer user_data) {
    auto *p = static_cast<StreamCtrl*>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {

    case GST_MESSAGE_ERROR: {
        GError *err;
        gchar *dbg;
        gst_message_parse_error(msg, &err, &dbg);

        g_printerr("GST ERROR: %s\n", err->message);

        g_error_free(err);
        g_free(dbg);

        // Stop pipeline cleanly
        gst_element_set_state(p->pipeline, GST_STATE_NULL);

        // You can trigger reconnect logic here
        // Optional: flush old data
        gst_element_send_event(p->pipeline, gst_event_new_flush_start());
        gst_element_send_event(p->pipeline, gst_event_new_flush_stop(TRUE));

        //g_main_context_invoke(p->context, start_pipeline_idle, p);
        // Restart
        //gst_element_set_state(p->pipeline, GST_STATE_PLAYING);
        //p->restart = false;
        //std::cout << "Set to playing\n";
        // e.g. schedule restart in another thread
        //return FALSE;
    }

    case GST_MESSAGE_EOS:
        gst_element_set_state(p->pipeline, GST_STATE_NULL);

        // Optional: flush old data
        gst_element_send_event(p->pipeline, gst_event_new_flush_start());
        gst_element_send_event(p->pipeline, gst_event_new_flush_stop(TRUE));

        //g_main_context_invoke(p->context, start_pipeline_idle, p);
        // Restart
        //gst_element_set_state(p->pipeline, GST_STATE_PLAYING);
        //p->restart = false;
        //return FALSE;

    default:
        break;
    }
    return TRUE;
}

int bus_listener(GstBus *bus, GstElement *pipeline, StreamCtrl *ctrl){
    GstMessage *msg;

    bool terminate = false;

    do {
    msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, 
       (GstMessageType) (GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    /* Parse message */
    if (msg != NULL) {
      GError *err;
      gchar *debug_info;

      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
          gst_message_parse_error (msg, &err, &debug_info);
          g_printerr ("Error received from element %s: %s\n",
              GST_OBJECT_NAME (msg->src), err->message);
          g_printerr ("Debugging information: %s\n",
              debug_info ? debug_info : "none");
          g_clear_error (&err);
          g_free (debug_info);
          terminate = TRUE;
          break;
        case GST_MESSAGE_EOS:
          g_print ("End-Of-Stream reached.\n");
          terminate = TRUE;
          break;
        case GST_MESSAGE_STATE_CHANGED:
          /* We are only interested in state-changed messages from the pipeline */
          if (GST_MESSAGE_SRC (msg) == GST_OBJECT (pipeline)) {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed (msg, &old_state, &new_state,
                &pending_state);
            g_print ("Pipeline state changed from %s to %s:\n",
                gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
          }
          break;
        default:
          /* We should not reach here */
          g_printerr ("Unexpected message received.\n");
          break;
      }
      gst_message_unref (msg);
    }
  } while (!terminate);
  return 1;
}


uint32_t start_manual_pipeline(int id, std::string rtsp_url, StreamCtrl *ctrl){
    StreamPipeline p;
    std::cout << ctrl->stream_ip << " Building pipeline\n";
    uint32_t ret = create_gst_pipeline(id, rtsp_url, &p);
    if (!ret){
        std::cout << "Failed to create pipeline for " << rtsp_url << std::endl;
        return 0;
    }
    std::cout << ctrl->stream_ip << " Pipeline Created\n";
    //copy the required pointer to ctrl structure
    ctrl->pipeline = p.pipeline;
    ctrl->appsink = p.appsink;
    ctrl->valve = p.valve;
    
    //guint sample_handler_id = g_signal_connect(ctrl->appsink, "new-sample", G_CALLBACK(sample_ready_callback), ctrl);
    //ctrl->sampleh_id = sample_handler_id;

    guint padadd_sig_1 = g_signal_connect(p.rtspsrc, "pad-added", G_CALLBACK(on_rtsp_pad_added), p.depay);
    //guint padadd_sig_2 = g_signal_connect(p.decodebin, "pad-added", G_CALLBACK(on_decode_pad_added), p.videoconvert);

    GstPad *probe_pad = gst_element_get_static_pad(p.capsfilter, "src");
    ctrl->probe_id = gst_pad_add_probe(probe_pad, GST_PAD_PROBE_TYPE_BUFFER, frame_probe_cb, ctrl, NULL);
    gst_object_unref(probe_pad);

    // dont create bus
    //ctrl->context = g_main_context_new();
    //ctrl->loop = g_main_loop_new(ctrl->context, FALSE);

    // reset the stream control
    std::mutex *mutex = (ctrl->lock);
    mutex->lock();
    reset_stream_control(ctrl);
    mutex->unlock();

    /* Start playing */
    ret = gst_element_set_state (ctrl->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (ctrl->pipeline);
        return 0;
    }

    //create bus
    GstBus *bus = gst_element_get_bus(ctrl->pipeline);
    // GSource *bus_source = gst_bus_create_watch(bus);
    // g_source_set_callback(bus_source, (GSourceFunc)bus_cb, ctrl, nullptr);
    // guint bus_watch_id = g_source_attach(bus_source, ctrl->context);
    // ctrl->bus_watch_id = bus_watch_id;
    // manual bus listener
    while (1){
        std::cout << ctrl->stream_ip << " Starting bus listener\n";
        bus_listener(bus, ctrl->pipeline, ctrl);
        gst_element_send_event(ctrl->pipeline, gst_event_new_flush_start());
        gst_element_send_event(ctrl->pipeline, gst_event_new_flush_stop(TRUE));
        gst_element_set_state (ctrl->pipeline, GST_STATE_NULL);
        gst_element_get_state(ctrl->pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);
        // Detach pad probe before restart to avoid dangling callbacks
        GstPad *probe_pad = gst_element_get_static_pad(p.capsfilter, "src");
        if (probe_pad && ctrl->probe_id) {
            gst_pad_remove_probe(probe_pad, ctrl->probe_id);
            ctrl->probe_id = 0;
        }
        if (probe_pad) gst_object_unref(probe_pad);
        mutex->lock();
        ctrl->restart = true;
        mutex->unlock();
        sleep(60);
        mutex->lock();
        reset_stream_control(ctrl);
        mutex->unlock();
                // Reinstall probe on new capsfilter
        GstPad *new_pad = gst_element_get_static_pad(p.capsfilter, "src");
        ctrl->probe_id = gst_pad_add_probe(new_pad, GST_PAD_PROBE_TYPE_BUFFER, frame_probe_cb, ctrl, NULL);
        gst_object_unref(new_pad);
        std::cout << ctrl->stream_ip << " Playing pipeline\n";
        gst_element_set_state (ctrl->pipeline, GST_STATE_PLAYING);
    }
    bus_listener(bus, ctrl->pipeline, ctrl);
    
    mutex->lock();
    ctrl->restart = true;
    mutex->unlock();
    std::cout << ctrl->stream_ip << " bus listener returned\n";
    //free resources
    gst_object_unref (bus);
    gst_element_set_state (ctrl->pipeline, GST_STATE_NULL);

    // if(ctrl->appsink && sample_handler_id){
    //     std::cout << ctrl->stream_ip << " removing sample_handler\n";
    //     g_signal_handler_disconnect(ctrl->appsink, sample_handler_id);
    //     std::cout << ctrl->stream_ip << "g_signal disconnected\n";
    // }
    std::cout << ctrl->stream_ip << "Removing pad probe\n";
    probe_pad = gst_element_get_static_pad(p.capsfilter, "src");
    if (probe_pad && ctrl->probe_id) {
        gst_pad_remove_probe(probe_pad, ctrl->probe_id);
        ctrl->probe_id = 0;
    }
    if (probe_pad) gst_object_unref(probe_pad);
    std::cout << ctrl->stream_ip << " Pad Probe Removed\n";

    // if(p.depay && padadd_sig_1){
    //     //g_signal_handler_disconnect(p.depay, padadd_sig_1);
    //     std::cout << ctrl->stream_ip << " padadd depay sig disconnected\n";
    // }

    if (ctrl->pipeline){
        gst_object_unref (ctrl->pipeline);
        std::cout << ctrl->stream_ip << " Pipeline Aufiderzein\n";
    }
    return 1;
}


/***
 * @brief Creates a gstreamer pipeline for h264 rtsp stream parse. Pipeline needs to be manually preloaded for frame pulls
 * @param rtsp_url rtsp stream url
 * @param ctrl reference to pipeline control structure
 */
uint32_t create_pipeline_multi_frame_manual(std::string rtsp_url, StreamCtrl *ctrl){

    std::cout << ctrl->stream_ip << " Creating Streamer Pipeline\n";
    //gst_init(NULL, NULL);
    std::string gst_launch =
        //"rtspsrc location=" + rtsp_url + " protocols=tcp latency=2000 retry=3 "
        "rtspsrc location=" + rtsp_url + " protocols=tcp "
        "! rtph264depay ! h264parse "
        "! queue leaky=2 max-size-buffers=1 "
        "! valve name=valve drop=false "
        "! decodebin "
        "! videoconvert "
        "! video/x-raw,format=RGB "
        "! queue leaky=2 max-size-buffers=1 "
        "! appsink name=sink emit-signals=true sync=false";
    //std::cout << "Get Pipeline pointer\n";
    ctrl->pipeline = gst_parse_launch(gst_launch.c_str(), NULL);
    //std::cout << "Get Appsink pointer\n";
    ctrl->appsink = gst_bin_get_by_name(GST_BIN(ctrl->pipeline), "sink");
    //std::cout << "Get Valve\n";
    ctrl->valve = gst_bin_get_by_name(GST_BIN(ctrl->pipeline), "valve");
    //std::cout << "Get main loop pointer\n";
    ctrl->context = g_main_context_new();
    ctrl->loop = g_main_loop_new(ctrl->context, FALSE);

    //std::cout << "Set State to STARTUP\n";
    if (!ctrl->pipeline || !ctrl->appsink || !ctrl->valve || !ctrl->loop || !ctrl->context) {
        g_printerr("%s Failed to create pipeline/elements/loop\n", ctrl->stream_ip.c_str());
        // cleanup everything allocated so far
        if (ctrl->appsink)  { gst_object_unref(ctrl->appsink);  ctrl->appsink = nullptr; }
        if (ctrl->valve)    { gst_object_unref(ctrl->valve);    ctrl->valve   = nullptr; }
        if (ctrl->pipeline) { gst_object_unref(ctrl->pipeline); ctrl->pipeline = nullptr; }
        if (ctrl->loop)     { g_main_loop_unref(ctrl->loop);    ctrl->loop    = nullptr; }
        if (ctrl->context)  { g_main_context_unref(ctrl->context); ctrl->context = nullptr; }
        return 0;
    }
    g_object_set(ctrl->appsink, "drop", true, "max-buffers", 1, NULL);
    g_object_set(ctrl->valve, "drop", FALSE, NULL);

    std::cout << ctrl->stream_ip << " Pipeline created!\n";
    guint sample_handler_id = g_signal_connect(ctrl->appsink, "new-sample", G_CALLBACK(sample_ready_callback), ctrl);
    ctrl->sampleh_id = sample_handler_id;
    

    gst_element_set_state(ctrl->pipeline, GST_STATE_PLAYING);

    //create bus
    GstBus *bus = gst_element_get_bus(ctrl->pipeline);
    GSource *bus_source = gst_bus_create_watch(bus);
    g_source_set_callback(bus_source, (GSourceFunc)bus_call, ctrl, nullptr);
    guint bus_watch_id = g_source_attach(bus_source, ctrl->context);
    ctrl->bus_watch_id = bus_watch_id;

    std::mutex *mutex = (ctrl->lock);
    mutex->lock();
    reset_stream_control(ctrl);
    mutex->unlock();

    g_main_loop_run(ctrl->loop);
    std::cout << ctrl->stream_ip << " Loop Returned\n";
    
    //cleanup
    std::cout  << ctrl->stream_ip << " Destroying bus_source\n";
    if (bus_source){
        g_source_destroy(bus_source);
        g_source_unref(bus_source);
        gst_object_unref(bus);
        std::cout  << ctrl->stream_ip << " bus destroyed\n";
    }
    // if (ctrl->bus_watch_id){
    //     std::cout  << ctrl->stream_ip << "  removing bus_watch\n";
    //     g_source_remove(ctrl->bus_watch_id);
    //     ctrl->bus_watch_id=0;
    //     std::cout << ctrl->stream_ip << " bus_watch removed\n";
    // }
    if(ctrl->appsink && sample_handler_id){
        std::cout << ctrl->stream_ip << " removing sample_handler\n";
        g_signal_handler_disconnect(ctrl->appsink, sample_handler_id);
        std::cout << ctrl->stream_ip << "g_signal disconnected\n";
    }

    GstState state, pending;
    if(GST_IS_ELEMENT(ctrl->pipeline)){

        std::cout << ctrl->stream_ip << " - Setting Pipeline to GST_STATE_NULL\n";

        GstStateChangeReturn ret = gst_element_set_state(ctrl->pipeline, GST_STATE_NULL);
        if (ret == GST_STATE_CHANGE_ASYNC){
            std::cout << ctrl->stream_ip << " StateChangeReturn = GST_STATE_CHANGE_ASYNC\n";
        }
        else if(ret == GST_STATE_CHANGE_FAILURE){
            std::cout << ctrl->stream_ip << " StateChangeReturn = GST_STATE_CHANGE_FAILURE\n";
        }
        else if(ret == GST_STATE_CHANGE_SUCCESS){
            std::cout << ctrl->stream_ip << " StateChangeReturn = GST_STATE_CHANGE_SUCCESS\n";
        }
        else if(ret == GST_STATE_CHANGE_NO_PREROLL){
            std::cout << ctrl->stream_ip << " StateChangeReturn = GST_STATE_CHANGE_NO_PREROLL\n";
        }
        else{
            std::cout << ctrl->stream_ip << " StateChangeReturn = Something else\n";
        }

        gst_element_get_state(ctrl->pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
        std::cout << ctrl->stream_ip << " pipeline state - " << gst_element_state_get_name(state) << std::endl;
        std::cout << ctrl->stream_ip << " pipeline pending - " << gst_element_state_get_name(pending) << std::endl;
    }

    if(ctrl->appsink){
        gst_object_unref(ctrl->appsink);
        ctrl->appsink = nullptr;
        std::cout << ctrl->stream_ip << " appsink unreffed\n";
    }
    if(ctrl->valve){
        gst_object_unref(ctrl->valve);
        ctrl->valve = nullptr;
        std::cout << ctrl->stream_ip << " valve unreffed\n";
    }
    if(ctrl->pipeline){
        gst_object_unref(ctrl->pipeline);
        ctrl->pipeline = nullptr;
        std::cout << ctrl->stream_ip << " pipeline unreffed\n";
    }
    if(ctrl->loop){
        g_main_loop_unref(ctrl->loop);
        ctrl->loop = nullptr;
        std::cout << ctrl->stream_ip << " loop unreffed\n";
    }
    if (ctrl->context){
        g_main_context_pop_thread_default(ctrl->context);
        g_main_context_unref(ctrl->context);
        ctrl->context = nullptr;
        std::cout << ctrl->stream_ip << " Context Unreffed\n";
    }
    //ctrl->context = NULL;
    return 1;
}



/**
 * @brief Manual Pipeline loop
 * @param rtsp_url rtsp stream url
 * @param ctrl stream control struct
 */
int pipeline_manual(int id, const char *rtsp_url, StreamCtrl *ctrl){
    while(ctrl->run){
        //create_pipeline_multi_frame_manual(rtsp_url, ctrl);
        start_manual_pipeline(id ,rtsp_url, ctrl);
        std::cout << "Playback ended. Closing...\n";
        std::this_thread::sleep_for(std::chrono::seconds(60));
        //break;
    }
    return 1;
}

/**
 * @brief Manual frame pulling gstreamer pipeline in a thread
 * @param rtsp_url rtsp stream url
 * @param ctrl stream control struct
 * @return vstream thread
 */
vstream load_manual_stream(int id, const char *rtsp_url, StreamCtrl *ctrl){
    return vstream(pipeline_manual, id, rtsp_url, ctrl);
}

