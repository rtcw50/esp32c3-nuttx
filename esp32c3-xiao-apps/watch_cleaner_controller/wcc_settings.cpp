/*
 * File: wcc_settings.cpp
 * Description:
 *   Settings management and persistent preset support for the
 *   Watch Cleaner Controller.
 */

#include <lvgl/lvgl.h>
#include <lvgl/src/others/observer/lv_observer.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "lvgl/src/core/lv_obj.h"
#include "lvgl/src/core/lv_obj_scroll.h"
#include "lvgl/src/misc/lv_area.h"
#include "watch_cleaner_controller.h"

typedef enum {
    WCC_CLEAN = 0,
    WCC_RINSE,
    WCC_SPIN,
    WCC_AGITATE,
    WCC_RPM,
    WCC_SPINUP
} wcc_data_binding_info_t;

typedef struct data_binding_info {
    lv_subject_t *subject;
    int32_t default_value;
    int32_t max_value;
    int32_t update_increment;
    lv_observer_cb_t label_updater;
} data_binding_info;

typedef struct wcc_preset_values {
    int32_t clean_duration;
    int32_t rinse_duration;
    int32_t spin_duration;
    int32_t agitate_duration;
    int32_t rpm;
    int32_t ramp_factor;
} wcc_preset_values_t;

typedef struct wcc_preset_file {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    wcc_preset_values_t values[WCC_PRESET_COUNT];
} wcc_preset_file_t;

static const uint32_t WCC_PRESET_FILE_MAGIC = 0x57434350UL;
static const uint16_t WCC_PRESET_FILE_VERSION = 2;
static wcc_preset_values_t presets[WCC_PRESET_COUNT];
static uint8_t selected_preset;
static bool presets_loaded;

extern enum OperatingMode g_operating_mode;
extern lv_style_t transparent_button_style;
extern lv_style_t on_button_style;
extern lv_style_t off_button_style;
extern lv_style_t duration_button_style;

lv_obj_t *settings_screen;

static lv_obj_t *return_to_main_button;
static lv_obj_t *settings_preset_buttons[WCC_PRESET_COUNT];
static lv_obj_t *main_preset_buttons[WCC_PRESET_COUNT];

lv_subject_t clean_duration_int_subject;
lv_subject_t rinse_duration_int_subject;
lv_subject_t spin_duration_int_subject;
lv_subject_t agitate_duration_int_subject;
lv_subject_t rpm_int_subject;
lv_subject_t ramp_factor_int_subject;

pwm_info pwm_values;


static void update_duration_cb(lv_observer_t *, lv_subject_t *);
static void update_agitate_interval_cb(lv_observer_t *, lv_subject_t *);
static void update_ramp_factor_cb(lv_observer_t *, lv_subject_t *);
static void update_rpm_value_cb(lv_observer_t *, lv_subject_t *);

static data_binding_info dbi[] = {
    { &clean_duration_int_subject, CLEAN_DUR_DEFAULT, CLEAN_DUR_MAX, 30,
      update_duration_cb },
    { &rinse_duration_int_subject, RINSE_DUR_DEFAULT, RINSE_DUR_MAX, 10,
      update_duration_cb },
    { &spin_duration_int_subject, SPIN_DUR_DEFAULT, SPIN_DUR_MAX, 10,
      update_duration_cb },
    { &agitate_duration_int_subject, AGITATE_DUR_DEFAULT, AGITATE_DUR_MAX, 1,
      update_agitate_interval_cb },
    { &rpm_int_subject, RPM_DEFAULT, RPM_MAX, 50, update_rpm_value_cb },
    { &ramp_factor_int_subject, RAMP_FACTOR_DEFAULT, RAMP_FACTOR_MAX, 1,
      update_ramp_factor_cb }
};

extern lv_style_t transparent_button_style;
extern lv_style_t on_button_style;
extern lv_style_t off_button_style;


static void set_default_preset_values(uint8_t preset_index,   
                                      wcc_preset_values_t  *values)                                                      
{                                                             
    switch (preset_index) {                                   
    case 0:                                                   
        values->clean_duration = CLEAN_DUR_DEFAULT;           
        values->rinse_duration = RINSE_DUR_DEFAULT;           
        values->spin_duration = SPIN_DUR_DEFAULT;             
        values->agitate_duration = AGITATE_DUR_DEFAULT;       
        values->rpm = RPM_DEFAULT;                            
        values->ramp_factor = RAMP_FACTOR_DEFAULT;            
        break;                                                
                                                              
    case 1:                                                   
        /* Define preset 2 defaults here. */                  
        values->clean_duration = CLEAN_DUR_DEFAULT2;           
        values->rinse_duration = RINSE_DUR_DEFAULT2;           
        values->spin_duration = SPIN_DUR_DEFAULT2;             
        values->agitate_duration = AGITATE_DUR_DEFAULT2;       
        values->rpm = RPM_DEFAULT2;                            
        values->ramp_factor = RAMP_FACTOR_DEFAULT2;            
        break;                                                
                                                              
    case 2:                                                   
        /* Define preset 3 defaults here. */                  
        values->clean_duration = CLEAN_DUR_DEFAULT3;           
        values->rinse_duration = RINSE_DUR_DEFAULT3;           
        values->spin_duration = SPIN_DUR_DEFAULT3;             
        values->agitate_duration = AGITATE_DUR_DEFAULT3;       
        values->rpm = RPM_DEFAULT3;                            
        values->ramp_factor = RAMP_FACTOR_DEFAULT3;            
        break;                                                
                                                              
    default:                                                  
        break;                                                
    }                                                         
}                                                             
                                                              
static void load_presets(void)                                
{                                                             
    FILE *file;                                               
    wcc_preset_file_t data;                                   
                                                              
    if (presets_loaded) {                                     
        return;                                               
    }                                                         
                                                              
    for (uint8_t i = 0; i < WCC_PRESET_COUNT; i++) {          
        set_default_preset_values(i, &presets[i]);            
    }                                                         
                                                              
    file = fopen(SETTINGS_DATA_PATH, "rb");                   
    if (file != NULL) {                                       
        size_t count = fread(&data, sizeof(data), 1, file);   
        fclose(file);                                         
                                                              
        if (count == 1 &&                                     
            data.magic == WCC_PRESET_FILE_MAGIC &&            
            data.version == WCC_PRESET_FILE_VERSION &&        
            data.count == WCC_PRESET_COUNT) {                 
            for (uint8_t i = 0; i < WCC_PRESET_COUNT; i++) {  
                presets[i] = data.values[i];                  
            }                                                 
        }                                                     
    }                                                         
                                                              
    presets_loaded = true;                                    
}                                                             

static void save_presets(void)                                
{                                                             
    FILE *file;                                               
    wcc_preset_file_t data;                                   
                                                              
    memset(&data, 0, sizeof(data));                           
    data.magic = WCC_PRESET_FILE_MAGIC;                       
    data.version = WCC_PRESET_FILE_VERSION;                   
    data.count = WCC_PRESET_COUNT;                            
                                                              
    for (uint8_t i = 0; i < WCC_PRESET_COUNT; i++) {          
        data.values[i] = presets[i];                          
    }                                                         
                                                              
    file = fopen(SETTINGS_DATA_PATH, "wb");                   
    if (file != NULL) {                                       
        fwrite(&data, sizeof(data), 1, file);                 
        fclose(file);                                         
    }                                                         
}

static void read_current_values(wcc_preset_values_t *values)
{
    values->clean_duration = lv_subject_get_int(&clean_duration_int_subject);
    values->rinse_duration = lv_subject_get_int(&rinse_duration_int_subject);
    values->spin_duration = lv_subject_get_int(&spin_duration_int_subject);
    values->agitate_duration = lv_subject_get_int(&agitate_duration_int_subject);
    values->rpm = lv_subject_get_int(&rpm_int_subject);
    values->ramp_factor = lv_subject_get_int(&ramp_factor_int_subject);
}

static void update_preset_button_states(void)                 
{                                                             
    for (int i = 0; i < WCC_PRESET_COUNT; i++) {              
        lv_obj_t *buttons[] = {                               
            settings_preset_buttons[i],                       
            main_preset_buttons[i]                            
        };                                                    
                                                              
        for (lv_obj_t *button : buttons) {                    
            if (button == NULL) {                             
                continue;                                     
            }                                                 
                                                              
            if (i == selected_preset) {                       
                lv_obj_add_state(button, LV_STATE_CHECKED);   
            } else {                                          
                lv_obj_clear_state(button, LV_STATE_CHECKED); 
            }                                                 
        }                                                     
    }                                                         
}    

static void apply_preset(uint8_t preset_index)
{
    if (preset_index >= WCC_PRESET_COUNT) {
        return;
    }

    selected_preset = preset_index;

    lv_subject_set_int(&clean_duration_int_subject,
                       presets[preset_index].clean_duration);
    lv_subject_set_int(&rinse_duration_int_subject,
                       presets[preset_index].rinse_duration);
    lv_subject_set_int(&spin_duration_int_subject,
                       presets[preset_index].spin_duration);
    lv_subject_set_int(&agitate_duration_int_subject,
                       presets[preset_index].agitate_duration);
    lv_subject_set_int(&rpm_int_subject, presets[preset_index].rpm);
    lv_subject_set_int(&ramp_factor_int_subject,
                       presets[preset_index].ramp_factor);

    update_preset_button_states();
}

void wcc_select_preset(uint8_t preset_index)
{
    apply_preset(preset_index);
}

void wcc_capture_preset(uint8_t preset_index)
{
    if (preset_index >= WCC_PRESET_COUNT) {
        return;
    }

    read_current_values(&presets[preset_index]);
    selected_preset = preset_index;
    save_presets();
    update_preset_button_states();
}

static void preset_button_event_cb(lv_event_t *event)
{
    uint8_t preset_index =
        (uint8_t)(uintptr_t)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_LONG_PRESSED) {
        wcc_capture_preset(preset_index);
    } else if (code == LV_EVENT_CLICKED) {
        wcc_select_preset(preset_index);
    }
}

static lv_obj_t *create_preset_button(lv_obj_t *parent,
                                      uint8_t preset_index,
                                      int width,
                                      int height)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_t *label = lv_label_create(button);
    char text[2];

    text[0] = (char)('1' + preset_index);
    text[1] = '\0';

    lv_obj_remove_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_size(button, width, height);
    lv_obj_add_style(button, &on_button_style, LV_STATE_DEFAULT);
    lv_obj_add_style(button, &off_button_style, LV_STATE_CHECKED);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, preset_button_event_cb, LV_EVENT_ALL,
                        (void *)(uintptr_t)preset_index);

    return button;
}

void wcc_create_main_preset_buttons(lv_obj_t *parent, lv_obj_t * mode_selector)
{
    for (int i = 0; i < WCC_PRESET_COUNT; i++) {
        main_preset_buttons[i] =
            create_preset_button(parent, (uint8_t)i, 32, 30);
        lv_obj_align_to(main_preset_buttons[i], mode_selector,LV_ALIGN_OUT_RIGHT_TOP, 8, i*36) ;
    }

    update_preset_button_states();
}

static void return_to_main_button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        lv_screen_load_anim(wcc_get_main_screen(),
                            LV_SCR_LOAD_ANIM_OVER_TOP, 500, 10, false);
    }
}

static uint16_t map_rpm_to_duty_cycle(uint16_t rpm)
{
    return lv_map(rpm, 0, RPM_MAX, MOTOR_MIN_DUTY, MOTOR_MAX_DUTY);
}

static void up_button_event_cb(lv_event_t *event)
{
    data_binding_info *binding =
        (data_binding_info *)lv_event_get_user_data(event);
    int32_t value = lv_subject_get_int(binding->subject);

    value += binding->update_increment;
    if (value > binding->max_value) {
        value = binding->max_value;
    }

    lv_subject_set_int(binding->subject, value);
}

static void down_button_event_cb(lv_event_t *event)
{
    data_binding_info *binding =
        (data_binding_info *)lv_event_get_user_data(event);
    int32_t value = lv_subject_get_int(binding->subject);

    value -= binding->update_increment;
    if (value < 0) {
        value = 0;
    }

    lv_subject_set_int(binding->subject, value);
}

static lv_obj_t *create_duration_item(const char *description)
{
    lv_color_t background = lv_color_make(WCC_BACKGROUND_GREY);
    lv_obj_t *container = lv_obj_create(settings_screen);
    lv_obj_t *description_label;
    lv_obj_t *up_button;
    lv_obj_t *down_button;
    lv_obj_t *value_container;
    lv_obj_t *value_label;

    lv_obj_set_width(container, lv_obj_get_width(settings_screen));
    lv_obj_set_height(container, 30);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(container, 0, LV_PART_MAIN);   
    lv_obj_set_style_shadow_width(container, 0, LV_PART_MAIN);  
    lv_obj_set_style_bg_color(container, background, LV_PART_MAIN);
    /* Prevent the duration row from scrolling. */                
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);         
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF); 

    description_label = lv_label_create(container);
    lv_label_set_text(description_label, description);
    lv_obj_set_width(description_label, 150);
    lv_obj_align(description_label, LV_ALIGN_TOP_LEFT, 0, -4);

    up_button = lv_btn_create(container);
    lv_obj_set_size(up_button, 30, 30);
    lv_obj_add_style(up_button, &duration_button_style, 0);
    //lv_obj_align(up_button, LV_ALIGN_TOP_LEFT, 150, -7);
    lv_obj_align_to(up_button, description_label, LV_ALIGN_OUT_RIGHT_TOP, 3, -9);
    lv_obj_t *up_label = lv_label_create(up_button);
    lv_label_set_text(up_label, LV_SYMBOL_UP);
    lv_obj_center(up_label);

    down_button = lv_btn_create(container);
    lv_obj_set_size(down_button, 30, 30);
    lv_obj_add_style(down_button, &duration_button_style, 0);
    lv_obj_align_to(down_button, up_button, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
    lv_obj_t *down_label = lv_label_create(down_button);
    lv_label_set_text(down_label, LV_SYMBOL_DOWN);
    lv_obj_center(down_label);

    value_container = lv_obj_create(container);
    lv_obj_set_size(value_container, 75 , 28);
    lv_obj_align_to(value_container, down_button, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_clear_flag(value_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(value_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(value_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(value_container, 1, 0);
    lv_obj_set_style_border_color(value_container, lv_color_black(), 0);

    value_label = lv_label_create(value_container);
    lv_label_set_text(value_label, "00:00");
    lv_obj_center(value_label);

    return container;
}

static lv_obj_t *create_return_to_main_button(void)
{
    lv_obj_t *button = lv_btn_create(settings_screen);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_size(button, 70, 30);
    lv_label_set_text(label, "Done");
    lv_obj_center(label);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(button, return_to_main_button_event_cb,
                        LV_EVENT_ALL, NULL);

    return button;
}

static void format_time(int32_t value, time_format *formatted)
{
    formatted->min = value / 60;
    formatted->sec = value % 60;

    if (formatted->min == 0 && formatted->sec == 0) {
        formatted->sec = value;
    }
}

static void update_time_label_from_subject(lv_observer_t *observer,
                                           lv_subject_t *subject)
{
    lv_obj_t *label = (lv_obj_t *)lv_observer_get_user_data(observer);
    time_format formatted;
    int32_t value = lv_subject_get_int(subject);

    format_time(value, &formatted);
    lv_label_set_text_fmt(label, TIME_FORMAT,
                          (long)formatted.min, (long)formatted.sec);
}

static void update_scalar_label_from_subject(lv_observer_t *observer,
                                             lv_subject_t *subject)
{
    lv_obj_t *label = (lv_obj_t *)lv_observer_get_user_data(observer);
    int32_t value = lv_subject_get_int(subject);

    lv_label_set_text_fmt(label, SCALAR_FORMAT, (long)value);
}

static void update_duration_cb(lv_observer_t *observer, lv_subject_t *subject)
{
    update_time_label_from_subject(observer, subject);
}

static void update_agitate_interval_cb(lv_observer_t *observer,
                                       lv_subject_t *subject)
{
    update_time_label_from_subject(observer, subject);
}

static void update_ramp_factor_cb(lv_observer_t *observer,
                                  lv_subject_t *subject)
{
    update_scalar_label_from_subject(observer, subject);

    if (lv_subject_get_int(subject) < 1) {
        lv_subject_set_int(subject, 1);
    }
}

static void update_rpm_value_cb(lv_observer_t *observer,
                                lv_subject_t *subject)
{
    update_scalar_label_from_subject(observer, subject);
}

static void get_duration_item_controls(const lv_obj_t *container,
                                       lv_obj_t **up_button,
                                       lv_obj_t **down_button,
                                       lv_obj_t **value_label)
{
    lv_obj_t *value_container = lv_obj_get_child(container, 3);

    *up_button = lv_obj_get_child(container, 1);
    *down_button = lv_obj_get_child(container, 2);
    *value_label = lv_obj_get_child(value_container, 0);
}

static void create_data_binding(lv_obj_t *container,
                                data_binding_info *binding)
{
    lv_obj_t *up_button;
    lv_obj_t *down_button;
    lv_obj_t *value_label;

    get_duration_item_controls(container, &up_button, &down_button,
                               &value_label);

    lv_subject_init_int(binding->subject, binding->default_value);
    lv_obj_add_event_cb(up_button, up_button_event_cb,
                        LV_EVENT_SHORT_CLICKED, binding);
    lv_obj_add_event_cb(down_button, down_button_event_cb,
                        LV_EVENT_SHORT_CLICKED, binding);

    lv_observer_t *observer =
        lv_subject_add_observer(binding->subject,
                                binding->label_updater,
                                value_label);
    binding->label_updater(observer, binding->subject);
}

static void create_settings_title_bar(void)
{
    lv_obj_t *bar = lv_obj_create(settings_screen);
    lv_obj_t *title = lv_label_create(bar);

    lv_obj_set_size(bar, lv_obj_get_width(settings_screen), 35);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 1, 0);
    lv_obj_set_style_bg_color(bar, lv_color_make(WCC_TITLE_BLUE), 0);

    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 6, 0);

    for (int i = 0; i < WCC_PRESET_COUNT; i++) {
        settings_preset_buttons[i] =
            create_preset_button(bar, (uint8_t)i, 30, 28);
        lv_obj_align(settings_preset_buttons[i], LV_ALIGN_LEFT_MID,
                     105 + i * 35, 0);
    }

    update_preset_button_states();
}

void wcc_create_settings(void)
{
    load_presets();

    settings_screen = lv_obj_create(NULL);
    wcc_set_screen_bg_style(settings_screen);

    create_settings_title_bar();

    return_to_main_button = create_return_to_main_button();
    lv_obj_align(return_to_main_button, LV_ALIGN_TOP_RIGHT, 0, 2);

    lv_obj_t *clean_item = create_duration_item("CLEAN DURATION:");
    lv_obj_align(clean_item, LV_ALIGN_TOP_LEFT, 0, 36);
    create_data_binding(clean_item, &dbi[WCC_CLEAN]);

    lv_obj_t *rinse_item = create_duration_item("RINSE DURATION:");
    lv_obj_align_to(rinse_item, clean_item, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    create_data_binding(rinse_item, &dbi[WCC_RINSE]);

    lv_obj_t *spin_item = create_duration_item("SPIN DURATION:");
    lv_obj_align_to(spin_item, rinse_item, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    create_data_binding(spin_item, &dbi[WCC_SPIN]);

    lv_obj_t *agitate_item = create_duration_item("AGITATE DURATION:");
    lv_obj_align_to(agitate_item, spin_item, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    create_data_binding(agitate_item, &dbi[WCC_AGITATE]);

    lv_obj_t *rpm_item = create_duration_item("RPM:");
    lv_obj_align_to(rpm_item, agitate_item, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    create_data_binding(rpm_item, &dbi[WCC_RPM]);

    lv_obj_t *ramp_item = create_duration_item("SPIN UP RATE:");
    lv_obj_align_to(ramp_item, rpm_item, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    create_data_binding(ramp_item, &dbi[WCC_SPINUP]);

    update_preset_button_states();
}

lv_obj_t *wcc_get_settings_screen(void)
{
    return settings_screen;
}

uint16_t get_duration(int32_t operating_mode)
{
    if (operating_mode == OperatingMode::clean) {
        return lv_subject_get_int(&clean_duration_int_subject);
    }

    if (operating_mode == OperatingMode::rinse) {
        return lv_subject_get_int(&rinse_duration_int_subject);
    }

    if (operating_mode == OperatingMode::spin) {
        return lv_subject_get_int(&spin_duration_int_subject);
    }

    return 0;
}

uint16_t get_agitate_interval_duration(void)
{
    uint16_t duration =
        (uint16_t)lv_subject_get_int(&agitate_duration_int_subject);

    return duration == 0 ? 1 : duration;
}

uint16_t get_ramp_factor(void)
{
    return (uint16_t)lv_subject_get_int(&ramp_factor_int_subject);
}

uint16_t get_duty_cycle(void)
{
    return map_rpm_to_duty_cycle(
        (uint16_t)lv_subject_get_int(&rpm_int_subject));
}
