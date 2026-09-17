/*
 * File: wcc_main_screen.cpp
 * Author: John
 * Date: 2025-11-17
 * Description:
 *      GUI widgets for the main screen 
 */
#include <nuttx/config.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <nuttx/mqueue.h>
#include <lvgl/lvgl.h>
#include <lvgl/src/font/lv_symbol_def.h>
#include <unistd.h>
#include "lvgl/src/core/lv_obj.h"
#include "lvgl/src/lv_api_map_v8.h"
#include "lvgl/src/misc/lv_event.h"
#include "lvgl/src/misc/lv_timer.h"
#include "watch_cleaner_controller.h"

/* Defines */
#define PLAYPAUSE_SYMBOL LV_SYMBOL_PLAY LV_SYMBOL_PAUSE

#define BUTTON_LABEL_IS(btn_label,symstring)  (strncmp(btn_label, symstring, strlen(symstring)) == 0) 

#define BUTTON_RESUME_CLICKED(btn) \
  strncmp(lv_label_get_text(lv_obj_get_child(btn, 0)), LV_SYMBOL_PLAY, strlen(LV_SYMBOL_PLAY)) == 0)

#define BUTTON_PAUSE_CLICKED(btn) \
  strncmp(lv_label_get_text(lv_obj_get_child(btn, 0)), LV_SYMBOL_PAUSE, strlen(LV_SYMBOL_PAUSE)) == 0)


/* Externs */
extern lv_style_t on_button_style;
extern lv_style_t off_button_style;
extern lv_style_t stop_button_style;
extern lv_style_t transparent_button_style;
extern lv_style_t style_radio_button_container;
extern lv_style_t style_radio;
extern lv_style_t style_radio_chk;

extern lv_obj_t * settings_screen;


/* Statics */
static lv_timer_t * g_one_sec_timer;
static int32_t g_countdown_in_sec;
static int16_t g_current_duty;
static int16_t g_saved_duty;
static lv_obj_t * main_screen;
static mqd_t ui_send_q, ui_receive_q;
static int32_t g_operating_mode;
static int32_t g_duration;
static int32_t g_agitate_interval_duration;

static const char *Mach_Status_Text_Stopped = "Stopped ...";
static const char *Mach_Status_Text_Running = "Running ...";
static const char *Mach_Status_Text_Paused =  "Paused ...";

/* Statically allocate some room to paste in seconds remaining*/
static char Mach_Status_Text_Time_Remaining[7] = "      ";

static lv_obj_t * start_button; 
static lv_obj_t * stop_button;
static lv_obj_t * settings_button;
static lv_obj_t * mach_status_label;
static lv_obj_t * time_remaining_label;

/* Local prototypes */

/* Local functions */
static void format_and_publish_time_and_status(uint16_t time_remaining, const char * motor_status)
{
  uint32_t minutes = time_remaining/60;
  uint32_t seconds = time_remaining%60;
  if (seconds == 0 && minutes == 0) {
    seconds = time_remaining;
  }
  if (time_remaining <= 0) {
    lv_label_set_text(time_remaining_label, "");
    return;
  }
  
  lv_snprintf(Mach_Status_Text_Time_Remaining,7,"%03d:%02d",minutes,seconds);
  //printf("Time remaining: %s\n", Mach_Status_Text_Time_Remaining);
  lv_label_set_text(time_remaining_label, Mach_Status_Text_Time_Remaining);

  lv_label_set_text(mach_status_label, motor_status);
  return;
}

static  void clear_time_remaining_label(void * user_data) {
  lv_label_set_text(time_remaining_label,"");
}

/* Ramp to target duty setting */
static void send_motor_sequence(int16_t target_duty)
{
  int16_t rf = (int16_t)get_ramp_factor();
  /* The start up sequence is the ramp up or down to the target duty cycle */
  uint16_t diff = (target_duty > g_current_duty) ? (target_duty - g_current_duty) : (g_current_duty - target_duty);
  int step = (diff + (rf-1)) / rf;

  while (g_current_duty < target_duty) {
    g_current_duty += step;
    if (g_current_duty > target_duty) { // check for overshoot
      g_current_duty = target_duty;
    }
    usleep(1000);
    ui_send_cmd(&ui_send_q, MSG_ACTION_PWM_SET_DUTY, g_current_duty);
  }
  while (g_current_duty > target_duty) {
    g_current_duty -= step;
    if (g_current_duty < target_duty) { // check for undershoot
      g_current_duty = target_duty;
    }
    usleep(1000);
    ui_send_cmd(&ui_send_q, MSG_ACTION_PWM_SET_DUTY, g_current_duty);
  }
}

static void shutdown(void)
{
    lv_obj_clear_state(start_button, LV_STATE_CHECKED); 
    lv_obj_t * start_button_label = lv_obj_get_child(start_button, 0);
    lv_label_set_text(start_button_label, LV_SYMBOL_PLAY LV_SYMBOL_PAUSE);
    if (g_one_sec_timer != NULL) {
      lv_timer_delete(g_one_sec_timer);
      g_one_sec_timer = NULL;
    }
    send_motor_sequence(0);
    lv_label_set_text(mach_status_label, Mach_Status_Text_Stopped);
    clear_time_remaining_label(NULL);
    //ui_send_cmd(&ui_send_q, MSG_ACTION_PWM_STOP, 0);
}

static void agitate_interval_reverse_sequence(void)
{
  send_motor_sequence(0);
  ui_send_cmd(&ui_send_q, MSG_ACTION_PWM_REVERSE, 0);
  send_motor_sequence(get_duty_cycle());
  g_agitate_interval_duration = get_agitate_interval_duration();
}

static void one_sec_timer_expiry_cb(lv_timer_t * timer)
{
  /* Respond to settings changes */
  /* ramp factor is read at motor sequencing time */
  if (g_current_duty != get_duty_cycle()) {
    send_motor_sequence(get_duty_cycle());
  }
  if (g_duration != get_duration(g_operating_mode)) {
    g_duration = get_duration(g_operating_mode);
    g_countdown_in_sec = g_duration;
  }
  g_agitate_interval_duration = get_agitate_interval_duration();

  /* Run time timed out */
  if (g_countdown_in_sec == 0) { 
    shutdown();
    return;
  }

  /* Time to reverse the motor */
  if ((g_countdown_in_sec % g_agitate_interval_duration == 0) &&
    g_countdown_in_sec >= g_agitate_interval_duration &&
    g_operating_mode != OperatingMode::spin &&
    g_countdown_in_sec != g_duration) {
      agitate_interval_reverse_sequence();
  }

  /* Update the display*/
  format_and_publish_time_and_status((uint16_t)g_countdown_in_sec, Mach_Status_Text_Running);

  /* Update the tick*/
  g_countdown_in_sec -= 1;
}

static void start_button_event_cb(lv_event_t * event)
{
  lv_obj_t * button = lv_event_get_target_obj(event);
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t * label = lv_obj_get_child(button, 0);  // Label of button

  // The start button has 3 states: ready to start (>||), running (||), paused (>). Due to 
  // issues with the checkable button feature, we have to use the label text to determine the current state of the button.  
  // The ready to start icon is the play-pause symbol.
  // When clicked, it changes to the pause symbol and the motor starts running.  
  // When the pause button is clicked, it changes to the play symbol and the motor pauses. 
  // When the play button is clicked, it changes to the pause symbol and the motor resumes running.
  // The separate stop button can be used to stop the motor and reset to ready to start state.
  #if 0
  if (code == LV_EVENT_LONG_PRESSED) {
    /* To keep the on state style of the button */
    lv_obj_set_state(button, LV_STATE_CHECKED, true);
    wcc_calibration_setup(true);
    return;
  }
  #endif

  if (code == LV_EVENT_SHORT_CLICKED) {
    if (BUTTON_LABEL_IS(lv_label_get_text(label), LV_SYMBOL_PLAY LV_SYMBOL_PAUSE)) {
      /* Start a cycle, button changes to pause symbol*/
      lv_label_set_text(label,LV_SYMBOL_PAUSE);

      ui_send_cmd(&ui_send_q,  MSG_ACTION_PWM_START, 0);

      /* Get the duration based on the operating mode */
      g_duration = get_duration(g_operating_mode);
      g_agitate_interval_duration = get_agitate_interval_duration();
      g_countdown_in_sec = g_duration;

      /* Create and start the 1 second tick timer*/
      g_one_sec_timer = lv_timer_create(one_sec_timer_expiry_cb, 1 * MC_MS_PER_SEC, NULL);
    }
    else if (BUTTON_LABEL_IS(lv_label_get_text(label), LV_SYMBOL_PAUSE)) {
      lv_label_set_text(label,LV_SYMBOL_PLAY); // play icon
      lv_timer_pause(g_one_sec_timer);
      /* Ramp down the motor */
      g_saved_duty = g_current_duty;
      send_motor_sequence(0);
      format_and_publish_time_and_status((uint16_t)g_countdown_in_sec, Mach_Status_Text_Paused);
    }
    else if (BUTTON_LABEL_IS(lv_label_get_text(label), LV_SYMBOL_PLAY)) {
      lv_label_set_text(label,LV_SYMBOL_PAUSE); // resume icon
      lv_timer_resume(g_one_sec_timer);
      /* Set motor back to previous speed */
      send_motor_sequence(g_saved_duty);
    }
    else {
      LV_ASSERT_MSG(false, "Unknown start button state");
    }
  }
}

static lv_obj_t * create_start_button(lv_obj_t * scr)
{
    /* Start Button*/
    start_button = lv_btn_create(scr);
    lv_obj_remove_style_all(start_button);
    lv_obj_t * start_button_label = lv_label_create(start_button);
    lv_label_set_text(start_button_label, LV_SYMBOL_PLAY LV_SYMBOL_PAUSE);
    lv_obj_center(start_button_label);
    lv_obj_set_style_text_font(start_button_label, &lv_font_montserrat_20, 0);
    lv_obj_add_flag(start_button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_remove_flag(start_button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(start_button, start_button_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_width(start_button, 60); 
    lv_obj_set_height(start_button, 60);
    lv_obj_align(start_button, LV_ALIGN_TOP_LEFT, 30, 45);
    /* Apply styles */
    lv_obj_add_style(start_button, &on_button_style, LV_STATE_DEFAULT);
    lv_obj_add_style(start_button, &off_button_style, LV_STATE_CHECKED);
    // Don't let the touch screen think the widget is scrolling
    lv_obj_remove_flag(start_button, LV_OBJ_FLAG_SCROLLABLE);
    return start_button;

}

static void stop_button_event_cb(lv_event_t * event)
{
  lv_event_code_t code = lv_event_get_code(event);

  if (code == LV_EVENT_CLICKED) { 
    /* Let the one sec timer shut it down */
    g_countdown_in_sec = 0;
    /* If paused and stop button is hit, unpause and let one sec timer do shutdown */
    if (g_one_sec_timer != NULL && lv_timer_get_paused(g_one_sec_timer)) {
      lv_timer_resume(g_one_sec_timer);
    }
  }
}

static lv_obj_t * create_stop_button(lv_obj_t * scr)
{
    stop_button = lv_btn_create(scr);
    lv_obj_remove_style_all(stop_button);
    lv_obj_t * stop_button_label = lv_label_create(stop_button);
    lv_label_set_text(stop_button_label, LV_SYMBOL_STOP);
    lv_obj_center(stop_button_label);
    lv_obj_set_style_text_font(stop_button_label, &lv_font_montserrat_20, 0);
    // Stop button is not checkable, does not retain button state.
    // lv_obj_add_flag(stop_button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_remove_flag(stop_button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_width(stop_button, 60); 
    lv_obj_set_height(stop_button,60);
    lv_obj_align_to(stop_button, start_button, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    /* Apply styles */
    lv_obj_add_style(stop_button, &stop_button_style, LV_STATE_DEFAULT);
    /* Actions on click */
    lv_obj_add_event_cb(stop_button, stop_button_event_cb, LV_EVENT_ALL, NULL);
    return stop_button;

}

static void settings_button_event_cb(lv_event_t * event)
{
  lv_event_code_t code = lv_event_get_code(event);

  if (code == LV_EVENT_CLICKED) {
    LV_LOG_USER("Settings button clicked");
    lv_screen_load_anim(wcc_get_settings_screen(), LV_SCR_LOAD_ANIM_OVER_TOP, 500 , 10,false);
  }
}

static lv_obj_t * create_settings_button(lv_obj_t * scr)
{
    settings_button= lv_btn_create(scr);
    lv_obj_remove_style_all(settings_button);
    lv_style_init(&transparent_button_style);
    lv_style_set_bg_opa(&transparent_button_style,  LV_OPA_TRANSP);
    lv_obj_add_style(settings_button, &transparent_button_style,LV_PART_MAIN);
    lv_obj_t * settings_button_label = lv_label_create(settings_button);
    // Custom sized gear "settings" icon button, see LVGL lv_font docs
    lv_obj_set_style_text_font(settings_button_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(settings_button_label, lv_color_black(), 0);
    lv_label_set_text(settings_button_label, LV_SYMBOL_SETTINGS);
    lv_obj_center(settings_button_label);
    lv_obj_remove_flag(settings_button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(settings_button, settings_button_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_width(settings_button, 60); 
    lv_obj_set_height(settings_button, 60);
    lv_obj_align(settings_button, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    return settings_button;
}

static void radio_event_handler(lv_event_t * e)
{
    int32_t * active_id = (int32_t *)lv_event_get_user_data(e);
    lv_obj_t * container = (lv_obj_t *)lv_event_get_current_target(e);
    lv_obj_t * act_cb = lv_event_get_target_obj(e);
    lv_obj_t * old_cb = lv_obj_get_child(container, *active_id);

    /*Do nothing if the container was clicked*/
    if(act_cb == container) return;

    lv_obj_remove_state(old_cb, LV_STATE_CHECKED);   /*Uncheck the previous radio button*/
    lv_obj_add_state(act_cb, LV_STATE_CHECKED);     /*Check the current radio button*/

    *active_id = lv_obj_get_index(act_cb);
    /* Save the operating mode: clean, rinse or spin */
    /*  active id range is 1 (clean), 2(rinse), 3(spin) - correct for OperatingMode enum values*/
    g_operating_mode = (uint16_t)(*active_id-1);
}

static void radio_button_create(lv_obj_t * parent, const char * txt)
{
    lv_obj_t * obj = lv_checkbox_create(parent);
    lv_checkbox_set_text(obj, txt);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_style(obj, &style_radio, LV_PART_INDICATOR);
    lv_obj_add_style(obj, &style_radio_chk, LV_PART_INDICATOR | LV_STATE_CHECKED);
}

static lv_obj_t * create_mode_selector(lv_obj_t * scr)
{
    static int32_t active_index=1;
    lv_obj_t * radio_button_container = lv_obj_create(scr);
    lv_obj_set_size(radio_button_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(radio_button_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_t * rb_box_label = lv_label_create(radio_button_container);
    lv_label_set_text(rb_box_label, "Mode Select");
    lv_obj_set_style_text_font(rb_box_label, &lv_font_montserrat_16, 0);
    //lv_obj_align(rb_box_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_align_to(radio_button_container, start_button, LV_ALIGN_OUT_RIGHT_TOP, 50, 0);
    lv_obj_add_event_cb(radio_button_container, radio_event_handler, LV_EVENT_CLICKED, &active_index);
    lv_obj_add_style(radio_button_container, &style_radio_button_container, 0);

    radio_button_create(radio_button_container, "Clean");
    radio_button_create(radio_button_container, "Rinse");
    radio_button_create(radio_button_container, "Spin");
    lv_obj_add_state(lv_obj_get_child(radio_button_container, 1), LV_STATE_CHECKED); 
    g_operating_mode = OperatingMode::clean;
    return radio_button_container;
}

static void create_machine_status(lv_obj_t * scr)
{
    /* Machine status text label*/
    mach_status_label = lv_label_create(scr);
    lv_obj_set_width(mach_status_label, 100);
    lv_obj_set_height(mach_status_label, 25);
    lv_obj_align(mach_status_label, LV_ALIGN_TOP_LEFT, 15, 215);
    lv_label_set_text(mach_status_label, Mach_Status_Text_Stopped); // Keyed by start button 
    /* Time remaining label - aligned next to machine status label */
    time_remaining_label = lv_label_create(scr);
    lv_obj_set_width(time_remaining_label, 100);
    lv_obj_set_height(time_remaining_label, 25);
    lv_obj_align_to(time_remaining_label, mach_status_label, LV_ALIGN_OUT_RIGHT_MID, 0, 0); 
    lv_label_set_text(time_remaining_label, Mach_Status_Text_Time_Remaining); // Keyed by start button 
    return;
}

/* Public functions */
extern "C" void wcc_create_main_screen_widgets(mqd_t * send_q, mqd_t * receive_q)
{
    /* Build UI on the active screen created by lv_nuttx_init */
    main_screen = lv_obj_create(NULL); 
    LV_ASSERT(main_screen != NULL);
    ui_send_q = *send_q;
    ui_receive_q = *receive_q;

    wcc_set_screen_bg_style(main_screen);

    /* Create the settings screen */
    //wcc_create_settings();



    (void)wcc_create_title_bar(main_screen, "Watch Cleaner Controller " WCC_VER);
    (void)create_start_button(main_screen);
    (void)create_stop_button(main_screen);
    (void)create_settings_button(main_screen);
    (void)create_mode_selector(main_screen);
    (void)create_machine_status(main_screen);
    return;
}

lv_obj_t * wcc_get_main_screen(void)
{
    return main_screen;
}


extern "C" void wcc_handle_motor_telemetry()
{
  struct clean_tel_msg_s telemetry_msg;

  while ( mq_receive(ui_receive_q, (char *)&telemetry_msg, sizeof(telemetry_msg), NULL) > 0) {
        //printf("Received telemetry message: state=%d, time_remaining=%d\n", telemetry_msg.state, telemetry_msg.time_remaining);
        switch (telemetry_msg.state) {
            case MOTOR_STATE_STOPPED:
                {
                lv_obj_clear_state(start_button, LV_STATE_CHECKED);
                }
                break; 
            case MOTOR_STATE_RUNNING:
            case MOTOR_STATE_PAUSED:
            case MOTOR_STATE_ABORTED:
                break; 
            default:
                break;
        }
  }
}

lv_obj_t * wcc_create_title_bar(lv_obj_t * scr, const char * title)
{
    // Pleasant blue screen title bar
    lv_color_t  tc = lv_color_make(WCC_TITLE_BLUE);
    lv_obj_t * title_cont = lv_obj_create(scr);
    lv_obj_t * title_label = lv_label_create(title_cont);
    // No border
    lv_obj_set_style_border_width(title_cont, 0, 0);
    // Squared off corners
    lv_obj_set_style_radius(title_cont, 1, 0);
    lv_label_set_text(title_label, title);
    lv_obj_center(title_label);
    LV_ASSERT(scr != NULL);
    lv_obj_set_width(title_cont, lv_obj_get_width(scr));
    lv_obj_set_height(title_cont, 35);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_bg_color(title_cont, tc, 0);
    lv_obj_align( title_cont, LV_ALIGN_TOP_LEFT, 0, 0 );
    return title_cont;
}