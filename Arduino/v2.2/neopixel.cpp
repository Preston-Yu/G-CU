#include "GCU.h"


#if ESP_ARDUINO_VERSION_MAJOR >= 3 || (defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5)

void neopixelWrite(uint8_t red_val, uint8_t green_val, uint8_t blue_val){
  rgbLedWriteOrdered(GCU_LED_PIN, RGB_BUILTIN_LED_COLOR_ORDER, red_val, green_val, blue_val);
}

#else

//LED Using GPIO38
void neopixelWrite(uint8_t red_val, uint8_t green_val, uint8_t blue_val){
  rmt_data_t led_data[24];
  static rmt_obj_t* rmt_send = NULL;
  static bool initialized = false;

  uint8_t _pin = GCU_LED_PIN;

  if(!initialized){
    if((rmt_send = rmtInit(_pin, RMT_TX_MODE, RMT_MEM_64)) == NULL){
        log_e("RGB LED driver initialization failed!");
        rmt_send = NULL;
        return;
    }
    rmtSetTick(rmt_send, 100);
    initialized = true;
  }

  int color[] = {green_val, red_val, blue_val};  // Color coding is in order GREEN, RED, BLUE
  int i = 0;
  for(int col=0; col<3; col++ ){
    for(int bit=0; bit<8; bit++){
      if((color[col] & (1<<(7-bit)))){
        // HIGH bit
        led_data[i].level0 = 1; // T1H
        led_data[i].duration0 = 8; // 0.8us
        led_data[i].level1 = 0; // T1L
        led_data[i].duration1 = 4; // 0.4us
      }else{
        // LOW bit
        led_data[i].level0 = 1; // T0H
        led_data[i].duration0 = 4; // 0.4us
        led_data[i].level1 = 0; // T0L
        led_data[i].duration1 = 8; // 0.8us
      }
      i++;
    }
  }
  rmtWrite(rmt_send, led_data, 24);
}
#endif

void neopixelBlink(uint8_t red_val, uint8_t green_val, uint8_t blue_val, uint8_t cycle_times, uint16_t delayms){

  for(uint16_t cycle_couts = 0; cycle_couts < cycle_times; cycle_couts++){
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms / 2);
    neopixelWrite(0, 0, 0);
    delay(delayms / 2);
  }
}

void neopixelRainbow(uint8_t delayms){

  uint8_t red_val = GCU_RGB_BRIGHTNESS;  // 初始红色值最大
  uint8_t green_val = 0; // 初始绿色值为0
  uint8_t blue_val = 0;  // 初始蓝色值为0

  // 从红变黄（增加绿色）
  for(green_val = 0; green_val < GCU_RGB_BRIGHTNESS; green_val++) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms); // 短暂延时以便观察变化
  }

  // 从黄变绿（减少红色）
  for(red_val = GCU_RGB_BRIGHTNESS; red_val > 0; red_val--) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms);
  }

  // 从绿变青（增加蓝色）
  for(blue_val = 0; blue_val < GCU_RGB_BRIGHTNESS; blue_val++) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms);
  }

  // 从青变蓝（减少绿色）
  for(green_val = GCU_RGB_BRIGHTNESS; green_val > 0; green_val--) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms);
  }

  // 从蓝变紫（增加红色）
  for(red_val = 0; red_val < GCU_RGB_BRIGHTNESS; red_val++) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms);
  }

  // 从紫变回红（减少蓝色）
  for(blue_val = GCU_RGB_BRIGHTNESS; blue_val > 0; blue_val--) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms);
  }
}