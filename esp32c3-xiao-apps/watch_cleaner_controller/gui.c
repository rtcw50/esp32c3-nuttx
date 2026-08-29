#include <nuttx/arch.h>
#include <nuttx/config.h>
#include <stdio.h>
#include <unistd.h>
#include <lvgl/lvgl.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <mqueue.h>
#include <nuttx/video/fb.h>
#include "watch_cleaner_controller.h"

/* Externs */
extern int g_speed;
extern int g_duty;
extern int g_duration;
extern int g_agitate_duration;

/* External Prototypes */
bool esp32c3_xiao_tsc_get_xy(int *x, int *y);

/* Local Prototypes */
static void local_lvgl_indev_cb(lv_indev_t *indev, lv_indev_data_t *data);
static void start_btn_event_cb(lv_event_t * e);
static void stop_btn_event_cb(lv_event_t * e);
static int create_button_ui(void);
static void queues_init(void);
static void touchscreen_init(void);
static void handle_motor_telemetry(void);

/* Structure for storing UI context */
struct ui_context_s {
    char str_status[64];
    char str_time_remaining[64];
};

static struct ui_context_s g_ui_ctx;

static char * g_motor_messages[] = {
    "Ready",
};

static lv_obj_t *scr;
static struct clean_cmd_msg_s g_cmd_msg;
static mqd_t cmd_q;
static mqd_t tel_q;


/* Callback function for the "Start" button */
static void start_btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        g_cmd_msg.command = MSG_RUN;
        g_cmd_msg.run_time_s = g_duration;
        g_cmd_msg.agitate_interval_s = g_agitate_duration;
        g_duty = ((g_speed * MOTOR_MAX_DUTY) / MOTOR_MAX_SPEED);
        //g_cmd_msg.max_duty = g_duty;
        g_cmd_msg.max_duty = g_duty;
        int rv = mq_send(cmd_q, (const char *)&g_cmd_msg, sizeof(g_cmd_msg), 0);     
        if (rv < 0) {
            printf("mq_send failed: %d\n", errno);
        }
        else {
            printf("mq_send succeeded\n");
        }

        printf("Start pressed\n");
    }
}

/* Callback function for the "Stop" button */
static void stop_btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        g_cmd_msg.command = MSG_STOP_REQ;
        int rv = mq_send(cmd_q, (const char *)&g_cmd_msg, sizeof(g_cmd_msg), 0);
        if (rv < 0) {
            printf("mq_send failed: %d\n", errno);
        }
        else {
            printf("mq_send succeeded\n");
        }
        printf("Stopped pressed\n");
    }
}

/* Main UI creation function */
static int create_button_ui(void)
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


    /* --- Stop Button --- */
    lv_obj_t * btn_stop = lv_button_create(scr);
    lv_obj_set_size(btn_stop, 100, 50);
    lv_obj_align(btn_stop, LV_ALIGN_CENTER, 60, 0);
    lv_obj_add_event_cb(btn_stop, stop_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * label_stop = lv_label_create(btn_stop);
    lv_label_set_text(label_stop, "Stop");
    lv_obj_center(label_stop);
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
        //printf("X: %d Y: %d\n", x, y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void queues_init()
{
    /* Open the TX channel to the motor controller (Write Only) */
    cmd_q = mq_open("/cleaner_cmd_q", O_WRONLY);
    
    /* Open the RX channel from the motor controller (Read Only) */
    tel_q = mq_open("/cleaner_tel_q", O_RDONLY);
    if (cmd_q == (mqd_t)-1 || tel_q == (mqd_t)-1) {
        printf("watch_cleaner(gui): failed to open message queues\n");
    }
}

static void touchscreen_init()
{
   // Register the input device in user space
    lv_indev_t *indev = lv_indev_create();
    
    // Configure type (e.g., POINTER for touch/mouse, BUTTON for keypads)
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    
    // Attach the local translation callback
    lv_indev_set_read_cb(indev, local_lvgl_indev_cb);
}

static void handle_motor_telemetry()
{
    struct clean_tel_msg_s telemetry_msg;
    struct timespec timeout;
    wcc_get_abstime_from_now(&timeout, 20 ); // 20 ms timeout for non-blocking check
    ssize_t bytes_received = mq_timedreceive(tel_q, (char *)&telemetry_msg, sizeof(telemetry_msg), NULL, &timeout);
    if (bytes_received > 0) {
        // Process the telemetry message and update the UI accordingly
        
        switch (telemetry_msg.state) {
            // TODO: handle time remaining, state, and other message fields
            case MOTOR_STATE_STOPPED:
                /* Reset countdown to default value */
                snprintf(g_ui_ctx.str_status, sizeof(g_ui_ctx.str_status), "XMotor stopped");
                snprintf(g_ui_ctx.str_time_remaining, sizeof(g_ui_ctx.str_time_remaining), "%02d:%02d", telemetry_msg.time_remaining / 60, telemetry_msg.time_remaining % 60);
                break; 
            case MOTOR_STATE_RUNNING:
                /* Update the status and time remaining */
                snprintf(g_ui_ctx.str_status, sizeof(g_ui_ctx.str_status), "XMotor running");
                snprintf(g_ui_ctx.str_time_remaining, sizeof(g_ui_ctx.str_time_remaining), "%02d:%02d", telemetry_msg.time_remaining / 60, telemetry_msg.time_remaining % 60);
                break; 
            case MOTOR_STATE_PAUSED:
                snprintf(g_ui_ctx.str_status, sizeof(g_ui_ctx.str_status), "XMotor paused");
                break; 
            case MOTOR_STATE_ABORTED:
                snprintf(g_ui_ctx.str_status, sizeof(g_ui_ctx.str_status), "XMotor run aborted");
                snprintf(g_ui_ctx.str_time_remaining, sizeof(g_ui_ctx.str_time_remaining), "%02d:%02d", telemetry_msg.time_remaining / 60, telemetry_msg.time_remaining % 60);
                break; 
            // Arbitrary message id from motor controller to UI
            case MOTOR_MESSAGE:
                snprintf(g_ui_ctx.str_status, sizeof(g_ui_ctx.str_status), "%s", g_motor_messages[telemetry_msg.message_id]);
                snprintf(g_ui_ctx.str_time_remaining, sizeof(g_ui_ctx.str_time_remaining), "%02d:%02d", g_duration / 60, g_duration % 60);
                break;
            default:
                snprintf(g_ui_ctx.str_status, sizeof(g_ui_ctx.str_status), "XMotor unknown motor state (%d)", telemetry_msg.state);
                break;
            }
    }
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
    printf("watch_cleaner: display wrapper initialized successfully\n");


    queues_init();
    touchscreen_init();

    /* Build UI on the active screen created by lv_nuttx_init */
    scr = lv_screen_active(); // or lv_scr_act()
    if (scr) {
        if (create_button_ui() != 0) {
            printf("watch_cleaner: did not create buttons\n");
            return -1;
        }
    }
    else {
        printf("watch_cleaner: could not get screen\n");
        return -1;
    }


    /* Main Execution Loop */
    printf("watch_cleaner: Enter main gui loop...\n");
    while (1) {
//        printf ("watch_cleaner: gui loop iteration...\n");
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
        handle_motor_telemetry();
    }

    return 0;
}