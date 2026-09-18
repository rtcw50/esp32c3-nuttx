/*
 * File: wcc_main_screen.cpp
 * Description:
 *   GUI widgets for the main screen.
 */

#include <nuttx/config.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <nuttx/mqueue.h>
#include <lvgl/lvgl.h>
#include <unistd.h>
#include "watch_cleaner_controller.h"

#define PLAYPAUSE_SYMBOL LV_SYMBOL_PLAY LV_SYMBOL_PAUSE

extern lv_style_t on_button_style;
extern lv_style_t off_button_style;
extern lv_style_t stop_button_style;
extern lv_style_t transparent_button_style;
extern lv_style_t style_radio_button_container;
extern lv_style_t style_radio;
extern lv_style_t style_radio_chk;

static lv_timer_t *g_one_sec_timer;
static int32_t g_countdown_in_sec;
static int16_t g_current_duty;
static int16_t g_saved_duty;
static lv_obj_t *main_screen;
static mqd_t ui_send_q;
static mqd_t ui_receive_q;
static int32_t g_operating_mode;
static int32_t g_duration;
static int32_t g_agitate_interval_duration;

static const char *Mach_Status_Text_Stopped = "Stopped ...";
static const char *Mach_Status_Text_Running = "Running ...";
static const char *Mach_Status_Text_Paused = "Paused ...";
static char Mach_Status_Text_Time_Remaining[7] = "      ";

static lv_obj_t *start_button;
static lv_obj_t *stop_button;
static lv_obj_t *settings_button;
static lv_obj_t *mach_status_label;
static lv_obj_t *time_remaining_label;

static void format_and_publish_time_and_status(uint16_t time_remaining,
                                               const char *motor_status)
{
    uint32_t minutes = time_remaining / 60;
    uint32_t seconds = time_remaining % 60;

    if (time_remaining == 0) {
        lv_label_set_text(time_remaining_label, "");
        return;
    }

    lv_snprintf(Mach_Status_Text_Time_Remaining,
                sizeof(Mach_Status_Text_Time_Remaining),
                "%03lu:%02lu",
                (unsigned long)minutes,
                (unsigned long)seconds);

    lv_label_set_text(time_remaining_label,
                      Mach_Status_Text_Time_Remaining);
    lv_label_set_text(mach_status_label, motor_status);
}

static void clear_time_remaining_label(void *user_data)
{
    (void)user_data;
    lv_label_set_text(time_remaining_label, "");
}

static void send_motor_sequence(int16_t target_duty)
{
    int16_t ramp_factor = (int16_t)get_ramp_factor();

    if (ramp_factor < 1) {
        ramp_factor = 1;
    }

    uint16_t difference = target_duty > g_current_duty
                              ? target_duty - g_current_duty
                              : g_current_duty - target_duty;
    int step = (difference + ramp_factor - 1) / ramp_factor;

    if (step < 1) {
        step = 1;
    }

    while (g_current_duty < target_duty) {
        g_current_duty += step;
        if (g_current_duty > target_duty) {
            g_current_duty = target_duty;
        }

        usleep(1000);
        ui_send_cmd(&ui_send_q, MSG_ACTION_PWM_SET_DUTY,
                    (uint16_t)g_current_duty);
    }

    while (g_current_duty > target_duty) {
        g_current_duty -= step;
        if (g_current_duty < target_duty) {
            g_current_duty = target_duty;
        }

        usleep(1000);
        ui_send_cmd(&ui_send_q, MSG_ACTION_PWM_SET_DUTY,
                    (uint16_t)g_current_duty);
    }
}

static void shutdown_cycle(void)
{
    lv_obj_clear_state(start_button, LV_STATE_CHECKED);
    lv_label_set_text(lv_obj_get_child(start_button, 0), PLAYPAUSE_SYMBOL);

    if (g_one_sec_timer != NULL) {
        lv_timer_delete(g_one_sec_timer);
        g_one_sec_timer = NULL;
    }

    send_motor_sequence(0);
    lv_label_set_text(mach_status_label, Mach_Status_Text_Stopped);
    clear_time_remaining_label(NULL);
}

static void agitate_interval_reverse_sequence(void)
{
    send_motor_sequence(0);
    ui_send_cmd(&ui_send_q, MSG_ACTION_PWM_REVERSE, 0);
    send_motor_sequence(get_duty_cycle());
    g_agitate_interval_duration = get_agitate_interval_duration();
}

static void one_sec_timer_expiry_cb(lv_timer_t *timer)
{
    (void)timer;

    if (g_current_duty != get_duty_cycle()) {
        send_motor_sequence(get_duty_cycle());
    }

    if (g_duration != get_duration(g_operating_mode)) {
        g_duration = get_duration(g_operating_mode);
        g_countdown_in_sec = g_duration;
    }

    g_agitate_interval_duration = get_agitate_interval_duration();

    if (g_countdown_in_sec == 0) {
        shutdown_cycle();
        return;
    }

    if (g_agitate_interval_duration > 0 &&
        g_countdown_in_sec % g_agitate_interval_duration == 0 &&
        g_countdown_in_sec >= g_agitate_interval_duration &&
        g_operating_mode != OperatingMode::spin &&
        g_countdown_in_sec != g_duration) {
        agitate_interval_reverse_sequence();
    }

    format_and_publish_time_and_status((uint16_t)g_countdown_in_sec,
                                       Mach_Status_Text_Running);
    g_countdown_in_sec--;
}

static void start_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) {
        return;
    }

    lv_obj_t *button = lv_event_get_target_obj(event);
    lv_obj_t *label = lv_obj_get_child(button, 0);
    const char *label_text = lv_label_get_text(label);

    if (strcmp(label_text, PLAYPAUSE_SYMBOL) == 0) {
        lv_label_set_text(label, LV_SYMBOL_PAUSE);
        ui_send_cmd(&ui_send_q, MSG_ACTION_PWM_START, 0);

        g_duration = get_duration(g_operating_mode);
        g_agitate_interval_duration = get_agitate_interval_duration();
        g_countdown_in_sec = g_duration;
        g_one_sec_timer =
            lv_timer_create(one_sec_timer_expiry_cb, MC_MS_PER_SEC, NULL);
    } else if (strcmp(label_text, LV_SYMBOL_PAUSE) == 0) {
        lv_label_set_text(label, LV_SYMBOL_PLAY);
        if (g_one_sec_timer != NULL) {
            lv_timer_pause(g_one_sec_timer);
        }

        g_saved_duty = g_current_duty;
        send_motor_sequence(0);
        format_and_publish_time_and_status((uint16_t)g_countdown_in_sec,
                                           Mach_Status_Text_Paused);
    } else if (strcmp(label_text, LV_SYMBOL_PLAY) == 0) {
        lv_label_set_text(label, LV_SYMBOL_PAUSE);
        if (g_one_sec_timer != NULL) {
            lv_timer_resume(g_one_sec_timer);
        }
        send_motor_sequence(g_saved_duty);
    }
}

static lv_obj_t *create_start_button(lv_obj_t *screen)
{
    start_button = lv_btn_create(screen);
    lv_obj_remove_style_all(start_button);

    lv_obj_t *label = lv_label_create(start_button);
    lv_label_set_text(label, PLAYPAUSE_SYMBOL);
    lv_obj_center(label);

    lv_obj_set_size(start_button, 60, 60);
    lv_obj_align(start_button, LV_ALIGN_TOP_LEFT, 30, 45);
    lv_obj_add_flag(start_button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_remove_flag(start_button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_style(start_button, &on_button_style, LV_STATE_DEFAULT);
    lv_obj_add_style(start_button, &off_button_style, LV_STATE_CHECKED);
    lv_obj_add_event_cb(start_button, start_button_event_cb,
                        LV_EVENT_ALL, NULL);

    return start_button;
}

static void stop_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    g_countdown_in_sec = 0;
    if (g_one_sec_timer != NULL && lv_timer_get_paused(g_one_sec_timer)) {
        lv_timer_resume(g_one_sec_timer);
    }
}

static lv_obj_t *create_stop_button(lv_obj_t *screen)
{
    stop_button = lv_btn_create(screen);
    lv_obj_remove_style_all(stop_button);

    lv_obj_t *label = lv_label_create(stop_button);
    lv_label_set_text(label, LV_SYMBOL_STOP);
    lv_obj_center(label);

    lv_obj_set_size(stop_button, 60, 60);
    lv_obj_align_to(stop_button, start_button,
                    LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_remove_flag(stop_button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_style(stop_button, &stop_button_style, LV_STATE_DEFAULT);
    lv_obj_add_event_cb(stop_button, stop_button_event_cb,
                        LV_EVENT_ALL, NULL);

    return stop_button;
}

static void settings_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        lv_screen_load_anim(wcc_get_settings_screen(),
                            LV_SCR_LOAD_ANIM_OVER_TOP, 500, 10, false);
    }
}

static lv_obj_t *create_settings_button(lv_obj_t *screen)
{
    settings_button = lv_btn_create(screen);
    lv_obj_remove_style_all(settings_button);

    lv_obj_t *label = lv_label_create(settings_button);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS);
    lv_obj_center(label);

    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_set_size(settings_button, 60, 60);
    lv_obj_align(settings_button, LV_ALIGN_BOTTOM_RIGHT, 9, 0);
    lv_obj_remove_flag(settings_button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_style(settings_button, &transparent_button_style, 0);
    lv_obj_add_event_cb(settings_button, settings_button_event_cb,
                        LV_EVENT_ALL, NULL);

    return settings_button;
}

static void radio_event_handler(lv_event_t *event)
{
    int32_t *active_id =
        (int32_t *)lv_event_get_user_data(event);
    lv_obj_t *container = static_cast<lv_obj_t *>(lv_event_get_current_target(event));
    lv_obj_t *active_checkbox = lv_event_get_target_obj(event);

    if (active_checkbox == container) {
        return;
    }

    lv_obj_t *old_checkbox =
        lv_obj_get_child(container, *active_id);
    lv_obj_remove_state(old_checkbox, LV_STATE_CHECKED);
    lv_obj_add_state(active_checkbox, LV_STATE_CHECKED);

    *active_id = lv_obj_get_index(active_checkbox);
    g_operating_mode = *active_id - 1;
}

static void radio_button_create(lv_obj_t *parent, const char *text)
{
    lv_obj_t *object = lv_checkbox_create(parent);
    lv_checkbox_set_text(object, text);
    lv_obj_add_flag(object, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_style(object, &style_radio, LV_PART_INDICATOR);
    lv_obj_add_style(object, &style_radio_chk,
                     LV_PART_INDICATOR | LV_STATE_CHECKED);
}

static lv_obj_t *create_mode_selector(lv_obj_t *screen)
{
    static int32_t active_index = 1;
    lv_obj_t *container = lv_obj_create(screen);
    lv_obj_t *title = lv_label_create(container);

    lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_label_set_text(title, "Mode Select");
    lv_obj_align_to(container, start_button,
                    LV_ALIGN_OUT_RIGHT_TOP, 50, 0);
    lv_obj_add_style(container, &style_radio_button_container, 0);
    lv_obj_add_event_cb(container, radio_event_handler,
                        LV_EVENT_CLICKED, &active_index);

    radio_button_create(container, "Clean");
    radio_button_create(container, "Rinse");
    radio_button_create(container, "Spin");
    lv_obj_add_state(lv_obj_get_child(container, 1), LV_STATE_CHECKED);

    g_operating_mode = OperatingMode::clean;
    return container;
}

static void create_machine_status(lv_obj_t *screen)
{
    mach_status_label = lv_label_create(screen);
    lv_obj_set_size(mach_status_label, 100, 25);
    lv_obj_align(mach_status_label, LV_ALIGN_TOP_LEFT, 15, 215);
    lv_label_set_text(mach_status_label, Mach_Status_Text_Stopped);

    time_remaining_label = lv_label_create(screen);
    lv_obj_set_size(time_remaining_label, 100, 25);
    lv_obj_align_to(time_remaining_label, mach_status_label,
                    LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_label_set_text(time_remaining_label,
                      Mach_Status_Text_Time_Remaining);
}

extern "C" void wcc_create_main_screen_widgets(mqd_t *send_q,
                                               mqd_t *receive_q)
{
    main_screen = lv_obj_create(NULL);
    ui_send_q = *send_q;
    ui_receive_q = *receive_q;

    wcc_set_screen_bg_style(main_screen);
    wcc_create_title_bar(main_screen,
                         "Watch Cleaner Controller " WCC_VER);
    create_start_button(main_screen);
    create_stop_button(main_screen);
    create_settings_button(main_screen);
    lv_obj_t * mode_selector = create_mode_selector(main_screen);
    create_machine_status(main_screen);
    wcc_create_main_preset_buttons(main_screen, mode_selector);
}

lv_obj_t *wcc_get_main_screen(void)
{
    return main_screen;
}

extern "C" void wcc_handle_motor_telemetry(void)
{
    struct clean_tel_msg_s telemetry_message;

    while (mq_receive(ui_receive_q, (char *)&telemetry_message,
                      sizeof(telemetry_message), NULL) > 0) {
        if (telemetry_message.state == MOTOR_STATE_STOPPED) {
            lv_obj_clear_state(start_button, LV_STATE_CHECKED);
        }
    }
}

lv_obj_t *wcc_create_title_bar(lv_obj_t *screen, const char *title)
{
    lv_color_t title_color = lv_color_make(WCC_TITLE_BLUE);
    lv_obj_t *title_container = lv_obj_create(screen);
    lv_obj_t *title_label = lv_label_create(title_container);
    lv_obj_clear_flag(title_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(title_container, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_width(title_container, lv_obj_get_width(screen));
    lv_obj_set_height(title_container, 35);
    lv_obj_align(title_container, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_border_width(title_container, 0, 0);
    lv_obj_set_style_radius(title_container, 1, 0);
    lv_obj_set_style_bg_color(title_container, title_color, 0);

    lv_label_set_text(title_label, title);
    lv_obj_center(title_label);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);

    return title_container;
}
