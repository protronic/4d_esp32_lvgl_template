#include <Arduino.h>
#include <lvgl.h>
#include "gfx4desp32_gen4_ESP32_70CT.h"

gfx4desp32_gen4_ESP32_70CT gfx = gfx4desp32_gen4_ESP32_70CT();


//Based on code from https://forum.4dsystems.com.au/node/80632

//#define LCD_DOUBLE_BUF

static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_color_t *disp_draw_buf1;
static lv_color_t *disp_draw_buf2;
static unsigned long last_ms;
static lv_display_t *my_disp;
static lv_obj_t *status_clock_label;
static lv_obj_t *brightness_value_label;
static lv_obj_t *load_bar;
static lv_obj_t *load_label;
static lv_timer_t *ui_timer;
static uint16_t demo_seconds;
static int32_t brightness_value = 0;

void init_ui(void);

static void on_brightness_changed(lv_event_t *e)
{
  lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
  brightness_value = lv_slider_get_value(slider);

  if (brightness_value_label)
  {
    lv_label_set_text_fmt(brightness_value_label, "%d%%", brightness_value);
  }
  Serial.printf("Brightness changed: %d%%\n", brightness_value);
}

static void on_nav_button_clicked(lv_event_t *e)
{
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
  const char *txt = lv_label_get_text(lv_obj_get_child(btn, 0));
  Serial.printf("Action tapped: %s\n", txt ? txt : "unknown");
}

static void on_toggle_changed(lv_event_t *e)
{
  lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
  bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  Serial.printf("Switch changed: %s\n", enabled ? "ON" : "OFF");
}

static void ui_tick_cb(lv_timer_t *timer)
{
  (void)timer;
  demo_seconds++;

  if (status_clock_label)
  {
    uint16_t mm = demo_seconds / 60;
    uint16_t ss = demo_seconds % 60;
    lv_label_set_text_fmt(status_clock_label, "%02u:%02u", mm, ss);
  }

  if (load_bar)
  {
    uint16_t value = (demo_seconds * 7) % 101;
    lv_bar_set_value(load_bar, value, LV_ANIM_ON);
    if (load_label)
    {
      lv_label_set_text_fmt(load_label, "Render load: %u%%", value);
    }
  }
}

uint32_t my_tick_cb(void)
{
  return (uint32_t)millis();
}

/* Display flushing */
void my_disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
	uint32_t numPixels = lv_area_get_size(area);
  gfx.SetGRAM(area->x1, area->y1, area->x2, area->y2);
  gfx.pushColors((uint16_t *)color_p, numPixels);
  lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data)
{
  static bool wasTouched = false;

  if (gfx.touch_Update())
  {
    int touchStatus = gfx.touch_GetPen();
    if (touchStatus == 1)
    {
      data->state = LV_INDEV_STATE_PR;
      data->point.x = gfx.touch_GetX();
      data->point.y = gfx.touch_GetY();

      if (!wasTouched)
      {
        Serial.print("Touch press: ");
        Serial.print(data->point.x);
        Serial.print(", ");
        Serial.println(data->point.y);
      }

      wasTouched = true;
    }
    else if (touchStatus == 2)
    {
      data->state = LV_INDEV_STATE_REL;
      if (wasTouched) Serial.println("Touch release");
      wasTouched = false;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
    wasTouched = false;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("LVGL Template");

  pinMode(GEN4_RGB_PIN_NUM_BK_LIGHT, OUTPUT);
  digitalWrite(GEN4_RGB_PIN_NUM_BK_LIGHT, HIGH);

  gfx.begin(16);
  gfx.Cls();
  gfx.Orientation(PORTRAIT);
  gfx.touch_Set(TOUCH_ENABLE);
  gfx.BacklightOn(true);

  lv_init();
  lv_tick_set_cb(my_tick_cb);
  
  screenWidth = gfx.getWidth();
  screenHeight = gfx.getHeight();

  /* Initialize the display */
  my_disp = lv_display_create(screenWidth, screenHeight);
  uint32_t bufsize = screenWidth * 128 * lv_color_format_get_size(lv_display_get_color_format(NULL));
  
  disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(bufsize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!disp_draw_buf1) Serial.println("LVGL disp_draw_buf1 allocate failed!");
  else
  {
#ifdef LCD_DOUBLE_BUF
    disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(bufsize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf2) Serial.println("LVGL disp_draw_buf2 allocate failed!");
    else
    {
#endif

      lv_display_set_flush_cb(my_disp, my_disp_flush_cb);
      lv_display_set_buffers(my_disp, disp_draw_buf1, disp_draw_buf2, bufsize, LV_DISP_RENDER_MODE_PARTIAL);  //LV_DISP_RENDER_MODE_PARTIAL

      /* Initialize the input device driver */
      static lv_indev_t * indev = lv_indev_create();
      lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
      lv_indev_set_read_cb(indev, my_touchpad_read);

      init_ui();

      Serial.println("Setup done");

#ifdef LCD_DOUBLE_BUF
    }
#endif

  }
  last_ms = millis();
}

void loop() {
  delay(5);
  lv_timer_handler();
  }

//Adapted from https://docs.lvgl.io/master/examples.html
void init_ui()
{
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0a2b3f), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x114f70), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_set_style_text_color(scr, lv_color_hex(0xf4f7fa), LV_PART_MAIN);

  lv_obj_t *header = lv_obj_create(scr);
  lv_obj_set_size(header, lv_pct(100), 52);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(header, LV_OPA_70, LV_PART_MAIN);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x072032), LV_PART_MAIN);
  lv_obj_set_style_pad_hor(header, 14, LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "LVGL Showcase");
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

  status_clock_label = lv_label_create(header);
  lv_label_set_text(status_clock_label, "00:00");
  lv_obj_align(status_clock_label, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t *tabview = lv_tabview_create(scr);
  lv_obj_set_size(tabview, lv_pct(100), screenHeight - 52);
  lv_obj_set_pos(tabview, 0, 52);

  lv_obj_t *tab_home = lv_tabview_add_tab(tabview, "Home");
  lv_obj_t *tab_controls = lv_tabview_add_tab(tabview, "Controls");
  lv_obj_t *tab_stats = lv_tabview_add_tab(tabview, "Stats");

  lv_obj_set_style_pad_all(tab_home, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_all(tab_controls, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_all(tab_stats, 12, LV_PART_MAIN);

  lv_obj_t *hero = lv_obj_create(tab_home);
  lv_obj_set_size(hero, lv_pct(100), 120);
  lv_obj_set_style_bg_color(hero, lv_color_hex(0x1b6c95), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(hero, lv_color_hex(0x239ec9), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(hero, LV_GRAD_DIR_HOR, LV_PART_MAIN);
  lv_obj_set_style_border_width(hero, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(hero, 12, LV_PART_MAIN);

  lv_obj_t *hero_title = lv_label_create(hero);
  lv_label_set_text(hero_title, "Touch UI Demo");
  lv_obj_align(hero_title, LV_ALIGN_TOP_LEFT, 10, 10);

  lv_obj_t *hero_sub = lv_label_create(hero);
  lv_label_set_text(hero_sub, "Navigation, live status and controls");
  lv_obj_set_style_text_font(hero_sub, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(hero_sub, LV_ALIGN_TOP_LEFT, 10, 48);

  lv_obj_t *actions = lv_obj_create(tab_home);
  lv_obj_set_size(actions, lv_pct(100), 120);
  lv_obj_align_to(actions, hero, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);
  lv_obj_set_style_bg_opa(actions, LV_OPA_40, LV_PART_MAIN);
  lv_obj_set_style_radius(actions, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(actions, 0, LV_PART_MAIN);
  lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_all(actions, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_gap(actions, 10, LV_PART_MAIN);

  const char *buttons[] = {"Scan", "Sync", "Deploy"};
  for (uint8_t i = 0; i < 3; i++)
  {
    lv_obj_t *btn = lv_button_create(actions);
    lv_obj_set_size(btn, 92, 56);
    lv_obj_add_event_cb(btn, on_nav_button_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, buttons[i]);
    lv_obj_center(btn_lbl);
  }

  lv_obj_t *brightness_card = lv_obj_create(tab_controls);
  lv_obj_set_size(brightness_card, lv_pct(100), 90);
  lv_obj_set_style_bg_opa(brightness_card, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_border_width(brightness_card, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(brightness_card, 12, LV_PART_MAIN);

  lv_obj_t *brightness_title = lv_label_create(brightness_card);
  lv_label_set_text(brightness_title, "Brightness");
  lv_obj_align(brightness_title, LV_ALIGN_TOP_LEFT, 8, 6);

  lv_obj_t *brightness_slider = lv_slider_create(brightness_card);
  lv_obj_set_width(brightness_slider, lv_pct(70));
  lv_slider_set_range(brightness_slider, 0, 100);
  lv_slider_set_value(brightness_slider, 70, LV_ANIM_OFF);
  lv_obj_align(brightness_slider, LV_ALIGN_BOTTOM_LEFT, 8, -10);
  lv_obj_add_event_cb(brightness_slider, on_brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);

  brightness_value_label = lv_label_create(brightness_card);
  lv_label_set_text(brightness_value_label, "70%");
  lv_obj_align(brightness_value_label, LV_ALIGN_BOTTOM_RIGHT, -8, -10);

  lv_obj_t *switch_card = lv_obj_create(tab_controls);
  lv_obj_set_size(switch_card, lv_pct(100), 90);
  lv_obj_align_to(switch_card, brightness_card, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);
  lv_obj_set_style_bg_opa(switch_card, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_border_width(switch_card, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(switch_card, 12, LV_PART_MAIN);

  lv_obj_t *switch_title = lv_label_create(switch_card);
  lv_label_set_text(switch_title, "Performance mode");
  lv_obj_align(switch_title, LV_ALIGN_LEFT_MID, 8, 0);

  lv_obj_t *perf_switch = lv_switch_create(switch_card);
  lv_obj_align(perf_switch, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_add_state(perf_switch, LV_STATE_CHECKED);
  lv_obj_add_event_cb(perf_switch, on_toggle_changed, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *load_card = lv_obj_create(tab_stats);
  lv_obj_set_size(load_card, lv_pct(100), 110);
  lv_obj_set_style_bg_opa(load_card, LV_OPA_30, LV_PART_MAIN);
  lv_obj_set_style_border_width(load_card, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(load_card, 12, LV_PART_MAIN);

  load_label = lv_label_create(load_card);
  lv_label_set_text(load_label, "Render load: 0%");
  lv_obj_align(load_label, LV_ALIGN_TOP_LEFT, 8, 8);

  load_bar = lv_bar_create(load_card);
  lv_obj_set_size(load_bar, lv_pct(95), 24);
  lv_obj_align(load_bar, LV_ALIGN_BOTTOM_MID, 0, -12);
  lv_bar_set_range(load_bar, 0, 100);
  lv_bar_set_value(load_bar, 0, LV_ANIM_OFF);

  lv_obj_t *hint = lv_label_create(tab_stats);
  lv_label_set_text(hint, "Live values update every second");
  lv_obj_align_to(hint, load_card, LV_ALIGN_OUT_BOTTOM_LEFT, 2, 10);

  demo_seconds = 0;
  ui_timer = lv_timer_create(ui_tick_cb, 1000, NULL);
}