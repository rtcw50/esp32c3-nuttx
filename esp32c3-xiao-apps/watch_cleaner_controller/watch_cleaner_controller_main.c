#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <nuttx/ioexpander/gpio.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <nuttx/timers/pwm.h>
#include <unistd.h>
#include <mqueue.h>
#include <time.h>
#include <sys/mount.h>
#include "watch_cleaner_controller.h"

/* Run as a standalone app, not via nsh */
#define WCC_STANDALONE

/* Define system wide globals here */
bool g_filesystem_ok;
bool g_calibration_ok;


void init_application_storage(void)
{
    g_filesystem_ok = false;

    // 1. Attempt to mount the LittleFS partition using the POSIX mount interface
    // Parameters: Source Block Device, Target Mount point, File system type, Mount flags, Data pointer
    int ret = mount(LFS_DEV_PATH, LFS_MOUNT_POINT, "littlefs", 0, NULL);
    
    if (ret < 0)
    {
        // Retry the mount after an automatic format pass
        ret = mount(LFS_DEV_PATH, LFS_MOUNT_POINT, "littlefs", 0, "autoformat");
    }

    if (ret == 0)
    {
        g_filesystem_ok = true;
    }
}

static void delete_calibration_file(void)
{
    FILE *file = fopen(CAL_DATA_PATH, "rb");
    if (file) {
        remove(CAL_DATA_PATH);
    }
}

/* If unused GPIO6 is pulled low, then set the force calibration flag */
static void check_recalibration_pin(void)
{
    int ret;
    bool pinval;

    int recal_pin = open("/dev/gpio6", O_RDWR);
    if (recal_pin < 0) {
        goto no_recal;
    }

    ret = ioctl(recal_pin, GPIOC_SETPINTYPE, GPIO_INPUT_PIN|GPIO_INPUT_PIN_PULLUP);
    if (ret < 0) {
        goto no_recal;
    }
    ret = ioctl(recal_pin, GPIOC_READ, &pinval);
    if (ret < 0) {
        goto no_recal;
    }
    /* Check for low value */
    if (pinval == false) {
        /* Delete current calibration file if it exists, this will force a recalibration*/
        delete_calibration_file();
    }
    close(recal_pin);
    return;

no_recal:
    return;
}


int watch_cleaner_controller_main(int argc, char *argv[])
{
    /* 1. Board initialization if not executed as a nsh nuttx application */
#ifdef WCC_STANDALONE
    if (board_app_initialize(0) < 0) {
        //printf("watch_cleaner: board_app_initialize failed\n");
        return -1;
    }   
    //printf("watch_cleaner: board initialized\n");
#endif

init_application_storage();

/*  Pre-create the Command Queue (UI -> Motor) */
static const struct mq_attr cleaner_cmd_attr = {
    .mq_maxmsg = 10,
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
    .mq_maxmsg = 10,
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

/* Check the status of GPIO6, if pulled low, force screen re-calibration */
check_recalibration_pin();

 /* Start GUI and motor controller tasks */
 task_create("motor_task", 100, 8192, wcc_motor_task, NULL);
 task_create("gui_task", 150, 8192, wcc_gui_task, NULL);
 return 0;
}
