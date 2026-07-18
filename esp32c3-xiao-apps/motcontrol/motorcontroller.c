#include <nuttx/config.h>
#include <nuttx/mqueue.h>
#include <time.h>
#include "motcontrol.h"

static mqd_t cmd_q;
static mqd_t tel_q;

static int run_cleaning_cycle(struct clean_cmd_msg_s *cmd, struct clean_tel_msg_s *res_from_motor);
static int handle_ui_command(struct clean_cmd_msg_s *cmd); 

void mc_motorcontroller_init() {
    // Initialize the motor controller
}


int mc_task_entry(int argc, char *argv[])
{
    /* Open the RX channel from the UI (Read Only) */
    /* Note: We keep this blocking, so the motor task sleeps until the UI sends a message  */
    cmd_q = mq_open("/cleaner_cmd_q", O_RDONLY);
    
    /* Open the TX channel to the UI (Write Only) */
    tel_q = mq_open("/cleaner_tel_q", O_WRONLY);
    if (cmd_q == (mqd_t)-1 || tel_q == (mqd_t)-1) {
        return -1;
    }

    struct clean_cmd_msg_s cmd; 
    ssize_t nbytes;
    while (1) {
        nbytes = mq_receive(cmd_q, (char *)&cmd, sizeof(cmd), NULL);
        if (nbytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("mq_receive");
            break;
        }
        handle_ui_command(&cmd);
    }
    mq_close(cmd_q);
    mq_close(tel_q);
    return 0;

}


static int handle_ui_command(struct clean_cmd_msg_s *cmd) { 
    static struct clean_tel_msg_s res_from_motor = {
        .state = MOTOR_STATE_STOPPED,
        .time_remaining = 0,
        .current_duty = 0
    };
    switch (cmd->command) {
        case MSG_RUN:
            // Handle run command
            res_from_motor.state = MOTOR_STATE_RUNNING;
            res_from_motor.time_remaining = cmd->run_time_s;
            if (run_cleaning_cycle(cmd, &res_from_motor) != MC_SUCCESS) {
                // Handle error
            }
            else {
                mq_send(tel_q, (void*)&res_from_motor, sizeof(struct clean_tel_msg_s), 0);  
            }
            break;
        case MSG_RESUME:
            // Handle resume command
            res_from_motor.state = MOTOR_STATE_RUNNING;
            if (run_cleaning_cycle(cmd, &res_from_motor) != MC_SUCCESS) {
                // Handle error
            }
            else {
                mq_send(tel_q, (void*)&res_from_motor, sizeof(struct clean_tel_msg_s), 0);  
            }
            break;
        // The following cases are handled in the run_cleaning_cycle function, 
        // but we can also send a message back to the UI to indicate the state change
        case MSG_STOP_REQ:
        case MSG_PAUSE:
        case MSG_ABORT:
        case MSG_SET_DURATION:
        case MSG_SET_SPEED:
        case MSG_SET_AGITATE_DURATION:
            res_from_motor.state = MOTOR_MESSAGE;
            res_from_motor.message_id = MSG_MOTOR_READY; // Indicate that the motor is ready for a new command
            mq_send(tel_q, (void*)&res_from_motor, sizeof(struct clean_tel_msg_s), 0);  
            break;
        default:
            // Handle unknown command
            break;
    }   
    return 0;

}

static int timespec_compare(const struct timespec *a, const struct timespec *b)
{
    if (a->tv_sec < b->tv_sec) {
        return -1;
    }
    if (a->tv_sec > b->tv_sec) {
        return 1;
    }
    if (a->tv_nsec < b->tv_nsec) {
        return -1;
    }
    if (a->tv_nsec > b->tv_nsec) {
        return 1;
    }
    return 0;
}

static struct timespec get_min_deadline(struct timespec *t1, struct timespec *t2, struct timespec *t3)
{
    struct timespec min = *t1;
    if (timespec_compare(t2, &min) < 0) {
        min = *t2;
    }
    if (timespec_compare(t3, &min) < 0) {
        min = *t3;
    }
    return min;
}

static int run_cleaning_cycle(struct clean_cmd_msg_s *cmd, struct clean_tel_msg_s *res_from_motor)
{
    struct clean_cmd_msg_s async_msg;
    uint16_t target_duty = cmd->max_duty;
    uint16_t target_duty_saved;  
    uint16_t time_remaining = res_from_motor->time_remaining;
    uint16_t agitate_interval_s = cmd->agitate_interval_s;
    int current_duty = 0;
    bool keep_running = true;
    bool reverse_motor = false;

    struct timespec now, next_tick, end_time, next_motor_reverse_time;
    
    #if 1
    /* motor driver initialization */
    if (motor_driver_start_pwm() != 0) {
        return -1;
    }
    #endif
    
    // Next tick time
    (void)mc_get_abstime_from_now(&next_tick, MC_MS_PER_SEC); // 1 second   
    // Cleaning cycle end time from now 
    (void)mc_get_abstime_from_now(&end_time, time_remaining * MC_MS_PER_SEC);
    // Deadline time to reverse the motor from now
    (void)mc_get_abstime_from_now(&next_motor_reverse_time, agitate_interval_s * MC_MS_PER_SEC);

    while (keep_running || current_duty > 0) {

        /* Get the minimum deadline among the three */
        struct timespec deadline = get_min_deadline(&end_time, &next_motor_reverse_time, &next_tick);
        // This will block for deadline milliseconds and then continue if no message is received
        ssize_t bytes_received = mq_timedreceive(cmd_q, (char *)&async_msg, sizeof(async_msg), NULL, &deadline);
        
        // Get the current time
        (void)mc_get_now(&now);
        
        if (bytes_received > 0) {
            if (async_msg.command == MSG_STOP_REQ )
            {
                res_from_motor->state = MOTOR_STATE_STOPPED;
                res_from_motor->time_remaining = 0;
                target_duty = 0; // Trigger the "Soft Landing"
                keep_running = false; // Stop trying to run after we hit zero
            }    
            if ( async_msg.command == MSG_PAUSE) {
                res_from_motor->state = MOTOR_STATE_PAUSED;
                res_from_motor->time_remaining = time_remaining;
                target_duty = 0; // Trigger the "Soft Landing"
                keep_running = false; // Stop trying to run after we hit zero
            }
            if (async_msg.command == MSG_ABORT) {
                motor_driver_set_duty(0); // Hard stop for emergencies
                return MC_ABORTED;
            }
            // Handle set commands to dynamically change the cleaning cycle parameters
            if (async_msg.command == MSG_SET_DURATION) {
                time_remaining = async_msg.run_time_s;
                (void)mc_get_abstime_from_now(&end_time, time_remaining * MC_MS_PER_SEC);
            }
            if (async_msg.command == MSG_SET_SPEED) {
                target_duty = async_msg.max_duty;
            }
            if (async_msg.command == MSG_SET_AGITATE_DURATION) {
                agitate_interval_s = async_msg.agitate_interval_s;
                (void)mc_get_abstime_from_now(&next_motor_reverse_time, agitate_interval_s * MC_MS_PER_SEC);
            }
        }
        
        // Send time update to UI every tick 
        if (timespec_compare(&now, &next_tick) >= 0) {
            time_remaining = end_time.tv_sec - now.tv_sec; 
            res_from_motor->time_remaining = time_remaining; 
            mq_send(tel_q, (void*)res_from_motor, sizeof(struct clean_tel_msg_s), 0);
            // Update the tick timer, now + 1 sec.
            (void)mc_get_abstime_from_now(&next_tick, MC_MS_PER_SEC);
        }

        // Check if the runtime has been exceeded
        if (timespec_compare(&now, &end_time) >= 0) {
            /* We're done with the cleaning cycle, ramp down */
            keep_running = false;
            target_duty = 0;
            res_from_motor->state = MOTOR_STATE_STOPPED;
            res_from_motor->time_remaining = 0;
        }

        // Check if it's time to reverse the motor
        if (timespec_compare(&now, &next_motor_reverse_time) >= 0) {
            /* It's time to reverse the motor */
            reverse_motor = true;
            target_duty_saved = target_duty; // Save the current target duty
            target_duty = 0; // Trigger the "Soft Landing" before reversing
            // Update the deadline for the next motor reverse
            (void)mc_get_abstime_from_now(&next_motor_reverse_time, agitate_interval_s * MC_MS_PER_SEC);
        }


        /* Incremental Ramp Logic */
        if (current_duty < target_duty) {
            current_duty += MC_RAMP_STEP;
        } else if (current_duty > target_duty) {
            current_duty -= MC_RAMP_STEP; // This provides the soft landing
        }

        motor_driver_set_duty(current_duty);
        
        if (reverse_motor && (current_duty <= target_duty)) {
            motor_driver_reverse_motor_direction();
            // Restore the target duty after the motor has stopped
            target_duty = target_duty_saved;
            reverse_motor = false;
        }


    #if 0
        struct clean_tel_msg_s mot_msg;
        mot_msg.state = MOTOR_MESSAGE;
        mot_msg.message_id = MSG_MOTOR_NOT_RUNNING;
        mq_send(tel_q, (void*)&mot_msg, sizeof(struct clean_tel_msg_s), 0);
    #endif
        

    }
    return MC_SUCCESS;
}