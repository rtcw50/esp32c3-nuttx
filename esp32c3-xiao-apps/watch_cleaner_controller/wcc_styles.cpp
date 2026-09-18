/*
 * File: wcc_styles.cpp
 * Author: John
 * Date: 2025-11-17
 * Description:
 *   Global styles used in project defined here 
 */
#include <lvgl/lvgl.h>
#include "watch_cleaner_controller.h"


lv_style_t on_button_style;  // button appearance when not clicked
lv_style_t off_button_style; // button appearance when clicked
lv_style_t stop_button_style; // stop button appearance
lv_style_t transparent_button_style; //  w no border or background
lv_style_t duration_button_style; // Used in settings screen
lv_style_t style_radio;  
lv_style_t style_radio_chk; 
lv_style_t style_radio_button_container;
lv_style_t style_bg;


void wcc_set_screen_bg_style(lv_obj_t * scr)
{
  lv_style_set_radius(&style_bg, 2);
  // Nice grey background
  lv_color_t bgc = lv_color_make(WCC_BACKGROUND_GREY);
  lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);
  lv_style_set_bg_color(&style_bg, bgc);
  lv_obj_add_style(scr, &style_bg, 0);
}

extern "C" void wcc_init_styles(void)
{
    /* Styles should be init'd once */
    lv_style_init(&on_button_style);
    lv_style_init(&off_button_style);
    lv_style_init(&stop_button_style);
    lv_style_init(&transparent_button_style);
    lv_style_init(&duration_button_style);
    lv_style_init(&style_radio);
    lv_style_init(&style_radio_chk);
    lv_style_init(&style_radio_button_container);
    lv_style_init(&style_bg);

    // FIXME: Rename on/off buttons

    // Checkable button - clicked and not clicked appearance 
    // Not clicked appearance
    lv_style_set_radius(&on_button_style, 3);
    lv_style_set_bg_opa(&on_button_style, LV_OPA_100);
    lv_style_set_bg_color(&on_button_style, lv_color_make(WCC_BUTTON_GREEN));
    lv_style_set_border_opa(&on_button_style, LV_OPA_40);
    lv_style_set_border_width(&on_button_style, 4);
    lv_style_set_border_color(&on_button_style, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_outline_opa(&on_button_style, LV_OPA_COVER);
    lv_style_set_outline_color(&on_button_style, lv_color_black());
    lv_style_set_outline_width(&on_button_style, 2);
    lv_style_set_text_color(&on_button_style, lv_color_black());
    lv_style_set_pad_all(&on_button_style, 10);

    // Clicked appearance
    lv_style_set_radius(&off_button_style, 3);
    lv_style_set_bg_opa(&off_button_style, LV_OPA_100);
    lv_style_set_bg_color(&off_button_style, lv_color_make(WCC_BUTTON_YELLOW));
    lv_style_set_border_opa(&off_button_style, LV_OPA_40);
    lv_style_set_border_width(&off_button_style, 4);
    lv_style_set_border_color(&off_button_style, lv_palette_darken(LV_PALETTE_GREY, 4));
    lv_style_set_outline_opa(&off_button_style, LV_OPA_COVER);
    lv_style_set_outline_color(&on_button_style, lv_color_black());
    lv_style_set_outline_width(&on_button_style, 2);
    lv_style_set_text_color(&off_button_style, lv_color_black());
    lv_style_set_pad_all(&off_button_style, 10);

    // Stop button
    lv_style_set_radius(&stop_button_style, 3);
    lv_style_set_bg_opa(&stop_button_style, LV_OPA_100);
    lv_style_set_bg_color(&stop_button_style, lv_color_make(WCC_BUTTON_RED));
    lv_style_set_border_opa(&stop_button_style, LV_OPA_40);
    lv_style_set_border_width(&stop_button_style, 4);
    lv_style_set_border_color(&stop_button_style, lv_palette_darken(LV_PALETTE_GREY, 4));
    lv_style_set_outline_opa(&stop_button_style, LV_OPA_COVER);
    lv_style_set_outline_color(&stop_button_style, lv_color_black());
    lv_style_set_outline_width(&stop_button_style, 2);
    lv_style_set_text_color(&stop_button_style, lv_color_black());
    lv_style_set_pad_all(&stop_button_style, 10);

    // Duration style buttons are up/down buttons for setting parameters
    lv_style_set_radius(&duration_button_style, 2);
    lv_style_set_bg_opa(&duration_button_style, LV_OPA_100);
    lv_style_set_bg_color(&duration_button_style, lv_color_make(WCC_BACKGROUND_GREY));
    lv_style_set_border_opa(&duration_button_style, LV_OPA_COVER);
    lv_style_set_border_width(&duration_button_style, 3);
    lv_style_set_border_color(&duration_button_style, lv_color_black());

     /* Radio Buttons */
    /* 3 radio buttons in a container determine whether we're in clean mode, 
      rinse mode, or spin (dry) mode */
    lv_style_set_border_color(&style_radio, lv_color_black());

    lv_style_set_border_color(&style_radio_chk, lv_color_black());
    lv_style_set_bg_color(&style_radio_chk, lv_color_black());
    
    /* container style */
    lv_style_set_bg_color(&style_radio_button_container, lv_color_white());
    lv_style_set_border_color(&style_radio_button_container, lv_color_black());
    lv_style_set_border_width(&style_radio_button_container, 2);

}