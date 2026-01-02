#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm_fourcc.h>
#include <sys/ioctl.h>
#include <linux/dma-heap.h>

// This is a simplified version of the create_dmabuf logic from your code
static int create_dmabuf(size_t size) {
    const char* heap_path = "/dev/dma_heap/system";
    int heap_fd = open(heap_path, O_RDONLY | O_CLOEXEC);
    if (heap_fd < 0) {
        perror("[DMABUF-CREATOR] Failed to open DMA heap");
        return -1;
    }

    struct dma_heap_allocation_data heap_data = {
        .len = size,
        .fd_flags = O_RDWR | O_CLOEXEC,
    };

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data) != 0) {
        perror("[DMABUF-CREATOR] DMA heap allocation failed");
        close(heap_fd);
        return -1;
    }
    close(heap_fd);
    return heap_data.fd;
}

// This external program is "pure C" and is trusted by the GPU driver.
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <width> <height> <socket_fd_to_parent>\n", argv[0]);
        return 1;
    }

    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    int sock_fd = atoi(argv[3]); // The socket FD is passed directly

    printf("[DMABUF-CREATOR] Pure C helper started (PID=%d)\n", getpid());

    // Use a compatible format and calculate stride
    uint32_t format = DRM_FORMAT_XRGB8888; // Highly compatible
    uint32_t stride = ((width * 4) + 255) & ~255; // 256-byte aligned stride
    size_t buffer_size = stride * height;

    int dmabuf_fd = create_dmabuf(buffer_size);
    if (dmabuf_fd < 0) {
        fprintf(stderr, "[DMABUF-CREATOR] Failed to create DMA-BUF\n");
        return 1;
    }

    printf("[DMABUF-CREATOR] Created DMA-BUF (fd=%d), sending to parent...\n", dmabuf_fd);

    // --- Send the FD back to the parent ---
    struct msghdr msg = {0};
    struct iovec iov[1];
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    char dummy_data = 'D'; // We need to send at least one byte of data

    iov[0].iov_base = &dummy_data;
    iov[0].iov_len = sizeof(dummy_data);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &dmabuf_fd, sizeof(int));

    if (sendmsg(sock_fd, &msg, 0) < 0) {
        perror("[DMABUF-CREATOR] Failed to send DMA-BUF FD");
        close(dmabuf_fd);
        close(sock_fd);
        return 1;
    }

    printf("[DMABUF-CREATOR] Successfully sent FD. Exiting.\n");
    close(dmabuf_fd);
    close(sock_fd);
    return 0;
}