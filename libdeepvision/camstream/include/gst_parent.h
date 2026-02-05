#ifndef GST_PARENT_H
#define GST_PARENT_H

#include "camstream.h"
#include "gst_worker.h"

#include <csignal>
#include <cstring>


#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>
#include <time.h>

#define STREAM_IS_OFF_AFTER 180 /* seconds */

int recv_fd(int sock);
uint64_t signal_parser(uint64_t val);



class GstChildWorker{
private:
    int id;
    int sv_[2];
    int evfd_;
    pid_t pid_;
    int shmfd_;
    void* shm_;
    size_t shm_bytes_;
    DataHeader* hdr_ = nullptr;
    uint64_t n_read=0;
    std::string fn;
    const char *worker_path;
    const char *rtsp_url;
    bool shm_ready = false;
    bool closed_ = false;
    time_t f_ts_ = 0;


public:

    //GstChildWorker(int id, const char *workerpath):
    GstChildWorker(int id, const char *workerpath, const char *rtsp_url):
    pid_(-1), id(id), worker_path(workerpath), rtsp_url(rtsp_url)
    {
        uint32_t ret = init();
        if (ret){
            std::cout << " [parent] failed to initialize\n";
        }
    }

    uint32_t init(void){
        closed_ = false;
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv_) != 0) {
            printf(" [child-%d] socket pair error\n", id);
            return 0;
        }

        // create eventfd
        evfd_ = eventfd(0, 0);
        if (evfd_ < 0) {
            printf(" [child-%d] eventfd error\n", id);
            return 0;
        }
        std::cout << "[parent] evfd = " << evfd_ << std::endl;

        pid_ = fork();

        if (pid_ < 0) {
            std::cerr << "[parent] fork failed: " << std::strerror(errno) << "\n";
            return 0;
        }
        if (pid_ == 0) {
            close(sv_[0]);// parent end
            if (sv_[1] != 3) {
                dup2(sv_[1], 3);// move to fd 3
                close(sv_[1]);
            }

            // map eventfd to FD 4
            if (evfd_ != 4) {
                dup2(evfd_, 4);
                close(evfd_);
            }
            std::cout << "[parent] evfd = " << evfd_ << std::endl;
            // child branch: replace process image
            execl(worker_path, worker_path, rtsp_url, (char*)nullptr);
            // only reached if exec fails
            std::cerr << "[parent->child] exec failed: " << std::strerror(errno) << "\n";
            return 0;
        }
        close(sv_[1]);
        fn = std::string("image-") + std::to_string(id) + std::string(".jpeg");
        time(&f_ts_);
        return 1;
    }

    int init_shm(void){
        shmfd_ = recv_fd(sv_[0]);
        if (shmfd_ < 0) {
            //std::cerr << "Failed to receive shmfd\n";
            printf(" [child-%d] shared mem error\n", id);
            //throw std::runtime_error("recv_fd");
            return 0;
        }

        struct stat st{};
        fstat(shmfd_, &st);
        shm_bytes_ = st.st_size;
        shm_ = mmap(nullptr, shm_bytes_, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd_, 0);
        if (shm_ == MAP_FAILED) {
            //perror("mmap");
            //throw std::runtime_error("mmap");
            printf(" [child-%d] memorymap error\n", id);
            return 0;
        }
        hdr_ = reinterpret_cast<DataHeader*>(shm_);
        std::cout << "[parent] mapped shared buffer, size [" << shm_bytes_ << "]\n";
        return 1;
    }

    ~GstChildWorker() {
        deinit();
    }

    uint32_t deinit(void){
        if (closed_) return 0;
        // stop child first
        terminate_and_wait();

        // release resources safely
        if (shm_ != MAP_FAILED) {
            munmap(shm_, shm_bytes_);
            shm_ = MAP_FAILED;
        }
        hdr_ = nullptr;
        shm_bytes_ = 0;

        if (shmfd_ >= 0) {
            close(shmfd_);
            shmfd_ = -1;
        }
        if (evfd_ >= 0) {
            close(evfd_);
            evfd_ = -1;
        }
        if (sv_[0] >= 0) {
            close(sv_[0]);
            sv_[0] = -1;
        }
        if (sv_[1] >= 0) {
            close(sv_[1]);
            sv_[1] = -1;
        }

        shm_ = nullptr;
        pid_ = -1;
        closed_ = true;
        return 1;

    }

    uint32_t reset(void){
        std::cout << " RESETING STREAM " << this->rtsp_url << std::endl;
        f_ts_ = 0;
        deinit();
        init();
        return 1;
    }

    bool wait_for_signal(uint64_t &out_val) {
        struct pollfd pfd{};
        pfd.fd = evfd_;
        pfd.events = POLLIN;
        int r = poll(&pfd, 1, -1);
        if (r <= 0) return false;
        if (read(evfd_, &out_val, sizeof(out_val)) != sizeof(out_val)) return false;
        return true;
    }

    bool check_for_signal(uint64_t &out_val){
        struct pollfd pfd{};
        pfd.fd = evfd_;
        pfd.events = POLLIN;
        int r = poll(&pfd, 1, 0);
        if (r <= 0) return false;
        if (read(evfd_, &out_val, sizeof(out_val)) != sizeof(out_val)) return false;
        return true;
    }

    void terminate_and_wait() {
        if (pid_ <= 0) return;
        if (pid_ > 0) kill(pid_, SIGKILL);
        int status = 0;
        waitpid(pid_, &status, 0);
    }

    int check_for_image(void){
        if (closed_)
            return 0;
        if (data_ptr()==nullptr){
            return 0;
        }
        if (hdr_ == nullptr){
            std::cout << " [parent] header pointer is null\n";
        }
        std::cout << " [parent] attempting to read data\n";
        if (hdr_->state == SHM_READY) {
            std::cout << " [parent] state - SHM_READY\n";
            uint8_t* data = (uint8_t *)shm_ + sizeof(DataHeader);
            std::cout << " [parent] pointer acquired\n";
            std::vector<unsigned char> jpg = encode_jpeg(data, hdr_->w, hdr_->h);
            n_read++;
            save_jpeg_to_file(jpg, fn);
            // do other processing here with the memory;
            hdr_->state = SHM_EMPTY;
            return 1;
        }
        else{
            std::cout << "[parent] buffer not ready\n";
            return 0;
        }
    }

    int read_frame(unsigned char **img_buf, uint64_t *size){
        /* first check if data exists*/
        DataHeader *d = header();
        if (d == nullptr){
            return 0;
        }
        if( closed_ || d->state != SHM_READY){
            return 0;
        }
        /* allocate destination buffer if does not exist*/
        if (*img_buf == nullptr){
            std::cout << " [parent] allocating memory\n";
            *img_buf = (unsigned char*)malloc(d->nbytes);
        }
        else{
            if (*size != d->nbytes){
                std::cout << "Reallocating provided buffer\n";
                free(*img_buf);
                *img_buf = (unsigned char*)malloc(d->nbytes);
            }
        }
        *size = d->nbytes;
        memcpy(*img_buf, data_ptr(), d->nbytes);
        /* set buffer to read */
        d->state = SHM_EMPTY;

        time(&f_ts_);
        return 1;
    }

    /* @brief class method to pull frame. User must free the buffer after the frame is pulled*/
    int pull_frame(unsigned char **img_buf, uint64_t *size){
        uint64_t sig=0;
        if(this->check_for_signal(sig)){
            if (signal_parser(sig) == EVT_PIPELINE_EXIT){
                //this->reset();
                return 0;
            }
            if (signal_parser(sig) == EVT_MMSH_COMPLETE){
                std::cout << " initializing shm memory\n";
                shm_ready = init_shm();
            }
        }
        if (!shm_ready){
            return 0;
        }
        if(is_offline()){
            //this->reset();
            return 0;
        }

        /* first check if data exists*/
        DataHeader *d = header();
        if (d == nullptr){
            return 0;
        }
        if( closed_ || d->state != SHM_READY){
            return 0;
        }
        /* allocate destination buffer if does not exist*/
        if (*img_buf == nullptr){
            std::cout << " [parent] allocating memory\n";
            *img_buf = (unsigned char*)malloc(d->nbytes);
        }
        else{
            if (*size != d->nbytes){
                std::cout << "Reallocating provided buffer\n";
                free(*img_buf);
                *img_buf = (unsigned char*)malloc(d->nbytes);
            }
        }
        *size = d->nbytes;
        memcpy(*img_buf, data_ptr(), d->nbytes);
        /* set buffer to read */
        d->state = SHM_EMPTY;

        time(&f_ts_);
        return 1;
    }

    time_t get_timediff(void){
        time_t now, dt;
        time(&now);
        dt = now - f_ts_;
        return dt;
    }
    uint32_t is_offline(){
        if (get_timediff() > STREAM_IS_OFF_AFTER){
            return 1;
        }
        else{
            return 0;
        }
    }
    void handle_event(uint64_t evt) {
        switch (evt) {
            case EVT_MMSH_COMPLETE:
                shm_ready = init_shm();
                break;

            // case EVT_FRAME_WAITING:
            //     pull_frame(...);
            //     break;

            case EVT_PIPELINE_EXIT:
                reset();
                break;
        }
    }
    
    DataHeader* header() const { return hdr_; }
    void* shm_ptr() const { return shm_; }
    uint8_t* data_ptr() const {return (uint8_t*)shm_ + sizeof(DataHeader);}
    size_t shm_size() const { return shm_bytes_; }
    pid_t pid() const { return pid_; }
    int get_id() const { return id; }
    time_t get_ts() const {return f_ts_;}
    int get_evfd() const {return evfd_;}


};


#endif