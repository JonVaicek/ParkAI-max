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
#include <fcntl.h>
#include <poll.h>
#include <time.h>

#define GST_WORKER_PATH "./libdeepvision/camstream/gst_worker"
#define STREAM_IS_OFF_AFTER 300  /* seconds */
#define MINUTES_5_IN_SECONDS 300


int recv_fd(int sock);
uint64_t signal_parser(uint64_t val);


enum ChildState{
    CREATION = 0,
    ALIVE = 1,
    INFECTED,
    ZOMBIE,
    PURGED,
    BURIED
};

class GstChildWorker{
private:
    static const int STRING_SIZE = 254; 
    int id;
    pid_t pid_;

    uint64_t n_read=0;
    char fn[STRING_SIZE];
    char worker_path[STRING_SIZE];

    bool frame_waiting = false;
    time_t f_ts_ = 0;
    time_t closed_ts = 0;
    bool epoll_registered = false;

    /* private shmfd variables*/
    void* shm_ = MAP_FAILED;
    size_t shm_bytes_ = -1;
    DataHeader* hdr_ = nullptr;
    bool shm_ready = false;
    bool shm_mapped = false;
    

public:
    ChildState state = CREATION; // initial state of this class

    int shmfd_;
    int sv_[2] = {-1, -1};
    int evfd_;
    char rtsp_url_[STRING_SIZE];

    /* constructor */
    GstChildWorker(void):
    pid_(-1), shmfd_(-1), evfd_(-1)
    {
    }
    uint32_t start_child(void){
        /* Create socket pair */
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv_) != 0) {
            printf(" [child-%d] socket pair error\n", id);
            return 0;
        }
        printf("[src-%s] socket pair created sv_={ %d, %d }\n", rtsp_url_, sv_[0], sv_[1]);

        /* Create event file-descriptor */
        evfd_ = eventfd(0, EFD_CLOEXEC);
        if (evfd_ < 0) {
            printf(" [child-%d] eventfd error\n", id);
            return 0;
        }
        printf("[src-%s] evfd created evfd={ %d }\n", rtsp_url_, evfd_);
        /* Create Child Process */
        pid_ = fork();

        if (pid_ < 0) {
            std::cerr << "[parent] fork failed: " << std::strerror(errno) << "\n";
            return 0;
        }
        /* Child code begin */
        if (pid_ == 0) {
            // remove CLOEXEC flags from fds that are passed to the child
            fcntl(sv_[1], F_SETFD, fcntl(sv_[1], F_GETFD) & ~FD_CLOEXEC);
            fcntl(evfd_, F_SETFD, fcntl(evfd_, F_GETFD) & ~FD_CLOEXEC);

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
            // child branch: replace process image
            execl(GST_WORKER_PATH, GST_WORKER_PATH, rtsp_url_, (char*)nullptr);
            // only reached if exec fails
            std::cerr << "[parent->child] exec failed: " << std::strerror(errno) << "\n";
            return 0;
        }
        /* Parent Code Continue */
        close(sv_[1]);
        snprintf(fn, STRING_SIZE, "image-%d.jpeg", id);

        epoll_registered = false;
        closed_ts = 0;
        f_ts_= 0;
        shm_mapped = false;
        state = ALIVE;
        printf("[src-%s] init complete\n", rtsp_url_);
        return 1;
    }

    uint32_t init(int id, const char *rtsp_url){
        this->id = id;
        snprintf(rtsp_url_, STRING_SIZE, "%s", rtsp_url); 
        start_child();
    }

    int reinit(void){
        if (is_past_timeout()){
            start_child();
        }
        return 1;
    }

    int init_shm(void){
        shmfd_ = recv_fd(sv_[0]);
        if (shmfd_ < 0) {
            printf(" [%s] shared mem error\n", rtsp_url_);
            return 0;
        }
        int flags = fcntl(shmfd_, F_GETFD);
        if (flags != -1) {
            fcntl(shmfd_, F_SETFD, flags | FD_CLOEXEC);
        }

        struct stat st{};
        fstat(shmfd_, &st);
        shm_bytes_ = st.st_size;
        shm_ = mmap(nullptr, shm_bytes_, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd_, 0);
        if (shm_ == MAP_FAILED) {
            printf(" [child-%s] memorymap error\n", rtsp_url_);
            return 0;
        }
        hdr_ = reinterpret_cast<DataHeader*>(shm_);
        shm_mapped = true;
        printf("[%s] - mapped shared buffer, size %lu", rtsp_url_, shm_bytes_);
        return 1;
    }

    ~GstChildWorker() {
    }

    uint32_t close_evfd(void){
        if (evfd_ > 0){
            close(evfd_);
            evfd_ = -1;
            return 1;
        }
        return 0;
    }
    uint32_t close_sockfd(void){
        if(sv_[0] >= 0){
            close(sv_[0]);
            printf("[%s] socketfd (%d) closed successfully\n", rtsp_url_, sv_[0]);
            sv_[0] = -1;
            return 1;
        }
        return 0;
    }

    uint32_t close_shmfd(void){
        if (shmfd_ >= 0) {
            close(shmfd_);
            printf("[%s] shared mem fd (%d) closed successfully\n", rtsp_url_, shmfd_);
            shmfd_ = -1;
            return 1;
        }
        return 1;
    }

    uint32_t bury(void){
        if (shmfd_ == -1 && evfd_ == -1 && sv_[0] == -1 && !shm_mapped){
            state = BURIED;
            time(&closed_ts);
        }
        return 1;
    }

    uint32_t release_mem(void){
        
        if (shm_mapped){
            if(munmap(shm_, shm_bytes_) != -1){
                shm_mapped = false;
                shm_ = MAP_FAILED;
                hdr_ = nullptr;
                shm_bytes_ = 0;
                if(close_shmfd()){
                    shm_ready = false;
                    printf("[%s] - memory unmaped\n", rtsp_url_);
                    return 1;
                }
            }
            else{
                printf(" [%s] - Error Unmapping Shared Memory\n", rtsp_url_);
            }
        }
        else{
            if(close_shmfd()){
                shm_ready = false;
                return 1;
            }
        }
        return 0;
    }


    uint32_t killit(void){
        std::cout << "[parent] Killing child with pid= "<< pid_ << std::endl;
        if (pid_ > 0) {
            kill(pid_, SIGKILL);
            //kill(pid_, SIGTERM);
            state = ZOMBIE;
        }
        else{
            printf("[parent] can't kill child [%s] pid is invalid\n", rtsp_url_);
            return 0;
        }
        return 1;
    }


    uint32_t reap(void){
        if (pid_ > 0){
            int status;
            pid_t result = waitpid(pid_, &status, WNOHANG);
            if (result == 0) {
                printf("[%s] - could not reap\n", rtsp_url_);
                return 0; // still running, must check later
            }
            else if (result == pid_){
                // child exited, handle status
                printf("[%s] - reaped succesfully\n", rtsp_url_);
                pid_ = -1;
                state = PURGED;
                return 1;
            }
            else {
                // error
                std::cout << "Error reaping children\n";
                return 0;
            }
        }
        else{
            printf("Eror reaping child [%s] not zombie or pid is invalid\n", rtsp_url_);
            return 0;
        }
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


    int read_frame(unsigned char **img_buf, uint64_t *size){
        /* first check if data exists*/
        DataHeader *d = header();
        if (d == nullptr){
            return 0;
        }
        if( state != ALIVE || d->state != SHM_READY){
            return 0;
        }
        /* allocate destination buffer if does not exist*/
        if (*img_buf == nullptr){
            *img_buf = (unsigned char*)malloc(d->nbytes);
        }
        else{
            if (*size != d->nbytes){
                free(*img_buf);
                *img_buf = (unsigned char*)malloc(d->nbytes);
            }
        }
        *size = d->nbytes;
        memcpy(*img_buf, data_ptr(), d->nbytes);
        /* set buffer to read */
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

            case EVT_PIPELINE_EXIT:
                printf("[%s] adding to infected\n", rtsp_url_);
                state = INFECTED;
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

    bool is_registered(void) const {return epoll_registered;}
    void set_epoll_flag(bool val){ epoll_registered = val;}

    bool is_infected(void){
        time_t now;
        time(&now);
        if (f_ts_ != 0 && now - f_ts_ > MINUTES_5_IN_SECONDS){
            state = INFECTED;
            return true;
        }
        else{
            return false;
        }
    }
};


#endif