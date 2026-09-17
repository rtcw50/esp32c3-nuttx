/*
 * File: wcc_calibration.cpp
 * Author: John
 * Date: 2026-Sep-14
 * Description:
 *   Implementation of the touchscreen calibration screen for the
 *   Watch Cleaner Controller (WCC).
 */

#include <nuttx/config.h>
#include <stdio.h>
#include <lvgl/lvgl.h>
#include "lvgl/src/misc/lv_timer.h"
#include "lvgl/src/widgets/label/lv_label.h"
#include <sys/mount.h>
#include "watch_cleaner_controller.h"

/* External Prototypes */
extern "C" {
bool  esp32c3_xiao_calibrate_touch_point(uint16_t * corner_point);
bool  esp32c3_xiao_finalize_calibration(uint16_t *tl, uint16_t *tr, uint16_t *bl, uint16_t *br, bool invert_x, bool invert_y, uint16_t *caldata);
void  esp32c3_xiao_set_touch_calibration(uint16_t * cal_data);
} 
#define CALIBRATION_TARGET_SIZE 10

typedef enum CalibrationState {
    CALIBRATION_IDLE,
    CALIBRATION_WAITING_FOR_TOP_LEFT,
    CALIBRATION_WAITING_FOR_TOP_RIGHT,
    CALIBRATION_WAITING_FOR_BOTTOM_LEFT,
    CALIBRATION_WAITING_FOR_BOTTOM_RIGHT,
    CALIBRATION_COMPLETE,
    CALIBRATION_SHUTDOWN,
    CALIBRATION_FAILED
} CalibrationState;

typedef struct
{
    lv_obj_t *top_left;
    lv_obj_t *top_right;
    lv_obj_t *bottom_left;
    lv_obj_t *bottom_right;
} calibration_screen_items_t;

/* Globals/statics */
static lv_obj_t *calibration_screen;
static lv_timer_t * g_calibration_timer;
static CalibrationState calibration_state = CALIBRATION_IDLE; 
static lv_color_t target_color_red = lv_color_make(255,0,0);
static lv_color_t target_color_blue = lv_color_make(0,0,255);
static calibration_screen_items_t cal_screen_items;

/* File system has been initialized */
extern bool g_filesystem_ok;
extern bool g_calibration_ok;

/* Local Prototypes */
static bool save_calibration_data(uint16_t * cal_data);



/* Create one visible calibration target. No event callback is attached yet. */
static lv_obj_t *create_calibration_target(lv_obj_t *parent,
                                           lv_align_t alignment, bool is_touch_target)
{
    lv_obj_t *target = lv_btn_create(parent);
    /* To help guide the user to which calibration square to touch */
    lv_color_t target_color = is_touch_target ? target_color_blue : target_color_red;


    /*
     * Remove the normal button appearance. The object remains a button,
     * but is currently only used as a visible calibration target.
     */
    lv_obj_remove_style_all(target);

    lv_obj_set_size(target,
                    CALIBRATION_TARGET_SIZE,
                    CALIBRATION_TARGET_SIZE);

    lv_obj_set_style_bg_opa(target, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(target,
                              target_color,
                              LV_PART_MAIN);
    lv_obj_set_style_border_width(target, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(target, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(target, 0, LV_PART_MAIN);

    /*
     * Align the target by its outer bounds. For a 10x10 target on a
     * 320x240 display, its center coordinates are approximately:
     *
     *   Top-left:     (5,   5)
     *   Top-right:    (315, 5)
     *   Bottom-left:  (5,   235)
     *   Bottom-right: (315, 235)
     */
    lv_obj_align(target, alignment, 0, 0);

    return target;
}

/* Public functions */
extern "C" void wcc_create_calibration_screen(void)
{
    if (calibration_screen != NULL)
    {
        return;
    }

    /*
     * Create an independent LVGL screen. This function does not load the
     * screen; use wcc_show_calibration_screen() when it should be displayed.
     */
    calibration_screen = lv_obj_create(NULL);

    lv_obj_remove_style_all(calibration_screen);
    lv_obj_set_style_bg_opa(calibration_screen,
                            LV_OPA_COVER,
                            LV_PART_MAIN);
    lv_obj_set_style_bg_color(calibration_screen,
                              lv_color_black(),
                              LV_PART_MAIN);
    lv_obj_set_style_pad_all(calibration_screen, 0, LV_PART_MAIN);

    lv_obj_t *instruction_label = lv_label_create(calibration_screen);
    lv_label_set_text(instruction_label,
                      "Touch Screen Calibration\n"
                      "Touch the highlighted square");
    lv_obj_set_style_text_color(instruction_label,
                                lv_color_white(),
                                LV_PART_MAIN);
    lv_obj_set_style_text_align(instruction_label,
                                LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN);
    lv_obj_center(instruction_label);

    cal_screen_items.top_left =
        create_calibration_target(calibration_screen,
                                  LV_ALIGN_TOP_LEFT, true);

    cal_screen_items.top_right =
        create_calibration_target(calibration_screen,
                                  LV_ALIGN_TOP_RIGHT, false);

    cal_screen_items.bottom_left =
        create_calibration_target(calibration_screen,
                                  LV_ALIGN_BOTTOM_LEFT, false);

    cal_screen_items.bottom_right =
        create_calibration_target(calibration_screen,
                                  LV_ALIGN_BOTTOM_RIGHT, false);
}

extern "C" lv_obj_t *wcc_get_calibration_screen(void)
{
    return calibration_screen;
}

/*
* The four calibration points must be touched in this order:
*
*   0: top-left
*   1: top-right
*   2: bottom-left
*   3: bottom-right
*/
static void calibration_timer_cb(lv_timer_t * timer)
{
    //printf("enter calibration timer cb\n");
    static uint16_t tl[2], tr[2], bl[2], br[2];
    bool invert_x, invert_y;
    CalibrationState * lcalstate = (CalibrationState *)lv_timer_get_user_data(timer);
    switch (*lcalstate) {
        case CALIBRATION_IDLE:
            *lcalstate = CALIBRATION_WAITING_FOR_TOP_LEFT;
            break;
        case CALIBRATION_WAITING_FOR_TOP_LEFT:
            //printf("waiting for top left\n");
            esp32c3_xiao_calibrate_touch_point(tl) ? 
            *lcalstate = CALIBRATION_WAITING_FOR_TOP_RIGHT :
            *lcalstate = CALIBRATION_FAILED;
            /* Advance the touch target */
            lv_obj_set_style_bg_color(cal_screen_items.top_left,
                              target_color_red,
                              LV_PART_MAIN);
            lv_obj_set_style_bg_color(cal_screen_items.top_right,
                              target_color_blue,
                              LV_PART_MAIN);
            break;
        case CALIBRATION_WAITING_FOR_TOP_RIGHT:
            esp32c3_xiao_calibrate_touch_point(tr) ?
            *lcalstate = CALIBRATION_WAITING_FOR_BOTTOM_LEFT :
            *lcalstate = CALIBRATION_FAILED;
            lv_obj_set_style_bg_color(cal_screen_items.top_right,
                              target_color_red,
                              LV_PART_MAIN);
            lv_obj_set_style_bg_color(cal_screen_items.bottom_left,
                              target_color_blue,
                              LV_PART_MAIN);
            break;
        case CALIBRATION_WAITING_FOR_BOTTOM_LEFT:
            esp32c3_xiao_calibrate_touch_point(bl) ?
            *lcalstate = CALIBRATION_WAITING_FOR_BOTTOM_RIGHT:
            *lcalstate = CALIBRATION_FAILED;
            lv_obj_set_style_bg_color(cal_screen_items.bottom_left,
                              target_color_red,
                              LV_PART_MAIN);
            lv_obj_set_style_bg_color(cal_screen_items.bottom_right,
                              target_color_blue,
                              LV_PART_MAIN);
            break;
        case CALIBRATION_WAITING_FOR_BOTTOM_RIGHT:
            esp32c3_xiao_calibrate_touch_point(br) ?
            *lcalstate = CALIBRATION_COMPLETE :
            *lcalstate = CALIBRATION_FAILED;
            lv_obj_set_style_bg_color(cal_screen_items.bottom_right,
                              target_color_red,
                              LV_PART_MAIN);
            break;
        case CALIBRATION_COMPLETE:
            uint16_t cal_data[4];
            invert_x = ((br[0] - tl[0]) < 0);
            invert_y = ((bl[1] - tr[1]) < 0);
            esp32c3_xiao_finalize_calibration(tl,tr,bl,br,invert_x, invert_y,cal_data) ?
            *lcalstate = CALIBRATION_SHUTDOWN :
            *lcalstate = CALIBRATION_FAILED ;
            
            if (!save_calibration_data(cal_data)) {
                *lcalstate = CALIBRATION_FAILED;
            }
            break;
        case CALIBRATION_SHUTDOWN:
            lv_timer_delete(timer);
            g_calibration_timer = NULL;
            g_calibration_ok = true;
            lv_screen_load_anim(wcc_get_main_screen(), LV_SCR_LOAD_ANIM_OVER_TOP, 500 , 10,false);
            break;
        case CALIBRATION_FAILED:
            /* fallthough */
        default:
            lv_timer_delete(timer);
            g_calibration_timer = NULL;
            g_calibration_ok = false;
            break;
    }
}

/* Save calibration data to a file */
static bool save_calibration_data(uint16_t * cal_data)
{
    FILE *file = fopen(CAL_DATA_PATH, "wb");
    if (file) {
        fwrite(cal_data, sizeof(uint16_t)*4, 1, file);
        fclose(file);
        return true;
    } else {
        return false;
    }
}

static bool load_calibration_data(void)
{
    uint16_t cal_data[4];
    if (!g_filesystem_ok) return false;

    FILE *file = fopen(CAL_DATA_PATH, "rb");
    if (file) {
        fread(cal_data, sizeof(uint16_t)*4, 1, file);
        fclose(file);
    } else {
        return false;
    }
    esp32c3_xiao_set_touch_calibration(cal_data);
    return true;
}

extern "C" void wcc_calibration_setup(bool force_calibration)
{
    /* Do the calibration if no calibration data or forced calibration */
    if (force_calibration || (load_calibration_data() == false)) {
        /* Become the active screen */
        lv_screen_load_anim(calibration_screen, LV_SCR_LOAD_ANIM_OVER_TOP, 500 , 10,false);

        /* Set up timer callbacks to do the calibration sequences */
        g_calibration_timer = lv_timer_create(calibration_timer_cb, 100, &calibration_state);
    } 
    else {
        /* Normal main screen startup */
        lv_screen_load_anim(wcc_get_main_screen(), LV_SCR_LOAD_ANIM_OVER_TOP, 500 , 10,false);
    }
}
