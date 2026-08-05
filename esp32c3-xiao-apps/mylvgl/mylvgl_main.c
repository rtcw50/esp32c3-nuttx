#include <nuttx/config.h>
#include <stdio.h>
#include <unistd.h>
#include <lvgl/lvgl.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <nuttx/video/fb.h>

int test_raw_fb(void);

int mylvgl_main(int argc, char *argv[])
{
    /* 1. Board initialization */
#define MYLVGL_STANDALONE
#ifdef MYLVGL_STANDALONE
    if (board_app_initialize(0) < 0) {
        printf("mylvgl: board_app_initialize failed\n");
        return -1;
    }   
    printf("mylvgl: board initialized\n");
#endif

#if 0
    if (test_raw_fb() == 0) {
        printf("mylvgl:test_raw_fb - Test passed\n");
        return 0;
    }
#endif

    
    /* 2. Initialize LVGL core */
    lv_init();

    /* 3. Initialize NuttX LVGL Driver Abstraction */
    lv_nuttx_dsc_t dsc;
    lv_nuttx_dsc_init(&dsc);

    /* Point to the device node created by NuttX driver */
    /* Use /dev/lcd0 if using the NuttX LCD subsystem, or /dev/fb0 for framebuffer */
    dsc.fb_path = "/dev/lcd0";  // Change to dsc.dev_path = "/dev/lcd0"; if using /dev/lcd0

    lv_nuttx_result_t result;
    lv_nuttx_init(&dsc, &result);
    if (result.disp == NULL ) {
        printf("mylvgl: lv_nuttx_init failed!\n");
        return -1;
    }
    printf("mylvgl: display wrapper initialized successfully\n");

    /* 4. Build UI on the active screen created by lv_nuttx_init */
    lv_obj_t *scr = lv_screen_active(); // or lv_scr_act()
    if (scr) {
        lv_obj_t *label = lv_label_create(scr);
        if (label) {
            lv_label_set_text(label, "Hello NuttX!");
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        }
    }

    /* 5. Main Execution Loop */
    printf("mylvgl: Enter main loop...\n");
    while (1) {
        uint32_t time_till_next = lv_timer_handler();
        
        /* Clamp sleep time to avoid integer overflow when no timer is pending */
        if (time_till_next > 30 || time_till_next == 0) {
            time_till_next = 5; 
        }
        
        usleep(time_till_next * 1000);
        // To test for SPI activity
        //lv_obj_invalidate(lv_screen_active());
    }

    return 0;
}


#if 0
int test_raw_fb(void)
{
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        printf("Failed to open /dev/fb0\n");
        return -1;
    }

    struct fb_videoinfo_s vinfo;
    struct fb_planeinfo_s pinfo;

    /* Get info */
    ioctl(fd, FBIOGET_VIDEOINFO, &vinfo);
    ioctl(fd, FBIOGET_PLANEINFO, &pinfo);

    printf("FB Res: %dx%d, BPP: %d, MemSize: %zu\n", 
            vinfo.xres, vinfo.yres, pinfo.bpp, pinfo.fblen);

    /* Fill framebuffer memory with white (0xFFFF for 16-bit RGB565) */
    memset(pinfo.fbmem, 0xFF, pinfo.fblen);

    /* Trigger flush if update area ioctl is supported */
    struct fb_area_s area = {
        .x = 0, .y = 0,
        .w = vinfo.xres, .h = vinfo.yres
    };
    ioctl(fd, FBIO_UPDATE, &area);

    close(fd);
    return 0;
}
#endif
