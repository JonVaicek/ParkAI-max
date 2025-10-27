/**
 * @file    detector.cpp
 * @brief   Application that detects cars and its attributes from the images
 * @author  Jonas Vaicekauskas
 * @date    2025-07-03
 * @details
 */

#include "detector.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>
#include <mutex>





int run_detector(bool *run, int index, ImgReader *inStream, bool visualize){
    std::string ce_name = "car-" + std::to_string(index);
    OnnxDetector car_engine(ce_name.c_str(), VEHICLE_MODEL_PATH, 1, VEHICLE_DET_CONFIDENCE_THRESHOLD);
    std::string le_name = "lp-" + std::to_string(index);
    OnnxDetector lp_engine(le_name.c_str(), LPD_MODEL_PATH, 1, LPLATE_DET_CONFIDENCE_THRESHOLD);
    std::string ocr_name = "ocr-" + std::to_string(index);
    LPRNetDetector ocr_engine(ocr_name.c_str(), LPR_MODEL_PATH);

    Pipeline pipeline(index, WORK_DIR);
    pipeline.connectModel(car_engine, VEHICLE_MODEL);
    pipeline.connectModel(lp_engine, LP_MODEL);
    pipeline.connectModel(ocr_engine, OCR_MODEL);
    pipeline.connectInputStream(*inStream);
    std::cout << "pipeline initialized\n";

    while (*run == true){
        std::string im = pipeline.readInputStream();
        if(im != ""){
            //std::cout << "Running on image " << im << std::endl;
            pipeline.run(im, visualize);
        }
        else{
            //std::cout << "Empty Input" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 1;
}

void detections_task2(bool *run, int nthreads, bool visualize, uint64_t *perf_fps){
    std::vector<std::thread> lthreads;
    ImgReader reader(IMAGES_DIR);
    bool th_run = true;
    for (int i = 0; i < nthreads; i++ ){
        lthreads.emplace_back(run_detector, &th_run, i, &reader, visualize);
    }
    while (*run == true){
        *perf_fps = reader.get_perf_data();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    th_run = false;
    reader.stop();
    for (auto & th:lthreads){
        if (th.joinable()){
            th.join();
        }
    }
}


void detections_task(bool *run, int nthreads, bool visualize){
    std::vector<std::thread> lthreads;
    ImgReader reader(IMAGES_DIR);
    for (int i = 0; i < nthreads; i++ ){
        lthreads.emplace_back(run_detector, run, i, &reader, visualize);
    }

    for(auto & th:lthreads){
        th.join();
    }
}


/* Helpers Begin */

float IoU(const bbox& a, const bbox& b) {
    float inter_x1 = std::max(a.x1, b.x1);
    float inter_y1 = std::max(a.y1, b.y1);
    float inter_x2 = std::min(a.x2, b.x2);
    float inter_y2 = std::min(a.y2, b.y2);
    float inter_area = std::max(0.0f, inter_x2 - inter_x1) * std::max(0.0f, inter_y2 - inter_y1);
    float area_a = (a.x2 - a.x1) * (a.y2 - a.y1);
    float area_b = (b.x2 - b.x1) * (b.y2 - b.y1);
    float union_area = area_a + area_b - inter_area;
    return inter_area / (union_area + 1e-6f);
}

std::vector<bbox> non_max_suppression(std::vector<bbox>& boxes, float iou_thresh = 0.45f){
    std::vector<bbox> result;
    std::sort(boxes.begin(), boxes.end(),[](const bbox& a, const bbox& b) { return a.conf > b.conf; });

    std::vector<bool> removed(boxes.size(), false);

    for (size_t i = 0; i < boxes.size(); ++i) {
        if (removed[i]) continue;
        result.push_back(boxes[i]);
        for (size_t j = i + 1; j < boxes.size(); ++j) {
            if (removed[j]) continue;
            if (boxes[i].cid == boxes[j].cid && IoU(boxes[i], boxes[j]) > iou_thresh) {
                removed[j] = true;
            }
        }
    }
    return result;
}

int draw_boxes(cv::Mat &img, bbox car){
    cv::rectangle(img, cv::Point(car.x1, car.y1), cv::Point(car.x2, car.y2), cv::Scalar(255, 0, 0), 3);
    if (car.y1 > 15)
        cv::putText(img, std::to_string(car.conf), cv::Point(car.x1, car.y1 - 5), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);
    else
        cv::putText(img, std::to_string(car.conf), cv::Point(car.x1, car.y2 - 5), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);
    return 1;
}

int save_input_image(uint32_t id, cv::Mat &img, std::string dir){
    const char *f_ext = ".png";
    char fn[16]; 
    sprintf(fn, "%05d%s", id, f_ext);
    std::string outpath = dir + fn;
    std::cout << "Saving input image " << fn << std::endl;
    //std::cout << "Image size: " << img.size() << std::endl;
    cv::imwrite(outpath, img);
    return 1;
}

int save_visualize_img(uint32_t id, cv::Mat &img, std::string dir){
    std::string f_ext = ".png";
    std::string name = std::to_string(id);
    std::string outname = dir + name + "-vis" + ".png";
    std::cout << "Saving visualize image " << outname << std::endl;
    cv::imwrite(outname, img);
    return 1;
}

int print_batch_detections(std::vector<std::vector<bbox>> &batch_dets){
    int size = batch_dets.size();
    std::cout << "Batch Detections Size: " << size << std::endl;
    for (int b=0; b<size; b++){
        std::cout << "batch item-"<<b<<" detections:\n";
        std::cout << "[";
        for (const auto & d:batch_dets[b]){
            std::cout << "Confidence: " <<d.conf<< " x1y1: (" << d.x1 << "," << d.y1<<"),";
        }
        std::cout << "]\n";
    }
    return 1;
}

/* Helpers End*/

/* OnnxRTDetector Methods Begin */

#ifdef WIN32
OnnxRTDetector::OnnxRTDetector(const char * name, const wchar_t * model_path, float threshold, int batchsize)
#elif __linux__
OnnxRTDetector::OnnxRTDetector(const char * name, const char * model_path, float threshold, int batchsize)
#endif
        : env(ORT_LOGGING_LEVEL_WARNING, name), 
        session_options(create_session_options()),
        session(env, model_path, session_options), 
        threshold(threshold),
        batch_size(batchsize),
        input_shape{batch_size, IN_CH, INPUT_H, INPUT_W}
{
        init();
        std::cout << "Loading model " << model_path << std::endl;
        std::cout << "Model threshold set to - " << (float) threshold << std::endl;
}


void OnnxRTDetector::init(void){
    auto input_names_p = session.GetInputNameAllocated(0, Ort::AllocatorWithDefaultOptions());
    auto output_names_p = session.GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions());

    input_names = input_names_p.get();
    output_names = output_names_p.get();

    input_names_raw = {input_names.c_str()};
    output_names_raw = {output_names.c_str()};

    // After session creation, check providers
    std::vector<std::string> providers =  Ort::GetAvailableProviders();
    std::cout << "Available providers: ";
    for (const auto& provider : providers) {
        std::cout << provider << " ";
    }
    std::cout << std::endl;
    try {
        // Input information
        size_t num_inputs = session.GetInputCount();
        std::cout << "=== Model Input Information ===" << std::endl;
        std::cout << "Number of inputs: " << num_inputs << std::endl;
        
        for (size_t i = 0; i < num_inputs; ++i) {
            auto input_name = session.GetInputNameAllocated(i, Ort::AllocatorWithDefaultOptions());
            auto input_type_info = session.GetInputTypeInfo(i);
            auto tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
            auto shape = tensor_info.GetShape();
            auto type = tensor_info.GetElementType();
            
            std::cout << "Input " << i << ":" << std::endl;
            std::cout << "  Name: " << input_name.get() << std::endl;
            std::cout << "  Shape: [";
            for (size_t j = 0; j < shape.size(); ++j) {
                std::cout << shape[j];
                if (j != shape.size() - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;
            std::cout << "  Type: " << type << std::endl;
        }
        
        // Output information
        size_t num_outputs = session.GetOutputCount();
        std::cout << "\n=== Model Output Information ===" << std::endl;
        std::cout << "Number of outputs: " << num_outputs << std::endl;
        
        for (size_t i = 0; i < num_outputs; ++i) {
            auto output_name = session.GetOutputNameAllocated(i, Ort::AllocatorWithDefaultOptions());
            auto output_type_info = session.GetOutputTypeInfo(i);
            auto tensor_info = output_type_info.GetTensorTypeAndShapeInfo();
            auto shape = tensor_info.GetShape();
            auto type = tensor_info.GetElementType();
            
            std::cout << "Output " << i << ":" << std::endl;
            std::cout << "  Name: " << output_name.get() << std::endl;
            std::cout << "  Shape: [";
            for (size_t j = 0; j < shape.size(); ++j) {
                std::cout << shape[j];
                if (j != shape.size() - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;
            std::cout << "  Type: " << type << std::endl;
        }
        
        std::cout << "\n=== Execution Providers ===" << std::endl;
        std::cout << "Session providers: ";
        for (const auto& provider : providers) {
            std::cout << provider << " ";
        }
        std::cout << std::endl;
        
        // Available providers
        std::vector<std::string> available_providers = Ort::GetAvailableProviders();
        std::cout << "Available providers: ";
        for (const auto& provider : available_providers) {
            std::cout << provider << " ";
        }
        std::cout << std::endl;
        
    } catch (const Ort::Exception& e) {
        std::cout << "Error getting model info: " << e.what() << std::endl;
    }
}

Ort::SessionOptions OnnxRTDetector::create_session_options() {
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


Ort::Value OnnxRTDetector::load_input_tensor(std::vector<cv::Mat> img_batch){
    input_tensor_values.clear(); // Clear previous data
    
    // For each image in batch
    for (const auto& src_img : img_batch) {
        // Convert HWC to CHW for this image
        for (int c = 0; c < 3; ++c) {
            for (int y = 0; y < INPUT_H; ++y) {
                for (int x = 0; x < INPUT_W; ++x) {
                    input_tensor_values.push_back(src_img.at<cv::Vec3f>(y, x)[c]);
                }
            }
        }
    }
    // Update input shape for current batch size
    std::array<int64_t, 4> current_input_shape{(int64_t)img_batch.size(), IN_CH, INPUT_H, INPUT_W};
    
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_tensor_values.data(), input_tensor_values.size(), 
        current_input_shape.data(), current_input_shape.size());
    return input_tensor;
}

void OnnxRTDetector::run(Ort::Value &input_tensor){
    output_tensors = session.Run(Ort::RunOptions{nullptr}, input_names_raw.data(),
                &input_tensor, 1, output_names_raw.data(), 1);
}


std::vector<std::vector<bbox>> OnnxRTDetector::post_process(void){
    std::vector<std::vector<bbox>> ret;

    auto shape = output_tensors.front().GetTensorTypeAndShapeInfo().GetShape(); // [2, 5, 8400]
    const int batch = shape[0];
    const int channels = shape[1];
    const int num_preds = shape[2];
    float *transposed;
    transposed = (float *)malloc(batch*channels*num_preds*sizeof(float));
    float* output = output_tensors.front().GetTensorMutableData<float>();

    bool has_classes = (channels > 5);
    /* Transpose Array to [2 8400 5]*/
    int p = 0;
    for (int b = 0; b< batch; b++){
        for (int i = 0; i < num_preds ; i++){
            for (int c = 0; c < channels; c++){
                int index =  b * channels * num_preds + c * num_preds + i;
                memcpy(transposed+p, &output[index], sizeof(float));
                p++;
            }
        }
    }

    //Read Detections
    for(int b=0; b< batch; b++){
        std::vector<sdet> boxes;
        std::vector <bbox> bboxes;
        int img_w = orig_imgs[b].cols;
        int img_h = orig_imgs[b].rows;
        float sw = (float)img_w/YOLO_INPUT_W;
        float sh = (float)img_h/YOLO_INPUT_H;

        for (int p=0; p < num_preds; p++){
            sdet box;
            for (int i = 0; i < channels; i++){
                switch (i){
                    case 0:
                        box.xc = (float )*(transposed + b*channels*num_preds + p*channels + i);
                        break;
                    case 1:
                        box.yc = (float )*(transposed + b*channels*num_preds + p*channels + i);
                        break;
                    case 2:
                        box.w = (float )*(transposed + b*channels*num_preds + p*channels + i);
                        break;
                    case 3:
                        box.h = (float )*(transposed + b*channels*num_preds + p*channels + i);
                        break;
                    case 4:
                        box.conf = (float )*(transposed + b*channels*num_preds + p*channels + i);
                        break;
                }
            }
            if (box.conf > threshold)
                boxes.push_back(box);
        }
        for (const auto & box:boxes){
            bbox bb;
            bb.x1 = box.xc * sw - (box.w * sw / 2); 
            bb.y1 = box.yc * sh - (box.h * sh / 2);
            bb.x2 = box.w * sw + bb.x1;
            bb.y2 = box.h * sh + bb.y1;
            bb.conf = box.conf;
            bb.cid = 0;
            bboxes.push_back(bb);
        }
        //std::cout << "Cars pre NMS: " << bboxes.size() << std::endl;
        std::vector<bbox> postNMS = non_max_suppression(bboxes);
        //std::cout << "Cars Found: " << postNMS.size() << std::endl;
        ret.push_back(postNMS);
    }
    free(transposed);
    return ret;
}

void OnnxRTDetector::clear_tensors(void){
    input_tensor_values.clear();
    output_tensors.clear();
}


std::vector<std::vector<bbox>> OnnxRTDetector::detect(std::vector<cv::Mat> im_batch, bool visualize){
    orig_imgs = im_batch;
    std::vector<std::vector<bbox>> dets;
    std::vector <cv::Mat> ri_batch;
    for (const auto & im:im_batch){
        cv::Mat fit_img = ImgUtils::resize_image(im, INPUT_W, INPUT_H);
        ri_batch.push_back(fit_img);
    }
    //std::cout <<"batch ready. Size: " << ri_batch.size() << std::endl;
    Ort::Value input_tensor = load_input_tensor(ri_batch);
    run(input_tensor);
    dets = post_process();
    clear_tensors();
    return dets;
}

/* OnnxRTDetector Methods End*/


/* Engine Class Methods Begin */

int Engine::pull_batch(std::vector <ImgData> &input_batch, uint32_t timeout){

    if(!muxer){
        std::cout << "Muxer not initialized\n";
        return 0;
    }
    int ret = muxer->pull_frames_batch(input_batch, batch_size);
    if (!ret)
        return 0;
    
    if(input_batch.size() == batch_size)
        return 1;
    else
        return 0;
}

int Engine::process(std::vector <ImgData> &img_batch, std::vector<std::vector<parknetDet>> &detl){

    std::vector <cv::Mat> input_batch;
    std::vector <cv::Mat> org_images;
    for (const auto & im:img_batch){
        cv::Mat imbuf(1, im.nbytes, CV_8UC1, im.data);

        cv::Mat img = cv::imdecode(imbuf, cv::IMREAD_COLOR);
        org_images.push_back(img);
        cv::Mat ims = img;
        if (img.empty()){
            std::cout << im.id <<" - Image not loaded! Skipping..\n";
            return 0;
        }
        cv::Mat prep_img = ImgUtils::preprocess_image(img);
        if(prep_img.empty()){
            return 0;
        }
        input_batch.push_back(prep_img);
    }
    std::vector<std::vector<bbox>> batch_dets =  car_det->detect(input_batch);
    detl.resize(batch_size);

    print_batch_detections(batch_dets);

    for (int b=0; b < batch_size; b++){
        //std::cout << "b = " << b << std::endl;
        for(const auto & car : batch_dets[b]){
            //std::cout << "Processing car\n";
            std::string plate_text;
            bool pl_found = false;
            bbox fplate, plate;
            if(DETECT_LPD){
                cv::Mat car_img = ImgUtils::crop_image(input_batch[b], (int)car.x1,(int) car.y1,
                                                            (int)car.x2, (int)car.y2);
                if (car_img.cols == 0 || car_img.rows == 0){
                    continue;
                }
                if (visualize){
                    draw_boxes(org_images[b], car);
                }
                //std::cout << "Looking for license plates\n";
                std::vector<bbox> plates = lp_det->detect_preproc(car_img);
                

                if (!plates.empty()){
                    fplate = detector::max_bbox(plates);
                    int x0 = (int)car.x1 + (int)fplate.x1;
                    int y0 = (int) car.y1 + (int)fplate.y1;
                    int x1 = (int)car.x1 + (int)fplate.x2;
                    int y1 = (int)car.y1 + (int)fplate.y2;
                    pl_found = true;
                    plate = {(float)x0, (float)x1, (float)y0, (float)y1, fplate.conf, fplate.cid};
                    cv::Mat lp_img = ImgUtils::crop_image(input_batch[b], x0, y0, x1, y1);
                    //std::cout << "Reading License Plate\n";
                    plate_text = ocr_eng->detect_preproc(lp_img);
                    //std::cout << "Plate Read\n";
                    if (visualize){
                        cv::rectangle(org_images[b], cv::Point(x0, y0), cv::Point(x1, y1), cv::Scalar(255, 255, 0), 3);
                        cv::putText(org_images[b], plate_text, cv::Point(x0 + (x1-x0)/2, y0 -10), cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(0, 255, 255), 2);
                    }
                }
            }
            parknetDet det = {car, plate, plate_text};
            det.lpr_found = pl_found;
            detl[b].push_back(det);
        }
        //std::cout << "Detections done\n";
        if (SAVE_INPUT_IMAGES){
            //save_input_image(img_batch[b].id, img_batch[b].data, IMG_DIR);
            save_input_image(img_batch[b].index, org_images[b], IMG_DIR);
        }
        if (visualize){
            save_visualize_img(img_batch[b].index, org_images[b], visualize_dir);
        }
    }
    return 1;
}


void Engine::runn(bool visualize){

    this->visualize = visualize;
    std::vector <ImgData> img_batch;

    std::vector<std::vector<parknetDet>> b_dets;

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

    int ret = pull_batch(img_batch, 50);

    if(!ret){
        return;
    }
    std::cout << "Pulled Batch Size : "<<img_batch.size() <<  std::endl;
    for(const auto & b:img_batch){
        std::cout << "id - " << b.id << ", img_size: " << b.nbytes << std::endl;
    }
    ret = process(img_batch, b_dets);

    for (int b=0; b< batch_size; b++){
        muxer->clear_frame_buffers(img_batch[b].id);
    }

    if (ret){
        for (int b=0; b<batch_size; b++){
            std::vector<parknetDet> dets = b_dets[b];
            char fn[16];
            sprintf(fn, "%05d.txt", img_batch[b].id);
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
    }
    else{
        std::cout << "Failed to do inference" << std::endl; 
    }
}

int Engine::pipeline_run(uchar *imgbuf, uint64_t nbytes, uint32_t img_uid, std::vector<parknetDet> &detl){
            //std::cout << "Running Inference on "<< img_uid << std::endl;
            cv::Mat jpegBuf(1, nbytes, CV_8UC1, imgbuf);
            if (jpegBuf.empty()){
                std::cout << "JPEG not loaded! Skipping...\n";
                return 0;
            }
            cv::Mat img = cv::imdecode(jpegBuf, cv::IMREAD_COLOR);
            cv::Mat im = img;
            // cv::Mat img = ImgUtils::load_image(img_file.c_str());
            if (img.empty()){
                std::cout << "Image not loaded! Skipping..\n";
                return 0;
            }
            cv::Mat prep_img = ImgUtils::preprocess_image(img);


            if (SAVE_INPUT_IMAGES){
                const char *f_ext = ".png";
                char fn[16]; 
                sprintf(fn, "%05d%s", img_uid, f_ext);
                std::string outpath = IMG_DIR + fn;
                std::cout << "Saving input image " << fn << std::endl;
                cv::imwrite(outpath, im);
            }
            if (visualize){
                std::string f_ext = ".png";
                std::string name = std::to_string(img_uid);
                std::string outname = visualize_dir + name + "-vis" + ".png";
                std::cout << "Saving visualize image " << outname << std::endl;
                cv::imwrite(outname, img);
            }
            return 1;
        }


/* Engine Class Methods End */




/* Detector Methods Begin */
float Detector::get_fps(void){
    return muxer.get_fps();
}

bool Detector::is_running(void){
    return true;
};

/* Detector Methods End */