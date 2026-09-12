#ifndef __WATCH_CLEANER_CONTROLLER_H
#define __WATCH_CLEANER_CONTROLLER_H

#include "lvgl/src/tick/lv_tick.h"
#include <nuttx/config.h>
#include <nuttx/mqueue.h>
#include <lvgl/lvgl.h>
#include <time.h>

/* UI to Motor message defines */
#define MSG_ACTION_PWM_START 0
#define MSG_ACTION_PWM_STOP 1   
#define MSG_ACTION_PWM_SET_DUTY 2  // cmd->value is duty cycle to set 
#define MSG_ACTION_PWM_REVERSE 3 
#define MSG_ACTION_PWM_ABORT 4 
#define MSG_ACTION_PWM_SET_DUTY_ALL 5  // set all pwms to this value, cmd->value is duty cycle to set 

/* Motor to UI message defines */
#define MOTOR_STATE_STOPPED 0
#define MOTOR_STATE_RUNNING 1
#define MOTOR_STATE_PAUSED 2
#define MOTOR_STATE_ABORTED 3
#define MOTOR_MESSAGE 4
#define MOTOR_READY 5


#define MC_RAMP_STEP 1 // Increase value for faster ramping, decrease for slower ramping
#define MC_SUCCESS 0
#define MC_ABORTED -1
#define MC_NS_PER_MS 1000000
#define MC_MS_PER_SEC 1000
#define MC_NS_PER_SEC (MC_NS_PER_MS * MC_MS_PER_SEC)


// Limits for motor speed and duty cycle
#define MOTOR_MAX_SPEED 600 // Maximum speed in RPM    
#define MOTOR_MIN_SPEED 0    // Minimum speed in RPM    
#define MOTOR_MAX_DUTY 95   // Maximum duty cycle percentage, 95% to avoid pwm saturation 
#define MOTOR_MIN_DUTY 0    // Minimum duty cycle percentage, 5% to avoid underdriving the motor

/* Message queue definitions */
#define MSG_SET_TYPE 0
#define MSG_ACTION_TYPE 1


struct clean_cmd_msg_s {
    uint16_t msg_type;              /* MSG_SET_TYPE or MSG_ACTION_TYPE*/
    union {
        uint16_t value;           /* Value, a set value associated with the type of message */
    };
};

struct clean_tel_msg_s {
    uint8_t state;          /* MOTOR_STATE_RUNNING, etc. */
    union {
        uint16_t time_remaining; /* Seconds left */
        uint16_t message_id;     /* If MOTOR_MESSAGE is received */
    };
};

/* GUI elements*/
#define WCC_VER "1.0"

/* Custom colors (lv_color_t) picked from: https://codepen.io/kevinli/pen/GRpXOvo */
#define WCC_BACKGROUND_GREY 0x99,0x99,0x99
#define WCC_TITLE_BLUE 0x0c,0x00,0xcc
#define WCC_BUTTON_GREEN 0x6f,0xe0,0x00
#define WCC_BUTTON_RED 0xff,0x28,0x28
#define WCC_BUTTON_YELLOW 0xff,0xff,0x78
#define TIME_FORMAT "%02ld:%02ld"
#define SCALAR_FORMAT "%4ld"

// #define CLEAN_DUR_DEFAULT (5*60)
 #define CLEAN_DUR_DEFAULT (60)
 #define CLEAN_DUR_MAX (60*60)
 #define RINSE_DUR_DEFAULT (3*60)
 #define RINSE_DUR_MAX (60*60)
 #define SPIN_DUR_DEFAULT  (1*60) 
 #define SPIN_DUR_MAX (60*60)
 //#define AGITATE_DUR_DEFAULT (10)
 #define AGITATE_DUR_DEFAULT (5)
 #define AGITATE_DUR_MAX (60)
 #define RPM_DEFAULT (300)
 #define RPM_MAX (600)
 #define RAMP_FACTOR_DEFAULT (5)
 #define RAMP_FACTOR_MAX (10)

 #define RAMP_UPDATE_MS (250)
 #define RAMP_UPDATE_STEPS_PER_SECOND (4) // 1/RAMP_UPDATE_MS

 #define WCC_IN1 5 
 #define WCC_IN2 21 

enum  OperatingMode {
  clean,
  rinse,
  spin
};

/* Motor state */
enum  OperatingState {
  running,
  stopped
};

typedef struct time_format {
  int32_t min;
  int32_t sec;
} time_format;

typedef struct pwm_info { 
int32_t pwm_rpm;       // Actual PWM setting based on the RPM setting, calculate when RPM changes
int32_t pwm_increment; // Actual PWM increment based on spin up time setting 
} pwm_info;

/* Generic WCC callback type */
typedef void (*wcc_cb_t)(void);


/* motor_driver.c functions */
int wcc_motor_driver_init(void);
void wcc_motor_driver_shutdown(void);
int wcc_motor_driver_start_pwm(void);
int wcc_motor_driver_stop_pwm(void);
void wcc_motor_driver_set_duty(uint16_t target_duty);
void wcc_motor_driver_set_duty_all(uint16_t target_duty);
void wcc_motor_driver_reverse_motor_direction(void);

/* util.c functions */
#ifdef __cplusplus
extern "C" {
#endif
int wcc_get_now(struct timespec *now);
int wcc_get_abstime_from_now(struct timespec *base, long milliseconds);
int wcc_timespec_compare(const struct timespec *a, const struct timespec *b);
struct timespec wcc_get_min_deadline(struct timespec *t1, struct timespec *t2, struct timespec *t3);
void wcc_timespec_add_ms(struct timespec *ts, long ms);
void ui_send_cmd(mqd_t *q, uint16_t msg_type,uint16_t value);
void motor_send_telemetry(mqd_t *q, uint16_t msg_type,uint16_t value);
void wcc_delay_ms(int32_t ms);
#ifdef __cplusplus
}
#endif

/* motorcontroller.c functions */
int wcc_motor_task(int argc, char *argv[]);

/* gui.c functions */
int wcc_gui_task(int argc, char *argv[]);

/* wcc_main_screen.c functions */
lv_obj_t * wcc_get_main_screen(void);
lv_obj_t * wcc_create_title_bar(lv_obj_t * parent, const char * title);


/* wcc_settings.c functions */
lv_obj_t * wcc_get_settings_screen(void);
uint16_t get_duration(int32_t operating_mode);
uint16_t get_agitate_interval_duration(void);
uint16_t get_ramp_factor(void);
uint16_t get_duty_cycle(void);

/* wcc_styles.c functions */
void wcc_set_screen_bg_style(lv_obj_t * scr);

// Extern C functions for C++ code to call C functions
#ifdef __cplusplus
extern "C" {
#endif
    void wcc_create_main_screen_widgets(mqd_t * ui_send_q, mqd_t * ui_receive_q);
    void wcc_handle_motor_telemetry(void);
    void wcc_create_settings(void);
    void wcc_init_styles(void);
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __WATCH_CLEANER_CONTROLLER_H