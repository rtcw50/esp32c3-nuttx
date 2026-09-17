#include <nuttx/arch.h>
#include <nuttx/config.h>
#include <stdio.h>
#include <unistd.h>
#include <lvgl/lvgl.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <nuttx/mqueue.h>
#include <nuttx/video/fb.h>
#include "watch_cleaner_controller.h"

/* Defines */
#define SCREEN_HEIGHT 240

/* Global Variables */
extern bool g_force_calibration;

/* External Prototypes */
bool esp32c3_xiao_tsc_get_xy(int *x, int *y);
bool esp32c3_xiao_get_touch(int32_t *x, int32_t *y);

/* Local Prototypes */
static void local_lvgl_indev_cb(lv_indev_t *indev, lv_indev_data_t *data);
static void queues_init(void);
static void touchscreen_init(void);
static mqd_t cmd_q;
static mqd_t tel_q;


/* Local wrapper callback that satisfies LVGL structure requirements */
static void local_lvgl_indev_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    int32_t x = 0;
    int32_t y = 0;

    if (esp32c3_xiao_get_touch(&x, &y)) {
//    if (esp32c3_xiao_tsc_get_xy(&x, &y)) {
        data->point.x = x;
        data->point.y = SCREEN_HEIGHT-y;
        data->state = LV_INDEV_STATE_PRESSED;
//        printf("X: %d Y: %d\n", x, y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void queues_init()
{
const struct mq_attr cleaner_cmd_attr = {
    .mq_maxmsg = 10,
    .mq_msgsize = sizeof(struct clean_cmd_msg_s),
    .mq_flags = 0
};
const struct mq_attr cleaner_tel_attr = {
    .mq_maxmsg = 10,
    .mq_msgsize = sizeof(struct clean_tel_msg_s),
    .mq_flags = 0
};
    /* Open the TX channel to the motor controller (Write Only) */
    cmd_q = mq_open("/cleaner_cmd_q", O_WRONLY|O_NONBLOCK, 0666, &cleaner_cmd_attr);
    
    /* Open the RX channel from the motor controller (Read Only) */
    tel_q = mq_open("/cleaner_tel_q", O_RDONLY|O_NONBLOCK, 0666, &cleaner_tel_attr);
    if (cmd_q == (mqd_t)-1 || tel_q == (mqd_t)-1) {
        printf("watch_cleaner(gui): failed to open message queues\n");
    }
}

static void touchscreen_init(void)
{
   // Register the input device in user space
    lv_indev_t *indev = lv_indev_create();
    
    // Configure type (e.g., POINTER for touch/mouse, BUTTON for keypads)
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    
    // Attach the local translation callback
    lv_indev_set_read_cb(indev, local_lvgl_indev_cb);
}


/* Public functions*/
int wcc_gui_task(int argc, char *argv[])
{
    /* 1. Initialize LVGL core */
    lv_init();

    /* 2. Initialize NuttX LVGL Driver Abstraction */
    lv_nuttx_dsc_t dsc;
    lv_nuttx_dsc_init(&dsc);

    /* Point to the device node created by NuttX driver */
    /* Use /dev/lcd0 if using the NuttX LCD subsystem, or /dev/fb0 for framebuffer */
    dsc.fb_path = "/dev/lcd0";  // Change to dsc.dev_path = "/dev/lcd0"; if using /dev/lcd0

    lv_nuttx_result_t result;
    lv_nuttx_init(&dsc, &result);
    if (result.disp == NULL ) {
        printf("watch_cleaner: lv_nuttx_init failed!\n");
        return -1;
    }
    //printf("watch_cleaner: display wrapper initialized successfully\n");
    //printf("Hor: %ld, Ver: %ld\n",lv_disp_get_hor_res(result.disp), lv_disp_get_ver_res(result.disp));



    queues_init();
    touchscreen_init();

    /* Build UI on the active screen created by lv_nuttx_init */
    wcc_init_styles();

    wcc_create_main_screen_widgets(&cmd_q, &tel_q);
    wcc_create_settings();
    wcc_create_calibration_screen();

    /* Check for calibration requirements */
    wcc_calibration_setup(g_force_calibration);
    g_force_calibration = false;


    /* Main Execution Loop */
    //printf("watch_cleaner: Enter main gui loop...\n");
    while (1) {
        //printf ("watch_cleaner: gui loop iteration...\n");
        uint32_t time_till_next = lv_timer_handler();
        
        /* Clamp sleep time to avoid integer overflow when no timer is pending */
        #if 0
        if (time_till_next > 30 || time_till_next == 0) {
            time_till_next = 5; 
        }
        #endif
        if (time_till_next > 10 || time_till_next == 0) {
            time_till_next = 5; 
        }
        
        usleep(time_till_next * 1000);
        // Check for any telemetry messages from the motor controller and update the UI accordingly
        wcc_handle_motor_telemetry();
    }

    return 0;
}