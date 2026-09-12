#include <nuttx/config.h>
#include <stdio.h>
#include <nuttx/mqueue.h>
#include <nuttx/signal.h>
#include <time.h>
#include <errno.h>
#include "watch_cleaner_controller.h"


/* Global and Statics*/
static mqd_t motor_recv_q;    
static mqd_t motor_send_q;

/* Externs*/



/* Local Prototypes */
static int handle_ui_command(struct clean_cmd_msg_s *cmd); 
static void motorcontroller_init(void);

static void motorcontroller_init() {
    // Initialize the motor controller
    if (wcc_motor_driver_init() != 0) {
        printf("watch_cleaner: Motor driver initialization failed\n");
    }
}

int wcc_motor_task(int argc, char *argv[])
{
    motorcontroller_init();

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
    /* Open the RX channel from the UI (Read Only) */
    /* Note: We keep this blocking, so the motor task sleeps until the UI/Timers sends a message  */
    motor_recv_q = mq_open("/cleaner_cmd_q", O_RDONLY, 0666, &cleaner_cmd_attr);
    
    /* Open the TX channel to the UI (Write Only) */
    motor_send_q = mq_open("/cleaner_tel_q", O_WRONLY|O_NONBLOCK, 0666, &cleaner_tel_attr);
    
    if (motor_recv_q == (mqd_t)-1 || motor_send_q == (mqd_t)-1) {
        return -1;
    }
    //printf("watch_cleaner: motor task started, waiting for commands...\n");

    struct clean_cmd_msg_s cmd; 
    ssize_t nbytes;
    while (1) {
        nbytes = mq_receive(motor_recv_q, (char *)&cmd, sizeof(cmd), NULL);
        if (nbytes > 0) {
            //printf("watch_cleaner: received command %d\n", cmd.msg_type);
            handle_ui_command(&cmd);
        } 
    }
    mq_close(motor_recv_q);
    mq_close(motor_send_q);
    return 0;

}

static int handle_ui_command(struct clean_cmd_msg_s *cmd)                            
{                                                                                    
    switch (cmd->msg_type) {
        /* --- UI Commands --- */                                                    
        case MSG_ACTION_PWM_START:
            wcc_motor_driver_start_pwm();
            break;

        case MSG_ACTION_PWM_STOP:
            wcc_motor_driver_stop_pwm();
            break;
                                                                                     
        case MSG_ACTION_PWM_SET_DUTY:
            wcc_motor_driver_set_duty(cmd->value);
            break;

        case MSG_ACTION_PWM_SET_DUTY_ALL:
            wcc_motor_driver_set_duty_all(cmd->value);
            break;

        case MSG_ACTION_PWM_REVERSE:
            wcc_motor_driver_reverse_motor_direction();
            break; 

        case MSG_ACTION_PWM_ABORT:                                                       
            wcc_motor_driver_set_duty(0);
            wcc_motor_driver_stop_pwm();
            break;

        default:                                                                     
            break;                                                                   
    }                                                                                
    return 0;                                                                        
}                                                                                    



