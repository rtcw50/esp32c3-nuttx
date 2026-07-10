#ifndef __MOTCONTROL_H
#define __MOTCONTROL_H

#include <nuttx/config.h>
#include <nuttx/mqueue.h>
#include <time.h>

#define MSG_STOP_REQ 1
#define MSG_RUN 0
#define MSG_PAUSE 2
#define MSG_ABORT 3
#define MSG_RESUME 4

#define MC_RAMP_STEP 10
#define MC_SUCCESS 0
#define MC_ABORTED -1
#define MC_NS_PER_MS 1000000
#define MC_MS_PER_SEC 1000
#define MC_NS_PER_SEC (MC_NS_PER_MS * MC_MS_PER_SEC)
#define MOTOR_STATE_STOPPED 0
#define MOTOR_STATE_RUNNING 1
#define MOTOR_STATE_PAUSED 2
#define MOTOR_STATE_ABORTED 3
#define MOTOR_MESSAGE 4

// Message IDs for communication between motor controller and UI
#define MSG_MOTOR_READY 0

struct clean_cmd_msg_s {
    uint8_t command;
    uint16_t run_time_s;
    uint16_t agitate_interval_s;
    uint16_t max_duty;
};

struct clean_tel_msg_s {
    uint8_t state;          /* MOTOR_STATE_RUNNING, etc. */
    union {
        uint16_t time_remaining; /* Seconds left */
        uint16_t message_id;
    };
    uint16_t current_duty;   /* Current PWM value */
};


int mc_get_now(struct timespec *now);
int mc_get_abstime_from_now(struct timespec *base, long nanoseconds);
void mc_motorcontroller_init(void);
int mc_task_entry(int argc, char *argv[]);
int textui_loop(int argc, char *argv[]);

#endif // __MOTCONTROL_H