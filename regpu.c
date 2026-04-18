
/*

 reGPU 0.2
 Licensed under GPLv3

*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <time.h>

static volatile int running = 1;
void handle_sig(int sig) { (void)sig; running = 0; }

int main() {
    signal(SIGINT, handle_sig);
    Display *dpy = XOpenDisplay(":8");
    if (!dpy) { fprintf(stderr, "Could not open the headless NVIDIA display\nIs X running at :8?"); return 1; }
    int screen = DefaultScreen(dpy);
    int w = DisplayWidth(dpy, screen);
    int h = DisplayHeight(dpy, screen);
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) { perror("fb0"); return 1; }
    struct fb_fix_screeninfo finfo;
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) { perror("ioctl"); return 1; }
    uint8_t *fb_ptr = mmap(NULL, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_ptr == MAP_FAILED) { perror("mmap"); return 1; }
    XShmSegmentInfo shminfo;
    memset(&shminfo, 0, sizeof(shminfo));
    int depth = DefaultDepth(dpy, screen);
    XImage *img = XShmCreateImage(dpy, DefaultVisual(dpy, screen), depth, ZPixmap, NULL, &shminfo, w, h);
    shminfo.shmid = shmget(IPC_PRIVATE, img->bytes_per_line * img->height, IPC_CREAT | 0600);
    shminfo.shmaddr = img->data = shmat(shminfo.shmid, NULL, 0);
    shminfo.readOnly = False;
    XShmAttach(dpy, &shminfo);
    XSync(dpy, False);
    shmctl(shminfo.shmid, IPC_RMID, NULL);
    size_t line_size = w * 4;
    size_t fb_stride = finfo.line_length;
    size_t img_stride = img->bytes_per_line;
    int pmatch = (fb_stride == img_stride);
    printf("reGPU 0.2\n");
    struct timespec req = {0, 16666666};
    while (running) {
        XShmGetImage(dpy, RootWindow(dpy, screen), img, 0, 0, AllPlanes);
        if (pmatch) {
            memcpy(fb_ptr, img->data, img_stride * h);
        } else {
            for (int y = 0; y < h; y++) {
                memcpy(fb_ptr + (y * fb_stride), img->data + (y * img_stride), line_size);
            }
        }
        nanosleep(&req, NULL);
    }
    XShmDetach(dpy, &shminfo);
    shmdt(shminfo.shmaddr);
    XDestroyImage(img);
    XCloseDisplay(dpy);
    close(fb_fd);
    return 0;
}
