#include <nuttx/config.h>
#include <stdio.h>
#include <unistd.h>
#include <lvgl/lvgl.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <nuttx/video/fb.h>

bool esp32c3_xiao_tsc_get_xy(int *x, int *y);

lv_obj_t *scr;

/* Callback function for the "Start" button */
static void start_btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        printf("Starts pressed\n");
    }
}

/* Callback function for the "Stop" button */
static void stop_btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        printf("Stopped pressed\n");
    }
}

/* Main UI creation function */
int create_button_ui(void)
{
    /* Get active screen */
    scr = lv_screen_active();
    if (!scr) {
        return 1;
    }

    /* --- Start Button --- */
    lv_obj_t * btn_start = lv_button_create(scr);
    lv_obj_set_size(btn_start, 100, 50);
    lv_obj_align(btn_start, LV_ALIGN_CENTER, -60, 0);
    lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_start, start_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * label_start = lv_label_create(btn_start);
    lv_label_set_text(label_start, "Start");
    lv_obj_center(label_start);

#if 0

    /* --- Stop Button --- */
    lv_obj_t * btn_stop = lv_button_create(scr);
    lv_obj_set_size(btn_stop, 100, 50);
    lv_obj_align(btn_stop, LV_ALIGN_CENTER, 60, 0);
    lv_obj_add_event_cb(btn_stop, stop_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * label_stop = lv_label_create(btn_stop);
    lv_label_set_text(label_stop, "Stop");
    lv_obj_center(label_stop);
#endif
    return 0;
}

/* Local wrapper callback that satisfies LVGL structure requirements */
static void local_lvgl_indev_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    int x = 0;
    int y = 0;

    if (esp32c3_xiao_tsc_get_xy(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        printf("X: %d Y: %d\n", x, y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
void touchscreen_init()
{
   // Register the input device in user space
    lv_indev_t *indev = lv_indev_create();
    
    // Configure type (e.g., POINTER for touch/mouse, BUTTON for keypads)
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    
    // Attach the local translation callback
    lv_indev_set_read_cb(indev, local_lvgl_indev_cb);
}
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

    touchscreen_init();


#if 1
    /* 4. Build UI on the active screen created by lv_nuttx_init */
    scr = lv_screen_active(); // or lv_scr_act()
    if (scr) {
#if 0
        lv_obj_t *label = lv_label_create(scr);
        if (label) {
            lv_label_set_text(label, "Hello NuttX!");
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        }
#endif
        lv_obj_t * btn_start = lv_btn_create(scr);
        lv_obj_set_size(btn_start, 100, 50);
        lv_obj_align(btn_start, LV_ALIGN_CENTER, -60, 0);
        lv_obj_set_style_bg_color(btn_start, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_add_event_cb(btn_start, start_btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * label_start = lv_label_create(btn_start);
        lv_label_set_text(label_start, "Start");
        lv_obj_center(label_start);
    }
#endif 
#if 0
    if (create_button_ui() != 0) {
        printf("mylvgl: did not create buttons\n");
        return -1;
    }
#endif


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
