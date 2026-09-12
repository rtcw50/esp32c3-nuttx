/*
 * File: wcc_settings.cpp
 * Author: John
 * Date: 2025-11-14
 * Description:
 *   Implementation of settings management for the Watch Cleaner Controller (WCC).
 *   This file will contain functions to load, save, and validate configuration
 *   parameters for the ESP32-based WCC project.
 */
#include <lvgl/lvgl.h>
#include <lvgl/src/others/observer/lv_observer.h>
#include "watch_cleaner_controller.h"

/* Enum for data binding types */
typedef enum {
    WCC_CLEAN = 0,
    WCC_RINSE,
    WCC_SPIN,
    WCC_AGITATE,
    WCC_RPM,
    WCC_SPINUP
} wcc_data_binding_info_t;


 /* Structs */
 typedef struct data_binding_info {
    lv_subject_t * subject; /**< Pointer to the subject associated with this data binding */
    int32_t default_value;
    int32_t max_value;
    int32_t update_increment; 
    lv_observer_cb_t label_updater;
 } data_binding_info;

/* Externs */
extern enum OperatingMode g_operating_mode;

/* Globals/Statics */
lv_obj_t * settings_screen;
static lv_obj_t * return_to_main_button;

lv_subject_t clean_duration_int_subject;
lv_subject_t rinse_duration_int_subject;
lv_subject_t spin_duration_int_subject;
lv_subject_t agitate_duration_int_subject;
lv_subject_t rpm_int_subject;
lv_subject_t ramp_factor_int_subject; 

pwm_info pwm_values;

/* Local prototypes */
static void update_duration_cb(lv_observer_t *, lv_subject_t *);
static void update_agitate_interval_cb(lv_observer_t *, lv_subject_t *);
static void update_ramp_factor_cb(lv_observer_t *, lv_subject_t *);
static void update_rpm_value_cb(lv_observer_t *, lv_subject_t *);


static data_binding_info dbi[] = {
    { 
        .subject = &clean_duration_int_subject,
        .default_value = CLEAN_DUR_DEFAULT,
        .max_value = CLEAN_DUR_MAX,
        .update_increment = 30,
        .label_updater = update_duration_cb,
    },
    { 
        .subject = &rinse_duration_int_subject,
        .default_value = RINSE_DUR_DEFAULT,
        .max_value = RINSE_DUR_MAX,
        .update_increment = 10,
        .label_updater = update_duration_cb,
    },
    { 
        .subject = &spin_duration_int_subject,
        .default_value = SPIN_DUR_DEFAULT,
        .max_value = SPIN_DUR_MAX,
        .update_increment = 10,
        .label_updater = update_duration_cb,
    },
    { 
        .subject = &agitate_duration_int_subject,
        .default_value = AGITATE_DUR_DEFAULT,
        .max_value = AGITATE_DUR_MAX,
        .update_increment = 1,
        .label_updater = update_agitate_interval_cb,
    },
    { 
        .subject = &rpm_int_subject,
        .default_value = RPM_DEFAULT,
        .max_value = RPM_MAX,
        .update_increment = 50,
        .label_updater = update_rpm_value_cb,
    },
    { 
        .subject = &ramp_factor_int_subject,
        .default_value = RAMP_FACTOR_DEFAULT,
        .max_value = RAMP_FACTOR_MAX,
        .update_increment = 1,
        .label_updater = update_ramp_factor_cb,
    }
};

 /* Externs */
extern lv_obj_t * main_screen;
extern lv_style_t transparent_button_style;

/* Public functions */
void wcc_create_settings(void);
lv_obj_t * wcc_get_settings_screen(void);

/* Local Prototypes*/
static void return_to_main_button_event_cb(lv_event_t * event);
static void up_button_event_cb(lv_event_t * event);
static void down_button_event_cb(lv_event_t * event);


static void return_to_main_button_event_cb(lv_event_t * event)
{
  lv_event_code_t code = lv_event_get_code(event);

  if (code == LV_EVENT_CLICKED) {
    LV_LOG_USER("Return button clicked");
    lv_screen_load_anim(wcc_get_main_screen(), LV_SCR_LOAD_ANIM_OVER_TOP, 500, 10 ,false);
  }
}

/* 
    Map the RPM setting to a duty cycle percentage
*/
static uint16_t map_rpm_to_duty_cycle(uint16_t rpm)
{
    return lv_map(rpm, 0, RPM_MAX, MOTOR_MIN_DUTY, MOTOR_MAX_DUTY); 
}

static void up_button_event_cb(lv_event_t * event)
{
  data_binding_info * ldbi  = (data_binding_info *)lv_event_get_user_data(event);
  int32_t val = lv_subject_get_int(ldbi->subject);

//  TBD: add LV_EVENT_LONG_PRESSED_REPEAT support for quick changing
//  values. There's seems to be a problem with SHORT_CLICKED events
//  intermixed with LONG_PRESSED_REPEAT events.  
//  For now, just update by +/- 30 sec increments.
    val += ldbi->update_increment;
    val = val >= ldbi->max_value ? ldbi->max_value : val;
    lv_subject_set_int(ldbi->subject, val);
}

static void down_button_event_cb(lv_event_t * event)
{
  data_binding_info * ldbi = (data_binding_info *)lv_event_get_user_data(event);
  int32_t val = lv_subject_get_int(ldbi->subject);

  val -= ldbi->update_increment;
  // time setting value bottoms out at zero
  val = val <= 0 ? 0 : val;
  lv_subject_set_int(ldbi->subject, val);
}

/*
    This describes a row in the settings screen that contains
    item_description up_button down_button time_settings_label
*/
static lv_obj_t * create_duration_item(const char * desc)
{
    // A container for the desc label, buttons, and time label
    lv_color_t bgc = lv_color_make(WCC_BACKGROUND_GREY);
    lv_obj_t * cont = lv_obj_create(settings_screen);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_width(cont, lv_obj_get_width(settings_screen));
    lv_obj_set_height(cont,30);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, bgc, 0);

    // Descriptor label
    lv_obj_t * desc_label = lv_label_create(cont);
    lv_label_set_text(desc_label, desc);
    lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_14, 0);
    // Define the width of the description
    lv_obj_set_width(desc_label, 150);
    // Left side of container
    lv_obj_align(desc_label, LV_ALIGN_TOP_LEFT, 0, -4);

    // Buttons
    extern lv_style_t duration_button_style;

    lv_obj_t * up_button = lv_btn_create(cont);
    lv_obj_set_width(up_button, 30);
    lv_obj_set_height(up_button, 30);
    lv_obj_add_style(up_button, &duration_button_style, 0);
    lv_obj_t * up_button_label = lv_label_create(up_button);
    lv_obj_center(up_button_label);
    lv_label_set_text(up_button_label, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(up_button_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(up_button_label, lv_color_black(), 0);
    lv_obj_align_to(up_button, desc_label, LV_ALIGN_OUT_RIGHT_TOP, 0, -7);

    lv_obj_t * down_button = lv_btn_create(cont);
    lv_obj_set_width(down_button, 30);
    lv_obj_set_height(down_button, 30);
    lv_obj_add_style(down_button, &duration_button_style, 0);
    lv_obj_t * down_button_label = lv_label_create(down_button);
    lv_obj_center(down_button_label);
    lv_label_set_text(down_button_label, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(down_button_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(down_button_label, lv_color_black(), 0);
    lv_obj_align_to(down_button, up_button, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    // Time display field with white, bordered background
    lv_obj_t * time_label_cont = lv_obj_create(cont);
    lv_obj_set_style_bg_color(time_label_cont, lv_color_white(), 0);
    lv_obj_set_style_border_width(time_label_cont, 1, 0);
    lv_obj_set_style_border_color(time_label_cont, lv_color_black(), 0);
    lv_obj_set_style_radius(time_label_cont, 1, 0);
    lv_obj_set_width(time_label_cont, 75);
    lv_obj_set_height(time_label_cont, 28);
    // Right side of parent container
    lv_obj_align(time_label_cont, LV_ALIGN_RIGHT_MID, -1, -1);

    lv_obj_t * time_label = lv_label_create(time_label_cont);
    lv_obj_set_style_bg_color(time_label_cont, lv_color_white(), 0);
    lv_label_set_text(time_label, "00:00");
    lv_obj_center(time_label);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_16, 0);

    return cont;
}

static lv_obj_t * create_return_to_main_button()
{
    lv_obj_t * button = lv_btn_create(settings_screen);
    //lv_obj_remove_style_all(button);
    lv_obj_t * button_label = lv_label_create(button);
    //lv_obj_add_style(button, &transparent_button_style, LV_PART_MAIN);
    lv_obj_set_style_text_font(button_label, &lv_font_montserrat_16, 0);
    lv_label_set_text(button_label, "Done " LV_SYMBOL_NEW_LINE);
    lv_obj_center(button_label);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(button, return_to_main_button_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_width(button, 70); 
    lv_obj_set_height(button, 30);
    return button;
}

static void format_time(const int32_t in, time_format * out)
{
    out->min = in/60;
    out->sec = in%60;

    // out->sec will be zero if in < 60
    if (out->min == 0 && out->sec ==0 ) {
        out->sec = in;
    }
}

uint16_t get_duration(int32_t operating_mode)
{
    if (operating_mode == OperatingMode::clean)
        return lv_subject_get_int(&clean_duration_int_subject);
    if (operating_mode == OperatingMode::rinse)
        return lv_subject_get_int(&rinse_duration_int_subject);
    if (operating_mode == OperatingMode::spin)
        return lv_subject_get_int(&spin_duration_int_subject);
    return 0;
}

uint16_t get_agitate_interval_duration()
{
    return lv_subject_get_int(&agitate_duration_int_subject);
}

uint16_t get_ramp_factor()
{
    return lv_subject_get_int(&ramp_factor_int_subject);
}

uint16_t get_duty_cycle()
{
    return  map_rpm_to_duty_cycle(lv_subject_get_int(&rpm_int_subject));
}

static void update_time_label_from_subject(lv_observer_t * observer, lv_subject_t * subj)
{
    lv_obj_t * label =(lv_obj_t *)lv_observer_get_user_data(observer);
    LV_ASSERT_NULL(label);
    int16_t value = lv_subject_get_int(subj);
    time_format tm;
    format_time(value, &tm);
    lv_label_set_text_fmt(label,TIME_FORMAT, tm.min, tm.sec);
}

static void update_scalar_label_from_subject(lv_observer_t * observer, lv_subject_t * subj)
{
    lv_obj_t * label =(lv_obj_t *)lv_observer_get_user_data(observer);
    LV_ASSERT_NULL(label);
    int16_t value = lv_subject_get_int(subj);
    lv_label_set_text_fmt(label,SCALAR_FORMAT, (int32_t)value);
}

static void  update_duration_cb(lv_observer_t * observer, lv_subject_t * subj)
{
    /* Update the setting display label */
    update_time_label_from_subject(observer, subj);
}

static void  update_agitate_interval_cb(lv_observer_t * observer, lv_subject_t * subj)
{
    update_time_label_from_subject(observer, subj);
}

static void  update_ramp_factor_cb(lv_observer_t * observer, lv_subject_t * subj)
{
    update_scalar_label_from_subject(observer, subj);
    uint16_t value = (uint16_t)lv_subject_get_int(subj);
    if (value < 1) {
        lv_subject_set_int(subj, 1); // Ensure ramp factor is at least 1
    }
}

/* When RPM value changes, the settings label is updated. */ 
static void  update_rpm_value_cb(lv_observer_t * observer, lv_subject_t * subj)
{
    update_scalar_label_from_subject(observer, subj);
}


static void get_duration_item_buttons_and_label(const lv_obj_t * dur_cont, lv_obj_t ** ubutton, lv_obj_t ** dbutton, lv_obj_t ** label)
{
    lv_obj_t * cont;

    // The time label is in a container
    cont = lv_obj_get_child(dur_cont, 3);
    LV_ASSERT(lv_obj_check_type(cont, &lv_obj_class));
    *label = lv_obj_get_child(cont, 0);
    LV_ASSERT(lv_obj_check_type(*label, &lv_label_class));
    *ubutton = lv_obj_get_child(dur_cont, 1);
    LV_ASSERT(lv_obj_check_type(*ubutton, &lv_button_class));
    *dbutton = lv_obj_get_child(dur_cont, 2);
    LV_ASSERT(lv_obj_check_type(*dbutton, &lv_button_class));
}

static void create_data_binding(lv_obj_t * cont, data_binding_info *ldbi)
{
    lv_obj_t *ubutton, *dbutton, *label;
    // Define button actions and data/label bindings
    get_duration_item_buttons_and_label(cont, &ubutton, &dbutton, &label);
    LV_ASSERT(lv_obj_check_type(ubutton, &lv_button_class));
    LV_ASSERT(lv_obj_check_type(dbutton, &lv_button_class));
    LV_ASSERT(lv_obj_check_type(label, &lv_label_class));
    // Init subject
    lv_subject_init_int(ldbi->subject, ldbi->default_value);
    lv_obj_add_event_cb(ubutton, up_button_event_cb, LV_EVENT_SHORT_CLICKED, ldbi);
    lv_obj_add_event_cb(dbutton, down_button_event_cb, LV_EVENT_SHORT_CLICKED, ldbi);

    // User data includes the label and data type for the subject. The label is updated when the subject value changes.
    // The data type is used to determine which setting is being updated (clean, rinse, spin, agitate, rpm, spinup)

    lv_observer_t * observer = lv_subject_add_observer(ldbi->subject, ldbi->label_updater, label);
    // init label -- may not be necessary.
    ldbi->label_updater(observer, ldbi->subject);
}

void wcc_create_settings(void)
{

    settings_screen = lv_obj_create(NULL);
    LV_ASSERT(settings_screen != NULL);
    LV_ASSERT(lv_obj_check_type(settings_screen, &lv_obj_class));

    // Background
    wcc_set_screen_bg_style(settings_screen);

    // Title Bar
    (void)wcc_create_title_bar(settings_screen, "Settings");

    // Create return to main button on settings screen 
    return_to_main_button = create_return_to_main_button();
    lv_obj_align(return_to_main_button, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t * clean_duration_item_container = create_duration_item("CLEAN DURATION:");
    lv_obj_align(clean_duration_item_container, LV_ALIGN_TOP_LEFT, 0, 36);

    // This binds the settings value to the label using subject/observer pattern
    // The up/down button callback just updates the subject value and the label is updated
    // via a label callback
    create_data_binding(clean_duration_item_container, &dbi[WCC_CLEAN]);

    lv_obj_t * rinse_duration_item_container = create_duration_item("RINSE DURATION:");
    lv_obj_align_to(rinse_duration_item_container, 
        clean_duration_item_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    create_data_binding(rinse_duration_item_container, &dbi[WCC_RINSE]);

    lv_obj_t * spin_duration_item_container = create_duration_item("SPIN DURATION:");
    lv_obj_align_to(spin_duration_item_container, 
        rinse_duration_item_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    create_data_binding(spin_duration_item_container, &dbi[WCC_SPIN]);

    lv_obj_t * agitate_duration_item_container = create_duration_item("AGITATE DURATION:");
    lv_obj_align_to(agitate_duration_item_container,
        spin_duration_item_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 2); 
    create_data_binding(agitate_duration_item_container, &dbi[WCC_AGITATE]);

    lv_obj_t * rpm_item_container = create_duration_item("RPM:");
    lv_obj_align_to(rpm_item_container,
        agitate_duration_item_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 2); 
    create_data_binding(rpm_item_container, &dbi[WCC_RPM]);

    lv_obj_t * spin_up_rate_item_container = create_duration_item("SPIN UP RATE:");
    lv_obj_align_to(spin_up_rate_item_container,
        rpm_item_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 2); 
    create_data_binding(spin_up_rate_item_container, &dbi[WCC_SPINUP]);

    return;

}

lv_obj_t * wcc_get_settings_screen(void)
{
    return settings_screen;
}
