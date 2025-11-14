#include "GCU.h"


#if ESP_ARDUINO_VERSION_MAJOR >= 3 || (defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5)

/**
 * @brief Drive the onboard RGB LED using the Arduino core helper.
 * @param red_val Red channel 0-255.
 * @param green_val Green channel 0-255.
 * @param blue_val Blue channel 0-255.
 */
void neopixelWrite(uint8_t red_val, uint8_t green_val, uint8_t blue_val){
  rgbLedWriteOrdered(GCU_LED_PIN, RGB_BUILTIN_LED_COLOR_ORDER, red_val, green_val, blue_val);
}

#else

//LED Using GPIO38
/**
 * @brief Drive the onboard RGB LED using the legacy RMT bit-banging path.
 * @param red_val Red channel 0-255.
 * @param green_val Green channel 0-255.
 * @param blue_val Blue channel 0-255.
 */
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

/**
 * @brief Blink the RGB LED with the requested color for a number of cycles.
 * @param red_val Red component.
 * @param green_val Green component.
 * @param blue_val Blue component.
 * @param cycle_times Number of blink cycles to perform.
 * @param delayms Milliseconds per on/off half-cycle.
 */
void neopixelBlink(uint8_t red_val, uint8_t green_val, uint8_t blue_val, uint8_t cycle_times, uint16_t delayms){

  for(uint16_t cycle_couts = 0; cycle_couts < cycle_times; cycle_couts++){
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms / 2);
    neopixelWrite(0, 0, 0);
    delay(delayms / 2);
  }
}

/**
 * @brief Run a blocking rainbow animation across the RGB LED.
 * @param delayms Delay between color steps (ms).
 */
void neopixelRainbow(uint8_t delayms){

  // Initialize primary colors (start at red)
  uint8_t red_val = GCU_RGB_BRIGHTNESS;  // Initial red value (max)
  uint8_t green_val = 0;                 // Initial green value
  uint8_t blue_val = 0;                  // Initial blue value

  // Red -> Yellow (increase green)
  for (green_val = 0; green_val < GCU_RGB_BRIGHTNESS; green_val++) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms); // Short delay to visualize change
  }

  // Yellow -> Green (decrease red)
  for (red_val = GCU_RGB_BRIGHTNESS; red_val > 0; red_val--) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms);
  }

  // Green -> Cyan (increase blue)
  for (blue_val = 0; blue_val < GCU_RGB_BRIGHTNESS; blue_val++) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms);
  }

  // Cyan -> Blue (decrease green)
  for (green_val = GCU_RGB_BRIGHTNESS; green_val > 0; green_val--) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms);
  }

  // Blue -> Purple (increase red)
  for (red_val = 0; red_val < GCU_RGB_BRIGHTNESS; red_val++) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms);
  }

  // Purple -> Red (decrease blue)
  for (blue_val = GCU_RGB_BRIGHTNESS; blue_val > 0; blue_val--) {
    neopixelWrite(red_val, green_val, blue_val);
    delay(delayms);
  }
}
