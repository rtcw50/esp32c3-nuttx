#include <nuttx/config.h>
#include <stdio.h>
#include <nuttx/mqueue.h>
#include <time.h>
#include <errno.h>
#include "watch_cleaner_controller.h"

/* Global and Statics*/
static uint16_t g_duty_cycle;  // Current duty cycle percentage
static uint16_t g_run_time; // Current duration in seconds
static uint16_t g_rinse_time; // Current rinse duration in seconds
static uint16_t g_agitate_interval; // Current agitate duration in seconds
static uint16_t g_ramp_factor; // Current speed in RPM
static uint16_t g_spin_time; // Current spin duration in seconds
static mqd_t motor_recv_q;
static mqd_t motor_send_q;

/* Externs*/

/* Enums */
typedef enum CycleType {
    CLEAN_CYCLE,
    RINSE_CYCLE,
    SPIN_CYCLE
} CycleType;


/* Local Prototypes */
static int run_cycle(CycleType t, struct clean_tel_msg_s *response_from_motor);
static int handle_ui_command(struct clean_cmd_msg_s *cmd); 
static void motorcontroller_init(void);


static void motorcontroller_init() {
    // Initialize the motor controller
    if (wcc_motor_driver_init() != 0) {
        printf("watch_cleaner: Motor driver initialization failed\n");
    }
    else {
        printf("watch_cleaner: Motor driver initialized successfully\n");
    }
    g_run_time = CLEAN_DUR_DEFAULT;
    g_rinse_time = RINSE_DUR_DEFAULT;
    g_duty_cycle = MOTOR_MAX_DUTY/2;
    g_agitate_interval = AGITATE_DUR_DEFAULT;
    g_ramp_factor = SPIN_UP_DEFAULT;
    g_spin_time = SPIN_DUR_DEFAULT;
}

int wcc_motor_task(int argc, char *argv[])
{
    motorcontroller_init();

    /* Open the RX channel from the UI (Read Only) */
    /* Note: We keep this blocking, so the motor task sleeps until the UI sends a message  */
    motor_recv_q = mq_open("/cleaner_cmd_q", O_RDONLY);
    
    /* Open the TX channel to the UI (Write Only) */
    motor_send_q = mq_open("/cleaner_tel_q", O_WRONLY);
    if (motor_recv_q == (mqd_t)-1 || motor_send_q == (mqd_t)-1) {
        return -1;
    }
    printf("watch_cleaner: motor task started, waiting for commands...\n");

    struct clean_cmd_msg_s cmd; 
    ssize_t nbytes;
    while (1) {
        nbytes = mq_receive(motor_recv_q, (char *)&cmd, sizeof(cmd), NULL);
        if (nbytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("mq_receive");
            break;
        }
        printf("watch_cleaner: received command %d\n", cmd.msg_type);
        handle_ui_command(&cmd);
    }
    mq_close(motor_recv_q);
    mq_close(motor_send_q);
    return 0;

}


static int handle_ui_command(struct clean_cmd_msg_s *cmd) { 
    static struct clean_tel_msg_s res_from_motor = {
        .state = MOTOR_STATE_STOPPED,
        .time_remaining = 0
    };
    static CycleType current_cycle = CLEAN_CYCLE; // Default to clean cycle
    switch (cmd->msg_type) {
        case MSG_ACTION_CLEAN:
            // Handle clean command
            res_from_motor.state = MOTOR_STATE_RUNNING;
            res_from_motor.time_remaining = cmd->value;
            g_run_time = cmd->value; // Update global run time
            current_cycle = CLEAN_CYCLE; // Set current cycle to clean
            if (run_cycle(CLEAN_CYCLE, &res_from_motor) != MC_SUCCESS) {
                // Handle error
            }
            else {
                mq_send(motor_send_q, (void*)&res_from_motor, sizeof(struct clean_tel_msg_s), 0);  
            }
            break;
        case MSG_ACTION_RINSE:
            // Handle clean command
            res_from_motor.state = MOTOR_STATE_RUNNING;
            res_from_motor.time_remaining = cmd->value;
            g_run_time = cmd->value; // Update global run time
            current_cycle = RINSE_CYCLE; // Set current cycle to rinse
            if (run_cycle(RINSE_CYCLE, &res_from_motor) != MC_SUCCESS) {
                // Handle error
            }
            else {
                mq_send(motor_send_q, (void*)&res_from_motor, sizeof(struct clean_tel_msg_s), 0);  
            }
            break;
        case MSG_ACTION_SPIN:
            // Handle clean command
            res_from_motor.state = MOTOR_STATE_RUNNING;
            res_from_motor.time_remaining = cmd->value;
            g_run_time = cmd->value; // Update global run time
            current_cycle = SPIN_CYCLE; // Set current cycle to spin
            if (run_cycle(SPIN_CYCLE, &res_from_motor) != MC_SUCCESS) {
                // Handle error
            }
            else {
                mq_send(motor_send_q, (void*)&res_from_motor, sizeof(struct clean_tel_msg_s), 0);  
            }
            break;
        case MSG_ACTION_RESUME:
            // Handle resume command
            res_from_motor.state = MOTOR_STATE_RUNNING;
            if (run_cycle(current_cycle, &res_from_motor) != MC_SUCCESS) {
                // Handle error
            }
            else {
                mq_send(motor_send_q, (void*)&res_from_motor, sizeof(struct clean_tel_msg_s), 0);  
            }
            break;
        // The following cases are handled in the run_cleaning_cycle function, 
        // but we can also send a message back to the UI to indicate the state change
        case MSG_ACTION_STOP:
            res_from_motor.state = MOTOR_STATE_STOPPED;
            res_from_motor.time_remaining = 0;
            mq_send(motor_send_q, (void*)&res_from_motor, sizeof(struct clean_tel_msg_s), 0);  
            break;
        case MSG_ACTION_PAUSE:
            res_from_motor.state = MOTOR_STATE_PAUSED;
            res_from_motor.time_remaining = 0;
            mq_send(motor_send_q, (void*)&res_from_motor, sizeof(struct clean_tel_msg_s), 0);  
            break;
        case MSG_ACTION_ABORT:
            res_from_motor.state = MOTOR_STATE_ABORTED;
            res_from_motor.time_remaining = 0;
            mq_send(motor_send_q, (void*)&res_from_motor, sizeof(struct clean_tel_msg_s), 0);  
            break;
        case MSG_SET_RUNTIME:
            g_run_time = cmd->value;
            break;
        case MSG_SET_DUTY_CYCLE:
            g_duty_cycle = cmd->value;
            break;
        case MSG_SET_AGITATE_INTERVAL:
            g_agitate_interval = cmd->value;
            break;
        case MSG_SET_RAMP_FACTOR:
            g_ramp_factor = cmd->value;
            break; 
        case MSG_SET_SPIN_TIME: 
            g_spin_time = cmd->value; 
            break; 
        default: 
         LV_ASSERT_MSG(false, "Unknown command received in motor task"); 
         break; 
    }   
    return 0;
}


static int run_cycle(CycleType cycle_type, struct clean_tel_msg_s *response_from_motor)
{
    struct clean_cmd_msg_s async_msg;  // Message received from UI during cleaning cycle
    uint16_t target_duty = g_duty_cycle;
    uint16_t target_duty_saved;  
    uint16_t time_remaining = g_run_time; 
    uint16_t agitate_interval = g_agitate_interval; 
    uint16_t ramp_factor = g_ramp_factor; // Local copy of ramp factor for this cycle
    uint16_t spin_time = g_spin_time; // Local copy of spin time for this cycle

    int current_duty = 0;
    bool keep_running = true;
    bool reverse_motor = false;

    struct timespec now, next_tick, end_time, next_motor_reverse_time;

    if (cycle_type == SPIN_CYCLE) {
        // For spin cycle, we use the spin time instead of the run time
        time_remaining = spin_time;
        target_duty = MOTOR_MAX_DUTY; // For spin cycle, we want to max out the duty cycle
    }
    if (cycle_type == RINSE_CYCLE) {
        // For rinse cycle, we use the rinse time instead of the run time
        time_remaining = g_rinse_time;
    }
    
    /* start the motor, duty is 0 via initialization */
    if (wcc_motor_driver_start_pwm() != 0) {
        return -1;
    }
    
    // Next tick time
    (void)wcc_get_abstime_from_now(&next_tick, MC_MS_PER_SEC); // 1 second   
    // Cleaning cycle end time from now 
    (void)wcc_get_abstime_from_now(&end_time, time_remaining * MC_MS_PER_SEC);
    // Deadline time to reverse the motor from now
    (void)wcc_get_abstime_from_now(&next_motor_reverse_time, agitate_interval * MC_MS_PER_SEC);

    while (keep_running || current_duty > 0) {

        /* Get the minimum deadline among the three */
        // CHECK THIS
        //struct timespec deadline = wcc_get_min_deadline(&end_time, &next_motor_reverse_time, &next_tick);
        
        // Use a shorter timeout and yield to allow GUI task to run
        struct timespec now_deadline;
        wcc_get_now(&now_deadline);
        now_deadline.tv_nsec += 10000000; // 10ms timeout for checking messages
        
        // This will block for deadline milliseconds and then continue if no message is received
        ssize_t bytes_received = mq_timedreceive(motor_recv_q, (char *)&async_msg, sizeof(async_msg), NULL, &now_deadline);
        
        // Get the current time
        (void)wcc_get_now(&now);
        
        if (bytes_received > 0) {
            switch(async_msg.msg_type) {
                case MSG_ACTION_STOP:
                    response_from_motor->state = MOTOR_STATE_STOPPED;
                    response_from_motor->time_remaining = 0;
                    target_duty = 0; // Trigger the "Soft Landing"
                    keep_running = false; // Stop trying to run after we hit zero
                    break;
                case MSG_ACTION_PAUSE:
                    response_from_motor->state = MOTOR_STATE_PAUSED;
                    response_from_motor->time_remaining = time_remaining;
                    target_duty = 0; // Trigger the "Soft Landing"
                    keep_running = false; // Stop trying to run after we hit zero
                    break;
                case MSG_ACTION_ABORT:
                    wcc_motor_driver_set_duty(0); // Hard stop for emergencies
                    return MC_ABORTED;
                case MSG_SET_RUNTIME:
                    time_remaining = async_msg.value;
                    (void)wcc_get_abstime_from_now(&end_time, time_remaining * MC_MS_PER_SEC);
                    break;
                case MSG_SET_DUTY_CYCLE:
                    target_duty = async_msg.value;
                    break;
                case MSG_SET_AGITATE_INTERVAL:
                    agitate_interval = async_msg.value;
                    (void)wcc_get_abstime_from_now(&next_motor_reverse_time, agitate_interval * MC_MS_PER_SEC);
                    break;
                case MSG_SET_RAMP_FACTOR:
                    ramp_factor = async_msg.value;
                    break;
                case MSG_SET_SPIN_TIME:
                    spin_time = async_msg.value;
                    break;
                default:
                    LV_ASSERT_MSG(false, "Invalid command received in run_cycle");
            }
        }
        
        // Send time update to UI every tick 
        if (wcc_timespec_compare(&now, &next_tick) >= 0) {
            time_remaining = end_time.tv_sec - now.tv_sec; 
            response_from_motor->time_remaining = time_remaining; 
            mq_send(motor_send_q, (void*)response_from_motor, sizeof(struct clean_tel_msg_s), 0);
            // Update the tick timer, now + 1 sec.
            (void)wcc_get_abstime_from_now(&next_tick, MC_MS_PER_SEC);
        }

        // Check if the runtime has been exceeded
        if (wcc_timespec_compare(&now, &end_time) >= 0) {
            /* We're done with the cleaning cycle, ramp down */
            keep_running = false;
            target_duty = 0;
            response_from_motor->state = MOTOR_STATE_STOPPED;
            response_from_motor->time_remaining = 0;
        }

        // Check if it's time to reverse the motor
        if (wcc_timespec_compare(&now, &next_motor_reverse_time) >= 0) {
            /* It's time to reverse the motor */
            reverse_motor = true;
            target_duty_saved = target_duty; // Save the current target duty
            target_duty = 0; // Trigger the "Soft Landing" before reversing
            // Update the deadline for the next motor reverse
            (void)wcc_get_abstime_from_now(&next_motor_reverse_time, agitate_interval * MC_MS_PER_SEC);
        }


        /* Incremental Ramp Logic */
        if (current_duty < target_duty) {
            current_duty += ramp_factor; // This provides the soft ramp up
        } else if (current_duty > target_duty) {
            current_duty -= ramp_factor; // This provides the soft landing
        }

        wcc_motor_driver_set_duty(current_duty);
        
        // Reverse the motor for clean and rinse cycles, but not for spin cycles
        if (reverse_motor && (current_duty <= target_duty) && (cycle_type != SPIN_CYCLE)) {
            wcc_motor_driver_reverse_motor_direction();
            // Restore the target duty after the motor has stopped
            target_duty = target_duty_saved;
            reverse_motor = false;
        }

        // Yield CPU time to allow GUI task to run
        usleep(10000); // 10ms yield

    #if 0
        struct clean_tel_msg_s mot_msg;
        mot_msg.state = MOTOR_MESSAGE;
        mot_msg.message_id = MSG_MOTOR_NOT_RUNNING;
        mq_send(motor_send_q, (void*)&mot_msg, sizeof(struct clean_tel_msg_s), 0);
    #endif
        

    }
    return MC_SUCCESS;
}