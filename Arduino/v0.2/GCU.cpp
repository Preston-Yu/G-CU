#include "GCU.h"


//LED Using GPIO38
void neopixelWrite38(uint8_t red_val, uint8_t green_val, uint8_t blue_val){
  rmt_data_t led_data[24];
  static rmt_obj_t* rmt_send = NULL;
  static bool initialized = false;

  uint8_t _pin = 38;

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


void data_receive(){
  //    const uint16_t port = 80;
  //    const char * host = "192.168.1.1"; // ip or dns
  unsigned long int sys_epoch;
  unsigned int sys_millis;

  WiFiClient client;
  WiFiUDP udp;

  // Use WiFiClient class to create TCP connections
  if(TCP_UDP_Flag){  
    if(!client.connected()){
      if(!client.connect(SeverIP, port)) {
        Serial.print("Connection failed to");
        Serial.println(SeverIP);
        if(working_flag){
          neopixelWrite38(RGB_BRIGHTNESS-50,RGB_BRIGHTNESS-50,0); 
          working_flag = 0;
        }
        return;
      }
    }
  }


  if(!working_flag){
    neopixelWrite38(0,RGB_BRIGHTNESS-50,0);
    working_flag = 1;
  }

  if(timestamp_flag){
    sys_epoch = rtc.getEpoch();
    // Serial.println(sys_epoch);
    for(uint8_t i = 4; i > 0; i--){
      *data_p = (sys_epoch >> (8 * (i - 1))) & 0xff;
      data_p++;
    }

    sys_millis = rtc.getMillis();
    // Serial.println(sys_millis);
    for(uint8_t i = 2; i > 0; i--){
      *data_p = (sys_millis >> (8 * (i - 1))) & 0xff;
      data_p++;
    }
  }
  
  for(uint8_t i = 0; i < sensors_rows_num; i++){
    for(uint8_t j = 0; j < sensors_columns_num; j++){
      pinMode(SelectIO[j],OUTPUT);
      digitalWrite(SelectIO[j], LOW);
      int analogVolts = analogReadMilliVolts(analogReadIO[i]);
      *data_p = (analogVolts >> 8) & 0xff;
      data_p++;
      *data_p = analogVolts & 0xff;
      data_p++;
      pinMode(SelectIO[j],INPUT);
      if(data_validation_flag){
      check_sum += analogVolts; 
      }
    }
  }
  if(data_validation_flag){
    check_sum &= 0xffff; 
    *data_p = check_sum >> 8;
    data_p++;
    *data_p = check_sum & 0xff;
    data_p++;
  }
  data_p -= (sensors_num + data_validation_flag) * 2 + timestamp_flag * 6;

  if(TCP_UDP_Flag){
    client.write(data,data_num);
  }
  else{
    udp.beginPacket(SeverIP,port);
    udp.write(data,data_num);
    udp.endPacket();
  }
}

