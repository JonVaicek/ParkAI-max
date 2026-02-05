#include "gst_worker.h"
#include "camstream.h"
#include <mutex>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>
#include <unistd.h>

#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <linux/memfd.h>
#include <sys/mman.h>

/* THESE WILL HAVE TO BE SOFT PROGRAMMED LATER*/
#define STREAM_ID 1
//#define RTSP_URL "rtsp://admin:Mordoras512_@192.168.0.13:554/live0"
#define RTSP_URL "rtsp://admin:1234@192.168.0.20:554/h.264"

static std::atomic<bool> g_run{true};
static bool mem_shared = false;
int memhandle = 0;
void* shm = nullptr;

StreamCtrl ctrl;


static int memfd_create_compat(const char* name, unsigned int flags) {
#ifdef SYS_memfd_create
    return (int)syscall(SYS_memfd_create, name, flags);
#else
    errno = ENOSYS;
    return -1;
#endif
}

static int send_fd(int sock, int fd_to_send) {
    char dummy = 'F';
    struct iovec iov{ &dummy, 1 };

    char cmsgbuf[CMSG_SPACE(sizeof(int))];
    std::memset(cmsgbuf, 0, sizeof(cmsgbuf));

    struct msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));

    std::memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));
    //msg.msg_controllen = cmsg->cmsg_len;

    return sendmsg(sock, &msg, 0);
}

/* sends signal to the parent */
static int send_signal(uint64_t sig, int evfd){
    if (eventfd_write(evfd, sig) != 0) {
        return 0;
    }
    else {
        return 1;
    }
}

int create_shared_mem(DataHeader &dh, int ctrlfd){
    int shmfd = memfd_create_compat("camframe", 0);
    uint64_t total_bytes = dh.nbytes + sizeof(DataHeader);
    if (shmfd < 0) { 
        perror("memfd_create"); 
        _exit(1); 
    }
    if (ftruncate(shmfd, total_bytes) != 0) { 
        perror("ftruncate"); 
        _exit(1); 
    }
    if (send_fd(ctrlfd, shmfd) == -1) {
        perror("send_fd");
        _exit(1);
    }

    mem_shared = true;
    return shmfd;
}

static void on_signal(int) {
    g_run = false;
    ctrl.run = false;
    ctrl.restart = true;
    quit_pipeline(&ctrl);
}

static void on_sigterm(int){
    //std::cout << " [child] exiting on sigterm\n";
    exit(0);
}


int main(int argc, char** argv) {

    int ctrl_fd = 3;
    int evfd    = 4;
    std::signal(SIGTERM, on_sigterm);
    //std::signal(SIGINT,  on_signal);

    gst_init(&argc, &argv);

    std::cout << "[worker] starting gstreamer worker, pid=" << getpid() << std::endl;

    const char* rtsp_url = (argc > 1) ? argv[1] : "EMPTY";
    /* Creating gstreamer pipeline */
    std::mutex lock;
    ctrl.lock = &lock;
    vstream stream = load_video_stream(STREAM_ID, rtsp_url, &ctrl);

    int ret = send_signal(EVT_CHILD_STARTED, evfd);
    if(ret){
        std::cout << " [child] sent signal = " << EVT_CHILD_STARTED << "\n"; 
    }
    
    while(g_run){
            if (ctrl.ended){
                if(send_signal(EVT_PIPELINE_EXIT, evfd)){
                    std::cout << " [child] sent signal = " << EVT_PIPELINE_EXIT << "\n"; 
                    g_run = false;
                }
            }
            if(ctrl.frame_rd == true && ctrl.restart == false){
                /* Share memory once */
                if (!mem_shared){
                    DataHeader dh;
                    dh.nbytes = ctrl.im_size;
                    dh.h = ctrl.imgH;
                    dh.w = ctrl.imgW;
                    //std::cout << " [gst_worker] Allocated Memory Array = " << dh.nbytes << std::endl;
                    memhandle = create_shared_mem(dh, ctrl_fd);
                    uint64_t tbytes = dh.nbytes + sizeof(DataHeader);
                    shm = mmap(nullptr, tbytes, PROT_READ | PROT_WRITE, MAP_SHARED, memhandle, 0);

                    /* init data header */
                    DataHeader * header = (DataHeader *)shm;
                    uint8_t *data = (uint8_t *)shm + sizeof(DataHeader);
                    header->state = SHM_EMPTY;
                    header->nbytes = ctrl.im_size;
                    header->h = ctrl.imgH;
                    header->w = ctrl.imgW;
                    memcpy(data, ctrl.image, ctrl.im_size);
                    header->state = SHM_READY;
                    ctrl.frame_rd = false;
                    g_object_set(ctrl.valve, "drop", FALSE, NULL);
                    if(send_signal(EVT_MMSH_COMPLETE|EVT_FRAME_WAITING, evfd)){
                        std::cout << " [child] - memory shared complete sent\n";
                    }
                }
                else{
                    
                    DataHeader *hdr = (DataHeader *)shm;
                    uint8_t *data = (uint8_t *)shm + sizeof(DataHeader);
                    /* Wait until frame is read */
                    if (hdr->state == SHM_EMPTY){
                        //std::cout << " [g_worker] New Frame is Ready\n";
                        memcpy(data, ctrl.image, ctrl.im_size);
                        hdr->nbytes = ctrl.im_size;
                        hdr->h = ctrl.imgH;
                        hdr->w = ctrl.imgW;
                        hdr->state = SHM_READY;
                        /* let pipeline acquire new frame */
                        ctrl.frame_rd = false;
                        g_object_set(ctrl.valve, "drop", FALSE, NULL);
                        if(send_signal(EVT_FRAME_WAITING, evfd)){
                            std::cout << " [child] - new frame event sent\n";
                        }
                    }
                }

            }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if(stream.joinable()){
        stream.join();
    }

    std::cout << "[child] exiting" << std::endl;
    return 0;
}
