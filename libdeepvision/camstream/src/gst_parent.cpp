

#include "gst_parent.h"
#include "gst_worker.h"




int recv_fd(int sock) {
    char dummy;
    struct iovec iov{ &dummy, 1 };

    char cmsgbuf[CMSG_SPACE(sizeof(int))];
    std::memset(cmsgbuf, 0, sizeof(cmsgbuf));

    struct msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    if (recvmsg(sock, &msg, 0) != 1) return -1;

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg) return -1;

    int fd = -1;
    std::memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
    return fd;
}


uint64_t signal_parser(uint64_t val){
    printf(" [sig-parser] received: %lu\n", val);
    if((val & EVT_PIPELINE_EXIT) == EVT_PIPELINE_EXIT){
        return EVT_PIPELINE_EXIT;
    }
    if ((val & EVT_MMSH_COMPLETE) == EVT_MMSH_COMPLETE){
        return EVT_MMSH_COMPLETE;
    }
    if((val & EVT_CHILD_STARTED) == EVT_CHILD_STARTED){
        return EVT_CHILD_STARTED;
    }
    return 0;
}