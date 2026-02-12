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

#define STREAM_IS_OFF_AFTER 20 /* seconds */
#define MINUTES_5_IN_SECONDS 300

int recv_fd(int sock);
uint64_t signal_parser(uint64_t val);



class GstChildWorker{
private:
    int id;
    
    
    pid_t pid_;
    void* shm_ = MAP_FAILED;
    size_t shm_bytes_ = -1;
    DataHeader* hdr_ = nullptr;
    uint64_t n_read=0;
    std::string fn;
    const char *worker_path;
    bool shm_ready = false;
    bool closed_ = false;
    
    bool frame_waiting = false;
    time_t f_ts_ = 0;
    time_t closed_ts = 0;
    bool epoll_registered = false;


public:
    int shmfd_;int sv_[2] = {-1, -1};
    int evfd_;
    const char *rtsp_url;
    bool unreg_ = false;
    bool deinit_ = false;
    bool init_complete_ = false;
    bool killed_ = false;

    //GstChildWorker(int id, const char *workerpath):
    GstChildWorker(int id, const char *workerpath, const char *rtsp_url):
    pid_(-1), id(id), worker_path(workerpath), rtsp_url(rtsp_url),
    shmfd_(-1), evfd_(-1)
    {
        uint32_t ret = init();
        if (!ret){
            std::cout << " [parent] failed to initialize\n";
        }
    }

    uint32_t init(void){
        killed_ = false;
        unreg_ = false;
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv_) != 0) {
            printf(" [child-%d] socket pair error\n", id);
            return 0;
        }
        printf("[src-%s] socket pair created sv_={ %d, %d }\n", rtsp_url, sv_[0], sv_[1]);

        // create eventfd
        evfd_ = eventfd(0, 0);
        if (evfd_ < 0) {
            printf(" [child-%d] eventfd error\n", id);
            return 0;
        }
        printf("[src-%s] evfd created evfd={ %d }\n", rtsp_url, evfd_);

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
        closed_ = false;
        epoll_registered = false;
        closed_ts = 0;
        f_ts_= 0; // start fresh
        init_complete_ = true;
        deinit_ = false;
        printf("[src-%s] init complete\n", rtsp_url);
        //time(&f_ts_);
        return 1;
    }

    int init_shm(void){
        std::cout << "sv_[0] = " << sv_[0] << std::endl;
        std::cout << "closed = " << closed_ << killed_ <<std::endl;
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

    uint32_t release_mem(void){
        shm_ready = false;
        if (shm_ != MAP_FAILED) {
            printf("[%s] - memory unmaped\n", rtsp_url);
            munmap(shm_, shm_bytes_);
            shm_ = MAP_FAILED;
        }
        hdr_ = nullptr;
        shm_bytes_ = 0;
        
        return 1;
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

    uint32_t kill_children(void){
        std::cout << "[parent] Killing child with pid= "<< pid_ << std::endl;
        if (pid_ > 0) {
            kill(pid_, SIGKILL);
            int status;
            pid_t result = waitpid(pid_, &status, WNOHANG);
            if (result == 0) {
                return 0; // still running, must check later
            }
            else if (result == pid_){
                // child exited, handle status
                pid_ = -1;
                killed_ = true;
                return 1;
            } else {
                // error
                std::cout << "Error reaping children\n";
                return 0;
            }
        }
        return 0;
    }

    uint32_t soft_deinit(){
        // release resources safely
        if (shmfd_ >= 0) {
            close(shmfd_);
            shmfd_ = -1;
        }

        std::cout << "[parent] - closing evfd " << evfd_ << std::endl;
        if (evfd_ >= 0) {
            close(evfd_);
            evfd_ = -1;
        }

        std::cout << "[parent] - closing sv_={"<<sv_[0] <<", " <<sv_[1] << "}\n";
        if (sv_[0] >= 0) {
            close(sv_[0]);
            sv_[0] = -1;
        }
        if (sv_[1] >= 0) {
            close(sv_[1]);
            sv_[1] = -1;
        }

        if (shm_ != MAP_FAILED) {
            munmap(shm_, shm_bytes_);
            shm_ = MAP_FAILED;
        }
        hdr_ = nullptr;
        shm_bytes_ = 0;
        shm_ = nullptr;

        closed_ = true;
        time(&closed_ts);
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
        //d->state = SHM_EMPTY;

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

    time_t is_past_timeout(void){
        if (closed_ts == 0){
            return false;
        }
        time_t now;
        time(&now);

        if (now - closed_ts > STREAM_IS_OFF_AFTER){
            return true;
        }
        return false;
    }

    void handle_event(uint64_t evt) {
        switch (evt) {
            case EVT_MMSH_COMPLETE:
            std::cout << "Init SHM Blocks\n";
                shm_ready = init_shm();
                break;

            // case EVT_FRAME_WAITING:
            //     pull_frame(...);
            //     break;

            case EVT_PIPELINE_EXIT:
                //this->closed_ = true;
                this->frame_waiting = false;
                //this->deinit_ = true;
                //soft_deinit();
                //time(&closed_ts);
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
    bool is_frame_waiting() const {return frame_waiting;}
    void set_frame_waiting(bool val){frame_waiting = val;}
    void allow_new_frame(void){
        DataHeader *d = header();
        if (d == nullptr){
            return;
        }
        d->state = SHM_EMPTY;
    }
    bool is_closed(void)const{return closed_;}
    bool is_registered(void) const {return epoll_registered;}
    void set_epoll_flag(bool val){ epoll_registered = val;}
    void mark_closed(void){closed_=true;time(&closed_ts);}
    bool is_stale(void){
        if (closed_){return false;}
        time_t now;
        time(&now);
        if (f_ts_ != 0 && now - f_ts_ > MINUTES_5_IN_SECONDS){
            return true;
        }
        else{
            return false;
        }
    }


};


#endif