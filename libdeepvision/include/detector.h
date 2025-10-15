/**
 * @file    detector.h
 * @brief   Implements vehicle detection using ONNXruntime and opencv.
 * @author  Jonas Vaicekauskas
 * @date    2025-07-03
 * @details
 * Contains core classes and functions for car license plate and its text detection and extraction
 * 
 */
#ifndef DETECTOR_H
#define DETECTOR_H

#include "onnxruntime_cxx_api.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>
#include <thread>
#include <sys/stat.h>
#include <filesystem>
#include <measure_time.h>

#include "camstream.h"
#include "streammuxer.h"

#define USE_CUDA true
#define YOLO_INPUT_W 640
#define YOLO_INPUT_H 640

#define LPRNET_INPUT_W 96
#define LPRNET_INPUT_H 48

#define VERBOSE true

#define THREADS 4

#define WORK_DIR   "/mnt/c/p/"
#define IMAGES_DIR "/mnt/c/p/images/"
#define DETAI_DIR  "/mnt/c/p/detai/"
#define VISUALIZE_DIR "/mnt/c/p/visualize/"

#define VEHICLE_MODEL_PATH "yolo11_indoor.onnx"
#define VEHICLE_DET_CONFIDENCE_THRESHOLD 0.2
#define LPD_MODEL_PATH "license_plate_detector.onnx"
#define LPLATE_DET_CONFIDENCE_THRESHOLD 0.5
#define LPR_MODEL_PATH "us_lprnet_baseline18_deployable.onnx"

class ImgReader;

struct bbox{
    float x1;
    float x2;
    float y1;
    float y2;
    float conf; //confidence score 0.0 to 1.0
    float cid;  // class id
};
/**
 * @brief Holds detection information from the image
 */
struct parknetDet{
    bbox car;
    bbox lplate;
    std::string plText; 
    std::string type = "";
    std::string make = "";
    std::string model = "";
    std::string color = "";
    bool lpr_found = false;
    int shutter = 0;
};

typedef enum {
    VEHICLE_MODEL = 0,
    LP_MODEL,
    OCR_MODEL
}model_t;

namespace ImgUtils {/* Private function to load the image file*/
    inline cv::Mat load_image(const char * img_path){
            // cv::Mat img = cv::imread(img_path, cv::IMREAD_COLOR_RGB);
            cv::Mat img = cv::imread(img_path, cv::IMREAD_COLOR);
            if (img.empty()){
                std::cout << "Could not load the image" << std::endl;
                return img;
            }
            return img;
    }

    inline cv::Mat preprocess_image(cv::Mat src_img){
            cv::Mat convRGB;
            cv::cvtColor(src_img, convRGB, cv::COLOR_BGR2RGB, 0);
            //cv::resize(convRGB, resized, cv::Size(dst_w, dst_h));
            convRGB.convertTo(convRGB, CV_32F, 1.0 / 255);
            return convRGB;
    }

    inline cv::Mat resize_image(cv::Mat src_img, int dst_w, int dst_h){
        cv::Mat res;
        if (dst_w == 0 || dst_h == 0 || src_img.empty()){
            return res;
        }
        cv::resize(src_img, res, cv::Size(dst_w, dst_h));
        return res;
    }

    inline cv::Mat crop_image(cv::Mat src_img, int x0, int y0, int x1, int y1){
        if (x0 < 0)
        x0=0;
        if(y0 < 0)
        y0=0;
        //std::cout << "("<<x0<<","<<y0<<") "<< "("<<x1<<","<<y1<<")\n";
        if (x1 > src_img.cols)
            x1 = src_img.cols;
        if(y1 > src_img.rows)
            y1 = src_img.rows;
        cv::Rect roi(x0, y0, x1-x0, y1-y0);
        cv::Mat ret = src_img(roi);
        return ret;
    }

    inline std::vector<std::string> get_files_in_directory(const std::string& directory_path) {
        std::vector<std::string> files;
        
        try {
            for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
                if (entry.is_regular_file()) {
                    //files.push_back(entry.path().string()); //full path
                    files.push_back(entry.path().filename()); // just file names
                }
            }
        } catch (const std::filesystem::filesystem_error& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
        
        return files;
    }

    inline std::vector<std::string> filter_img_list(std::vector<std::string> list){
        std::vector <std::string> ret;
        for (const auto & f:list){
            if (f.find("e") == std::string::npos){
                ret.push_back(f);
            }
        }
        return ret;
    }
}

namespace detector {
    inline bbox max_bbox(std::vector<bbox> bboxes){
        float max = 0.0;
        bbox ret;
        for (const auto & b:bboxes){
            if (b.conf > max){
                max = b.conf;
                ret = b;
            }
        }
        return ret;
    }  
}





/**
 * @class LPRNetDetector
 * @brief Detector Engine for Text extraction from license plates.
 * 
 * Handles image preprocessing, inference, and postprocessing.
 */

class LPRNetDetector{
    private:
        const char *name;
        const int INPUT_W = LPRNET_INPUT_W;
        const int INPUT_H = LPRNET_INPUT_H;
        const std::vector<std::string> ALPHABET = {
            "0","1","2","3","4","5","6","7","8","9",
            "A","B","C","D","E","F","G","H","I","J","K","L","M","N","P","Q","R","S","T","U","V","W","X","Y","Z",
            ""};

        Ort::Env env;
        Ort::SessionOptions session_options;
        Ort::Session session;

        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        cv::Mat img;

        std::string input_names;
        std::string output_names;
        std::vector<const char*> input_names_raw;
        std::vector<const char*> output_names_raw;

        std::vector<float> input_tensor_values;
        std::array<int64_t, 4> input_shape{1, 3, INPUT_H, INPUT_W};
        std::vector<Ort::Value> output_tensors;
    
        static Ort::SessionOptions create_session_options() {
            Ort::SessionOptions options;
            if (USE_CUDA) {
                std::cout << "Adding CUDA provider..." << std::endl;
                try {
                    OrtCUDAProviderOptions cuda_options;
                    options.AppendExecutionProvider_CUDA(cuda_options);
                    //options.SetLogSeverityLevel(0);
                } catch (const std::exception& e) {
                    std::cout << "Failed to add CUDA provider: " << e.what() << std::endl;
                }
            }
            //options.SetExecutionMode(ORT_PARALLEL);
            //options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            //options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
            options.SetIntraOpNumThreads(1);
            return options;
        }

    void init(void){

        auto input_names_p = session.GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        auto output_names_p = session.GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions());

        input_names = input_names_p.get();
        output_names = output_names_p.get();

        input_names_raw = {input_names.c_str()};
        output_names_raw = {output_names.c_str()};
        //std::cout << "Output Names: " << output_names << std::endl;
        
    }
    Ort::Value load_input_tensor(cv::Mat img){

        // Convert HWC to CHW
        for (int c = 0; c < 3; ++c)
            for (int y = 0; y < INPUT_H; ++y)
                for (int x = 0; x < INPUT_W; ++x)
                    input_tensor_values.push_back(img.at<cv::Vec3f>(y, x)[c]);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor_values.data(), input_tensor_values.size(), 
            input_shape.data(), input_shape.size());
        return input_tensor;
    }

    void run(Ort::Value &input_tensor){
        output_tensors = session.Run(Ort::RunOptions{nullptr}, input_names_raw.data(),
                    &input_tensor, 1, output_names_raw.data(), 1);
    }

    std::string postprocess(void){
        int* output = output_tensors.front().GetTensorMutableData<int>();
        auto tensor_info = output_tensors.front().GetTensorTypeAndShapeInfo();
        auto shape = tensor_info.GetShape();
        // Print output shape
        // std::cout << "LPRNet output shape: (";
        // for (size_t i = 0; i < shape.size(); ++i) {
        //     std::cout << shape[i];
        //     if (i != shape.size() - 1) std::cout << ", ";
        // }
        // std::cout << ")" << std::endl;
        int seq_len = shape[1];      // e.g., 18
        std::string plate;
        int prev_class = -1;
        for (int i = 0; i < seq_len; ++i) {
            //std::cout << (float)output[i] << std::endl;
            if (output[i] != prev_class && output[i] != 35) { 
                plate += ALPHABET[output[i]];
            }
            prev_class = output[i];
        }
        //std::cout << "Predicted plate: " << plate << std::endl;
        return plate;
    }
    void clear_tensors(void){
        input_tensor_values.clear();
        output_tensors.clear();
    }
    public:
    LPRNetDetector(const char * name, const char * model_path)
        : name(name), 
        env(ORT_LOGGING_LEVEL_ERROR, name), 
        session_options(create_session_options()),
        session(env, model_path, session_options)
        {
            std::cout << "Initializing LPRNet Engine\n";
            std::cout << "Loading model " << model_path << std::endl;
            init();
        }

    std::string detect_from_file(const char * img_path){
        std::string ret = "";
        img = ImgUtils::load_image(img_path);
        if (img.empty()){
            return ret;
        }
        cv::Mat prep_img = ImgUtils::preprocess_image(img);
        cv::Mat fit_img = ImgUtils::resize_image(prep_img, INPUT_W, INPUT_H);
        Ort::Value input_tensor = load_input_tensor(fit_img);
        run(input_tensor);
        ret = postprocess();
        clear_tensors();
        return ret;
    }
    std::string detect_preproc(cv::Mat src_i, bool visualize = false){
        std::string ret = "";
        img = src_i;

        cv::Mat fit_img = ImgUtils::resize_image(src_i, INPUT_W, INPUT_H);
        if(fit_img.empty()){
            return ret;
        }
        Ort::Value input_tensor = load_input_tensor(fit_img);
        run(input_tensor);
        ret = postprocess();
        clear_tensors();
        return ret;
    }

};


class OnnxDetector{
    private:
    const char * name;
    const int INPUT_W = YOLO_INPUT_W;
    const int INPUT_H = YOLO_INPUT_H;

    const std::vector<std::string> COCO80C = {
        "person", "bycicle", "car", "motorcycle", "airplane", "bus", "train", "truck"
    };
    Ort::Env env;
    Ort::SessionOptions session_options;
    Ort::Session session;

    std::vector<float> input_tensor_values;
    std::vector<Ort::Value> output_tensors;

    float threshold;

    std::array<int64_t, 4> input_shape{1, 3, INPUT_H, INPUT_W};
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::AllocatorWithDefaultOptions allocator = Ort::AllocatorWithDefaultOptions();
    std::string input_names;
    std::string output_names;
    std::vector<const char*> input_names_raw;
    std::vector<const char*> output_names_raw;
    cv::Mat img;

    static Ort::SessionOptions create_session_options() {
        Ort::SessionOptions options;
        if (USE_CUDA) {
            std::cout << "Adding CUDA provider..." << std::endl;
            try {
                OrtCUDAProviderOptions cuda_options;
                options.AppendExecutionProvider_CUDA(cuda_options);
            } catch (const std::exception& e) {
                std::cout << "Failed to add CUDA provider: " << e.what() << std::endl;
            }
        }
        options.SetIntraOpNumThreads(1);
        return options;
    }

    void init(void){

        auto input_names_p = session.GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
        auto output_names_p = session.GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions());

        input_names = input_names_p.get();
        output_names = output_names_p.get();

        input_names_raw = {input_names.c_str()};
        output_names_raw = {output_names.c_str()};
        //std::cout << "Output Names: " << output_names << std::endl;
        // After session creation, check providers
    std::vector<std::string> providers =  Ort::GetAvailableProviders();
    std::cout << "Available providers: ";
    for (const auto& provider : providers) {
        std::cout << provider << " ";
    }
    std::cout << std::endl;
        
    }


    Ort::Value load_input_tensor(cv::Mat src_img){
        // Convert HWC to CHW
        for (int c = 0; c < 3; ++c)
            for (int y = 0; y < INPUT_H; ++y)
                for (int x = 0; x < INPUT_W; ++x)
                    input_tensor_values.push_back(src_img.at<cv::Vec3f>(y, x)[c]);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor_values.data(), input_tensor_values.size(), 
            input_shape.data(), input_shape.size());
        return input_tensor;
    }

    void run(Ort::Value &input_tensor){
        output_tensors = session.Run(Ort::RunOptions{nullptr}, input_names_raw.data(),
                    &input_tensor, 1, output_names_raw.data(), 1);
    }

    std::vector<bbox> postprocess(void){
        float *output = output_tensors.front().GetTensorMutableData<float>();
        auto tensor_info = output_tensors.front().GetTensorTypeAndShapeInfo();
        auto shape = tensor_info.GetShape();
        size_t num_elements = output_tensors.front().GetTensorTypeAndShapeInfo().GetElementCount();
        int num_detections = num_elements / shape[2];
        // Display detections
        std::vector<bbox> detections;
        int iw = img.cols;
        int ih = img.rows;
        float sw = (float)iw/INPUT_W;
        float sh = (float)ih/INPUT_H;
        for (int i = 0; i < num_detections; ++i) {
            bbox det;
            det.x1 = output[i * 6 + 0] * sw;
            det.y1 = output[i * 6 + 1] * sh;
            det.x2 = output[i * 6 + 2]* sw;
            det.y2 = output[i * 6 + 3] * sh;
            det.conf = output[i * 6 + 4];
            det.cid = output[i * 6 + 5];
            
            if (det.conf >= threshold ) {
                detections.push_back(det);
            }
        }
        return detections;
    }

    void clear_tensors(void){
        input_tensor_values.clear();
        output_tensors.clear();
    }

    void display_output(std::vector<bbox> detections){
        for (const auto & d:detections){
            cv::rectangle(img, cv::Point(d.x1, d.y1), cv::Point(d.x2, d.y2), cv::Scalar(0, 255, 0), 2);
            cv::putText(img, std::to_string(int(d.cid)), cv::Point(d.x1, d.y1 - 5), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
        }
        cv::imshow("Yolo Detections", img);
        cv::waitKey(0);
    }

    void visualize_bboxes(std::vector<bbox> detections){
        for (const auto & d:detections){
            // scale detections to original image;
            cv::rectangle(img, cv::Point(d.x1, d.y1), cv::Point(d.x2, d.y2), cv::Scalar(255, 0, 0), 2);
            //cv::putText(img, std::to_string(int(d.cid)), cv::Point(d.x1, d.y1 - 5), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
            cv::putText(img, COCO80C[(int)d.cid], cv::Point(d.x1, d.y1 - 5), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
        }
        cv::imwrite("output.png", img);
    }

    public:

    OnnxDetector(const char * name, const char * model_path, float threshold)
        : name(name), 
        env(ORT_LOGGING_LEVEL_ERROR, name), 
        session_options(create_session_options()),
        session(env, model_path, session_options), 
        threshold(threshold)
    {
            init();
            std::cout << "Loading model " << model_path << std::endl;
            std::cout << "Model threshold set to - " << (float) threshold << std::endl;
    }

    /**
     * @brief Load and run image file through the model
     * @param img_path path to the image file
     * @param visualize save visualization of the detection
     * @return vector of detected bboxes
     */
    std::vector<bbox> detect_from_file(const char * img_path, bool visualize = false){
        img = ImgUtils::load_image(img_path);
        std::vector<bbox> ret;
        if (img.empty()){
            return ret;
        }
        cv::Mat prep_img = ImgUtils::preprocess_image(img);
        cv::Mat fit_img = ImgUtils::resize_image(prep_img, INPUT_W, INPUT_H);
        Ort::Value input_tensor = load_input_tensor(fit_img);
        run(input_tensor);
        std::vector<bbox> dets = postprocess();
        clear_tensors();
        if(visualize){
            visualize_bboxes(dets);
        }
        return dets;
    }
    /**
     * @brief Run preprocessed (must be RGB and normalized to 1.0/255) image
     * through the model
     * @param src_i preprocessed image
     * @param visualize save visualization of the detection
     * @return vector of detected bboxes
     */
    std::vector<bbox> detect_preproc(cv::Mat src_i, bool visualize = false){
        std::vector<bbox> dets;
        img = src_i;

        cv::Mat fit_img = ImgUtils::resize_image(src_i, INPUT_W, INPUT_H);
        int rwi = fit_img.cols;
        int rhe = fit_img.rows;

        if (fit_img.empty()){
            return dets;
        }
        Ort::Value input_tensor = load_input_tensor(fit_img);
        run(input_tensor);
        dets = postprocess();
        clear_tensors();
        if(visualize){
            visualize_bboxes(dets);
        }
        return dets;
    }
};



namespace wdet{
    inline bool dir_exists(std::string dir){
        struct stat info;
        if( stat( dir.c_str(), &info ) != 0 ){
            //std::cout << "Cannot access " << dir << std::endl;
            return false;
        }
        else if( info.st_mode & S_IFDIR ){
            //std::cout << dir << "is directory" << std::endl;
            return true;
        }
        else{
            //std::cout << dir << "is not a directory" << std::endl;
            return false;
        }
    }

    inline std::string get_filename(std::string img_fn, std::string detai_dir){
        std::string f_ext = ".png";
        std::string name = img_fn.substr(0, img_fn.length() - f_ext.length());
        std::string outname = detai_dir + name + ".txt";
        return outname;
    }

    inline int WriteDetectionInfo(std::vector<parknetDet> detlist, std::string fn){
        std::ofstream file(fn);
        float dummy_conf = 0.99;
        file << "width=3008\nheight=1680\n";
        for (const auto & det:detlist){
            float x_w = det.car.x2 - det.car.x1;
            float y_h = det.car.y2 - det.car.y1;
            /* Write car detection */
            file << "detection= " << det.car.conf << " " << det.car.x1 << " " << det.car.y1 << " ";
            file << det.car.x2 << " " << det.car.y2 << " " << x_w << " " << y_h << " ";
            /* Secondary car info */
            file << det.type << " " << det.make << " " << det.model << " " << det.color << " ";
            /* Reserved 3 cols */
            for (int i = 0; i < 3; i ++)
                file << " ";
            file << det.lpr_found << " " << det.shutter << " ";
            if(det.lpr_found){
                //file << det.lplate.x1 << " " << det.lplate.y1 << " " << det.lplate.x2 << " " << det.lplate.y2 << " ";
                file << det.lplate.y1 << " " << det.lplate.x1 << " " << det.lplate.y2 << " " << det.lplate.x2 << " " ;
                file << det.lplate.conf << " " << det.plText << " " << det.plText.length() << " ";
                for (const auto & ch:det.plText){
                    file << ch << " " << dummy_conf << " ";
                }
            }
            file << std::endl;
        }
        file.close();
        return 1;
    }
    
}


class Pipeline{
    private:
        int id;
        const char * work_dir;
        const std::string IMG_DIR = std::string(work_dir) + "images/";
        const std::string visualize_dir = std::string(work_dir) + "visualize/";
        const std::string detai_dir = std::string(work_dir) + "detai/";
        std::vector<std::string> images;
        bool visualize;
        const char * save_dir;
        std::thread thr;

        OnnxDetector *car_det = nullptr;
        OnnxDetector *lp_det = nullptr;
        LPRNetDetector *ocr_eng = nullptr;
        ImgReader *input_str = nullptr;
        StreamMuxer *muxer = nullptr;

        static std::string make_name(std::string txt, int id){
            return txt + "-" + std::to_string(id);
        }

        /**
         * @brief run image through pipeline and get detections list.
         * @param imgfn image file name
         * @param det pointer to detections vector
         * @return 1 if failed to load image, 0 if no errors
         */
        int pipeline_run(std::string imgfn, std::vector<parknetDet> &detl){

            std::string img_file = IMG_DIR + imgfn;
            cv::Mat img = ImgUtils::load_image(img_file.c_str());
            if (img.empty()){
                std::cout << "Image "<< imgfn << " not loaded! Skipping..\n";
                return 1;
            }
            cv::Mat prep_img = ImgUtils::preprocess_image(img);

            std::vector<bbox> cars = car_det->detect_preproc(prep_img);
            
            for (const auto & car:cars){
                cv::Mat car_img = ImgUtils::crop_image(prep_img, (int)car.x1,(int) car.y1,
                                                                (int)car.x2, (int)car.y2);
                if (car_img.cols == 0 || car_img.rows == 0){
                    continue;
                }
                if (visualize){
                    cv::rectangle(img, cv::Point(car.x1, car.y1), cv::Point(car.x2, car.y2), cv::Scalar(255, 0, 0), 3);
                    if (car.y1 > 15)
                        cv::putText(img, std::to_string(car.conf), cv::Point(car.x1, car.y1 - 5), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);
                    else
                        cv::putText(img, std::to_string(car.conf), cv::Point(car.x1, car.y2 - 5), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);
                    }
                std::vector<bbox> plates = lp_det->detect_preproc(car_img);
                bbox fplate, plate;
                std::string plate_text;
                bool pl_found = false;
                if (!plates.empty()){

                    fplate = detector::max_bbox(plates);
                    int x0 = (int)car.x1 + (int)fplate.x1;
                    int y0 = (int) car.y1 + (int)fplate.y1;
                    int x1 = (int)car.x1 + (int)fplate.x2;
                    int y1 = (int)car.y1 + (int)fplate.y2;
                    pl_found = true;
                    plate = {(float)x0, (float)x1, (float)y0, (float)y1, fplate.conf, fplate.cid};
                    cv::Mat lp_img = ImgUtils::crop_image(prep_img, x0, y0, x1, y1);
                    plate_text = ocr_eng->detect_preproc(lp_img);
                    if (visualize){
                        cv::rectangle(img, cv::Point(x0, y0), cv::Point(x1, y1), cv::Scalar(255, 255, 0), 3);
                        cv::putText(img, plate_text, cv::Point(x0 + (x1-x0)/2, y0 -10), cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(0, 255, 255), 2);
                    }
                }
                parknetDet det = {car, plate, plate_text};
                det.lpr_found = pl_found;
                
                detl.push_back(det);
            }
            if (visualize){
                std::string f_ext = ".png";
                std::string name = imgfn.substr(0, imgfn.length() - f_ext.length());
                std::string outname = visualize_dir + name + "-vis" + ".png";
                cv::imwrite(outname, img);
            }
            return 0;
        }

    public:
        /**
         * @brief Pipeline Construct.
         * 
         * @param id unique id for the pipeline and its elements.
         * @param work_dir path to working directory where images located
         * and detections are to be saved
         * @param car_mod_path path to model for car detection
         * @param lp_model_path path to model for license plate detections
         * @param ocr_model_path path to LPRNet Model for plate text extraction
         * @return Pipeline class object
         */
        Pipeline(int id, const char * work_dir)
            :id(id),
            work_dir(work_dir){}
        
        int connectInputStream(ImgReader &in){
            if(&in != nullptr){
                input_str = &in;
                return 1;
            }
            return 0;
        }
        std::string readInputStream(void);



        void connectModel(OnnxDetector &detector, model_t mod_type){
            if (mod_type == VEHICLE_MODEL){
                if (car_det != nullptr){
                    std::cout << "Vehicle model is already connected\n";
                    return;
            }
            car_det = &detector;
            }
            else if(mod_type == LP_MODEL){
                if (lp_det != nullptr){
                    std::cout << "License plate model is already connected\n";
                    return;
                }
            lp_det = &detector;
            }
        }

        void connectModel(LPRNetDetector &detector, model_t mod_type){
            if(mod_type == OCR_MODEL){
                if(ocr_eng != nullptr){
                    std::cout << "LP OCR model is already connected\n";
                    return;
                }
                ocr_eng = &detector;
            }
        }
        /**
        * @brief Runs image through the pipeline
        * @param imgfn - text string of image file
        * @param save_out - bool value whether to save output image with annotation
        * @returns None
        */
        void run(std::string imgfn, bool save_out){
             if (car_det == nullptr){
                std::cerr << "Car Detector Model is missing\n";
                return;
            }
            if(lp_det == nullptr){
                std::cerr << "LP Detector Model is missing\n";
                return;
            }
            if(ocr_eng == nullptr){
                std::cerr << "LPR Model is missing\n";
                return;
            }
            if (save_out)
                if (!wdet::dir_exists(visualize_dir)){
                    std::filesystem::create_directory(visualize_dir);
                }
                visualize = true;

            std::vector<parknetDet> dets;
            int ret = pipeline_run(imgfn, dets);
            if (!wdet::dir_exists(detai_dir)){
                std::filesystem::create_directory(detai_dir);
            }
            if (!ret){
                wdet::WriteDetectionInfo(dets, wdet::get_filename(imgfn, detai_dir));
                std::cout << "in image " << imgfn << " detected " << dets.size() << " objects\n";
                if (dets.size() != 0){
                    std::cout << "[";
                    for (int i=0; i < dets.size()-1; i++){
                        std::cout << dets[i].plText << ", ";
                    }
                    std::cout << dets[dets.size()-1].plText << "]\n";
                }
                }
            else{
                std::cout << "Failed to load img " << imgfn << std::endl; 
            }
        }
};

class ImgReader{
    std::vector <std::thread> app;
    std::vector<std::string> flist;
    bool reload = true;
    std::vector<Pipeline *> pipes;
    std::thread rel;
    std::thread tickth;
    uint64_t perf_fps;
    uint64_t frames = 0;
    const char * path;
    bool run = true;

    void fps_data(void){
        std::cout << "READER: Perf Data - FPS: " << frames << std::endl;
        perf_fps = frames;
        frames = 0;
    };
    void periodic_tick(int milliseconds, bool *run){

        while(*run){
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
            fps_data();
        }
    }

    void read_dir_content(const char * path, bool *run){
        while (*run){
            if (reload){
                std::vector<std::string> list = ImgUtils::get_files_in_directory(path);
                flist = ImgUtils::filter_img_list(list);
                reload = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    public:
    ImgReader(const char * img_dir): path(img_dir){
        rel = std::thread([this]() {
            read_dir_content(path, &run);
        });
        //rel.detach();
        tickth = std::thread([this] () {
            periodic_tick(1000, &run);
        });
    }

    void stop(void){
        run = false;
        if (rel.joinable()){
            std::cout << "Stopping Img Reader\n";
            rel.join();
        }
        if(tickth.joinable()){
            std::cout << "Stopping Img Reader tick\n";
            tickth.join();
        }
    }

    int link_pipe(Pipeline &p){
        if(&p != nullptr){
            pipes.push_back(&p);
            return 1;
        }
        return 0;
    }


    std::string provide_img_name(void){
        std::string ret = "";
        if (flist.empty()){
            std::cout << "READER: image list is empty\n";
            reload = true;
            return ret;
        }
        ret = flist.back();
        flist.pop_back();
        frames ++;
        return ret;
    }

    uint64_t get_perf_data(void){
        return perf_fps;
    } 
};

inline std::string Pipeline::readInputStream(void){
    std::string ret = "";
    if(input_str == nullptr){
        std::cout << "Input stream not connected\n";
        return ret;
    }
    ret = input_str->provide_img_name();
    return ret;
}

//void detections_task(bool *run, int nthreads, bool visualize);
//void detections_task2(bool *run, int nthreads, bool visualize, uint64_t *perf_fps);
int run_detector(bool *run, int index, ImgReader *inStream, bool visualize);
int build_engine(bool *run, StreamMuxer *src, int index, bool visualize);

class ImageDetector{
    std::thread app;
    uint64_t perf_fps = 0;
    bool running = false;
    bool run = true;

    void detection_task(bool *run, int nthreads, bool visualize){
        std::vector<std::thread> lthreads;
        ImgReader reader(IMAGES_DIR);
        bool th_run = true;
        for (int i = 0; i < nthreads; i++ ){
            lthreads.emplace_back(run_detector, &th_run, i, &reader, visualize);
        }
        running = true;
        while (*run == true){
            perf_fps = reader.get_perf_data();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        running = false;
        th_run = false;
        reader.stop();
        for (auto & th:lthreads){
            if (th.joinable()){
                th.join();
            }
        }
    };

    public:
        // Detector(bool *run, int nthreads, bool visualize)
        //     :app(detections_task2, run, nthreads, visualize, &perf_fps){};
        ImageDetector(int nthreads, bool visualize){
            app = std::thread([this, nthreads, visualize](){
                detection_task(&run, nthreads, visualize);
            });
        };

        void stop(void){
            run = false;
            if(app.joinable()){
                std::cout << "Thread joinable\n";
                app.join();
                std::cout << "Detector stopped succesfully\n";
            }
            else{
                std::cout << "Stop unsuccesful\n";
            }
        };

        uint64_t get_perf_data(void){return perf_fps;}
        bool is_running(void) {return running;}
};

/* Class for advanced vehicle detections*/
class Engine{
    private:
        int id;
        const char * work_dir;
        const std::string IMG_DIR = std::string(work_dir) + "images/";
        const std::string visualize_dir = std::string(work_dir) + "visualize/";
        const std::string detai_dir = std::string(work_dir) + "detai/";
        std::vector<std::string> images;
        bool visualize;
        const char * save_dir;
        std::thread thr;

        OnnxDetector *car_det = nullptr, car_e;
        OnnxDetector *lp_det = nullptr, lpd_e;
        LPRNetDetector *ocr_eng = nullptr, lpr_e;

        ImgReader *input_str = nullptr;
        StreamMuxer *muxer = nullptr;

        static std::string make_name(std::string txt, int id){
            return txt + "-" + std::to_string(id);
        }

        int pipeline_run(uchar *imgbuf, uint64_t nbytes, uint32_t img_uid, std::vector<parknetDet> &detl){
            std::cout << "Running Inference on "<< img_uid << std::endl;
            cv::Mat jpegBuf(1, nbytes, CV_8UC1, imgbuf);
            if (jpegBuf.empty()){
                std::cout << "JPEG not loaded! Skipping...\n";
                return 1;
            }
            cv::Mat img = cv::imdecode(jpegBuf, cv::IMREAD_COLOR);
            // cv::Mat img = ImgUtils::load_image(img_file.c_str());
            if (img.empty()){
                std::cout << "Image not loaded! Skipping..\n";
                return 1;
            }
            cv::Mat prep_img = ImgUtils::preprocess_image(img);

            std::vector<bbox> cars = car_det->detect_preproc(prep_img);
            
            for (const auto & car:cars){
                cv::Mat car_img = ImgUtils::crop_image(prep_img, (int)car.x1,(int) car.y1,
                                                                (int)car.x2, (int)car.y2);
                if (car_img.cols == 0 || car_img.rows == 0){
                    continue;
                }
                if (visualize){
                    cv::rectangle(img, cv::Point(car.x1, car.y1), cv::Point(car.x2, car.y2), cv::Scalar(255, 0, 0), 3);
                    if (car.y1 > 15)
                        cv::putText(img, std::to_string(car.conf), cv::Point(car.x1, car.y1 - 5), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);
                    else
                        cv::putText(img, std::to_string(car.conf), cv::Point(car.x1, car.y2 - 5), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);
                    }
                std::vector<bbox> plates = lp_det->detect_preproc(car_img);
                bbox fplate, plate;
                std::string plate_text;
                bool pl_found = false;
                if (!plates.empty()){

                    fplate = detector::max_bbox(plates);
                    int x0 = (int)car.x1 + (int)fplate.x1;
                    int y0 = (int) car.y1 + (int)fplate.y1;
                    int x1 = (int)car.x1 + (int)fplate.x2;
                    int y1 = (int)car.y1 + (int)fplate.y2;
                    pl_found = true;
                    plate = {(float)x0, (float)x1, (float)y0, (float)y1, fplate.conf, fplate.cid};
                    cv::Mat lp_img = ImgUtils::crop_image(prep_img, x0, y0, x1, y1);
                    plate_text = ocr_eng->detect_preproc(lp_img);
                    if (visualize){
                        cv::rectangle(img, cv::Point(x0, y0), cv::Point(x1, y1), cv::Scalar(255, 255, 0), 3);
                        cv::putText(img, plate_text, cv::Point(x0 + (x1-x0)/2, y0 -10), cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(0, 255, 255), 2);
                    }
                }
                parknetDet det = {car, plate, plate_text};
                det.lpr_found = pl_found;
                
                detl.push_back(det);
            }
            if (visualize){
                std::string f_ext = ".png";
                std::string name = std::to_string(img_uid);
                std::string outname = visualize_dir + name + "-vis" + ".png";
                std::cout << "Saving visualize image " << outname << std::endl;
                cv::imwrite(outname, img);
            }
            return 0;
        }
    
    public:
        /**
         * @brief Pipeline Construct.
         * @param id unique id for the pipeline and its elements.
         * @param work_dir path to working directory where images located
         * and detections are to be saved
         * @return Pipeline class object
         */
        Engine(int id, const char * work_dir)
            :id(id),
            work_dir(work_dir),
            car_e("car", VEHICLE_MODEL_PATH, VEHICLE_DET_CONFIDENCE_THRESHOLD),
            lpd_e("lpd", LPD_MODEL_PATH, LPLATE_DET_CONFIDENCE_THRESHOLD),
            lpr_e("lpr", LPR_MODEL_PATH)
            {
                connectModel(car_e, VEHICLE_MODEL);
                connectModel(lpd_e, LP_MODEL);
                connectModel(lpr_e, OCR_MODEL);
            }
        
        void connectModel(OnnxDetector &detector, model_t mod_type){
            if (mod_type == VEHICLE_MODEL){
                if (car_det != nullptr){
                    std::cout << "Vehicle model is already connected\n";
                    return;
            }
            car_det = &detector;
            }
            else if(mod_type == LP_MODEL){
                if (lp_det != nullptr){
                    std::cout << "License plate model is already connected\n";
                    return;
                }
            lp_det = &detector;
            }
        }

        void connectModel(LPRNetDetector &detector, model_t mod_type){
            if(mod_type == OCR_MODEL){
                if(ocr_eng != nullptr){
                    std::cout << "LP OCR model is already connected\n";
                    return;
                }
                ocr_eng = &detector;
            }
        }

        void connectSource(StreamMuxer *src){
            muxer = src;
        }

        void run(bool visualize){
            this->visualize = visualize;
            uchar *img = nullptr;
            uint64_t nbytes = 0;
            uint32_t id;
            if (car_det == nullptr){
                std::cerr << "Car Detector Model is missing\n";
                return;
            }
            if(lp_det == nullptr){
                std::cerr << "LP Detector Model is missing\n";
                return;
            }
            if(ocr_eng == nullptr){
                std::cerr << "LPR Model is missing\n";
                return;
            }

            if (visualize){
                if (!wdet::dir_exists(visualize_dir)){
                    std::filesystem::create_directory(visualize_dir);
                }
            }

            std::vector<parknetDet> dets;
            if(!muxer){
                std::cout << "Muxer not initialized\n";
                return;
            }


            id = muxer->pull_valid_frame(&img, &nbytes);
            if (id == 128){
                //std::cout << "No valid frame pulled\n";
                return;
            }


            if (nbytes == 0 || img == nullptr){
                //std::cout << "ID - " << id << "img size = 0\n"; 
                return;
            }
            std::cout << "ID - " << id << " Running Inference\n";
            int ret = pipeline_run(img, nbytes, id, dets);
            std::cout << "ret Value is: " << ret << std::endl;
            if (ret){
                std::cout << "ID = " << id << " Detection failed\n";
            }

            muxer->clear_frame_buffers(id);

            if (!wdet::dir_exists(detai_dir)){
                std::filesystem::create_directory(detai_dir);
            }
            ret = true; //skip the disk writing
            if (!ret){
                char fn[16]; 
                sprintf(fn, "%05d.txt", id);
                wdet::WriteDetectionInfo(dets, wdet::get_filename(fn, detai_dir));
                std::cout << "in image " << fn << " detected " << dets.size() << " objects\n";
                if (dets.size() != 0){
                    std::cout << "[";
                    for (int i=0; i < dets.size()-1; i++){
                        std::cout << dets[i].plText << ", ";
                    }
                    std::cout << dets[dets.size()-1].plText << "]\n";
                }
                }
            else{
                std::cout << "Failed to do inference" << std::endl; 
            }
        }
};


class Inference{
    std::vector <std::thread> lthreads;
    std::thread app;
    uint64_t perf_fps = 0;
    bool running = false;
    bool runth = true;
    bool visualize;
    StreamMuxer *muxer;
    Engine engine;


    void inference_engine(bool *run, int nthreads, bool visualize){
        std::vector<std::thread> lthreads;
        bool th_run = true;
        // for (int i = 0; i < nthreads; i++ ){
        //     lthreads.emplace_back(build_engine, &th_run, muxer, i, visualize);
        // }
        running = true;
        while (*run == true){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        running = false;
        th_run = false;
        for (auto & th:lthreads){
            if (th.joinable()){
                th.join();
            }
        }
    };
    public:
    Inference(int nthreads, bool visualize):
    engine(0, WORK_DIR),
    visualize(visualize)
    {
    
    }
    void stop(void){
        runth = false;
        if(app.joinable()){
            std::cout << "Thread joinable\n";
            app.join();
            std::cout << "Detector stopped succesfully\n";
        }
        else{
            std::cout << "Stop unsuccesful\n";
        }
    };

    void run(bool visualize, int id){
        engine.run(visualize);
    }

    int link_muxer(StreamMuxer *mux){
        muxer = mux;
        engine.connectSource(mux);
        return 1;
    }
};


class Detector{
    uint64_t perf_fps = 0;
    bool running = false;
    bool run = true;
    int nthreads = 0;
    bool visualize = false;
    std::vector <vstream> sources;
    std::vector <StreamCtrl> src_handles;
    std::vector <std::string> urls;
    std::vector <std::thread> task;

    StreamMuxer muxer;

    void detection_task(bool *run, int nthreads, bool visualize){
        init_camstream();
        src_handles.reserve(urls.size());
        sources.reserve(urls.size());
        for (int i=0; i<urls.size(); i++){
            std::cout << "Creating srcbin " << urls[i] << std::endl;
            StreamCtrl ctrl;

            src_handles.emplace_back(ctrl);
            sources.emplace_back(load_manual_stream(urls[i].c_str(), &src_handles[i]));
            muxer.link_stream(&sources[i], &src_handles[i]);
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // add streams with a delay
        }
        Inference inference (1, 1);
        inference.link_muxer(&muxer);
        int i = 0;
        while (true){
            //std::cout << "###############\n";
            //std::cout << "ITERATION - " << i << std::endl;
            inference.run(false, 0);
            //i++;
            //std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };

    public:
        Detector(int nthreads, bool visualize, std::vector<std::string> urls):
        nthreads(nthreads),
        visualize(visualize),
        urls(urls)
        {
            //detection_task(&run, nthreads, visualize);
        }

        uint64_t get_perf_data(void){return perf_fps;}
        bool is_running(void) {return running;}

        int get_frame(uchar **img, uint32_t cid,  uint64_t *max_size){
            int ret = muxer.pull_frame(img, cid, max_size);
            return ret;
        }

        void start(void){
            if (task.size() == 0){
                //task.emplace_back(std::thread(detection_task, &run, nthreads, visualize));
                task.emplace_back(&Detector::detection_task, this, &run, nthreads, visualize);
            }
        }

        int stop(void){
            run = false;
            if(task.size() == 1){
                if(task[0].joinable()){
                    task[0].join();
                    return 1;
                }
                return 0;
            }
            else{
                return 0;
            }
        }
};


#endif