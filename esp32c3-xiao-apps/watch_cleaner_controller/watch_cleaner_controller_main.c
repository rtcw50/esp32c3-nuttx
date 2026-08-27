#include <nuttx/config.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <nuttx/timers/pwm.h>
#include <unistd.h>
#include <mqueue.h>
#include <time.h>
#include "watch_cleaner_controller.h"

#define WCC_STANDALONE

int watch_cleaner_controller_main(int argc, char *argv[])
{
    /* 1. Board initialization if not executed as a nsh nuttx application */
#ifdef WCC_STANDALONE
    if (board_app_initialize(0) < 0) {
        printf("watch_cleaner: board_app_initialize failed\n");
        return -1;
    }   
    printf("watch_cleaner: board initialized\n");
#endif

/*  Pre-create the Command Queue (UI -> Motor) */
static const struct mq_attr cleaner_cmd_attr = {
    .mq_maxmsg = 4,
    .mq_msgsize = sizeof(struct clean_cmd_msg_s),
    .mq_flags = 0
};

mq_unlink("/cleaner_cmd_q"); /* ignore ENOENT */
mqd_t setup_cmd = mq_open("/cleaner_cmd_q", O_CREAT | O_RDWR, 0666, &cleaner_cmd_attr);
if (setup_cmd == (mqd_t)-1)
{
    perror("mq_open /cleaner_cmd_q");
}
else
{
    mq_close(setup_cmd);
}

/* Pre-create the Telemetry Queue (Motor -> UI) */
static const struct mq_attr cleaner_tel_attr = {
    .mq_maxmsg = 4,
    .mq_msgsize = sizeof(struct clean_tel_msg_s),
    .mq_flags = 0
};
mq_unlink("/cleaner_tel_q");
mqd_t setup_tel = mq_open("/cleaner_tel_q", O_CREAT | O_RDWR, 0666, &cleaner_tel_attr);
if (setup_tel == (mqd_t)-1)
{
    perror("mq_open /cleaner_tel_q");
}
else
{
    mq_close(setup_tel);
}



 task_create("motor_task", 150, 16384    , wcc_motor_task, NULL);
 task_create("gui_task", 100, 4096, wcc_gui_task, NULL);

 return 0;
}
