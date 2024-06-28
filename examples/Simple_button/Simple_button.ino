#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "ui.h"
#include "ui_helpers.h"

// setting PWM properties
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;
int dutyCycle = 255;
byte dimmedDutyCycle = 15;

bool printTouchData = 1;
unsigned long touchedTime = 0;
bool lcdTouched = 1;
unsigned long dimmingInterval = 10000;

#define TFT_BL 2
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    14 /* R0 */, 21 /* R1 */, 47 /* R2 */, 48 /* R3 */, 45 /* R4 */,
    9 /* G0 */, 46 /* G1 */, 3 /* G2 */, 8 /* G3 */, 16 /* G4 */, 1 /* G5 */,
    15 /* B0 */, 7 /* B1 */, 6 /* B2 */, 5 /* B3 */, 4 /* B4 */,
    0 /* hsync_polarity */, 180 /* hsync_front_porch */, 30 /* hsync_pulse_width */, 16 /* hsync_back_porch */,
    0 /* vsync_polarity */, 12 /* vsync_front_porch */, 13 /* vsync_pulse_width */, 10 /* vsync_back_porch */);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    800 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */);
    
#include "touch.h"

/* Change to your screen resolution */
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char *buf) {
  Serial.printf(buf);
  Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  if (touch_has_signal()) {
    if (touch_touched()) {

      //light up backlight to full power
      dutyCycle = 255;
      ledcWrite(ledChannel, dutyCycle);

      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;

      touchedTime = millis();
      lcdTouched = 1;
      if (printTouchData) {
        Serial.print("Data x ");
        Serial.println(data->point.x);
        Serial.print("Data y ");
        Serial.println(data->point.y);
      }
    } else if (touch_released()) {
      data->state = LV_INDEV_STATE_REL;
    }
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void dimmLCD() {
  if (lcdTouched != 0) {
    if (millis() - touchedTime > dimmingInterval) {
      for (byte i = dutyCycle; i >= dimmedDutyCycle; i--) {
        dutyCycle = i;
        ledcWrite(ledChannel, dutyCycle);
      }
      lcdTouched = 0;
      Serial.println("Dimming");
    }
  }
}

void setup() {

  // configure LED PWM functionalitites
  ledcSetup(ledChannel, freq, resolution);

  // attach the channel to the GPIO to be controlled
  ledcAttachPin(TFT_BL, ledChannel);

  //write to the channel dutyCycle value
  ledcWrite(ledChannel, dutyCycle);

  Serial.begin(115200); /* prepare for possible serial debug */

  // Init Display
  gfx->begin();

  lv_init();

  // Init touch device
  pinMode(TOUCH_GT911_RST, OUTPUT);
  digitalWrite(TOUCH_GT911_RST, LOW);
  delay(10);
  digitalWrite(TOUCH_GT911_RST, HIGH);
  delay(10);
  touch_init();
  //  touch.setTouch( calData );

  screenWidth = gfx->width();
  screenHeight = gfx->height();

  disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 4, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);


#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

  if (!disp_draw_buf) {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  } else {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 4);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Initialize the (dummy) input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    Serial.println("Setup done");
    ui_init();
  }
}

void loop() {
  dimmLCD();
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);
}
