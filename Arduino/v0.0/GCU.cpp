#include "esp32-hal-rgb-led.h"
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




//RTC
bool init_RTC_from_net(){


  struct tm timeinfo1;
  struct tm timeinfo2;
  time_t epochTime;

  sntp_servermode_dhcp(1);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);


  if(!getLocalTime(&timeinfo1)){
    Serial.println("No time available (yet)");
    return false;
  }
  do
  {
    
    if(!getLocalTime(&timeinfo2)){
    Serial.println("No time available (yet)");
    return false;
    }
    Serial.print(".");
  }
  while(timeinfo2.tm_sec==timeinfo1.tm_sec);
  //Serial.println(&timeinfo2, "%A, %B %d %Y %H:%M:%S");
  epochTime = mktime(&timeinfo2);
  rtc.setTime(epochTime);
  write_tm_to_bq32002(timeinfo2);
  Serial.print("\n");
  Serial.println(epochTime);
  // Serial.println(rtc.getMicros());
  Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));
  return true;
}

bool init_RTC_from_bq32002(){

  struct tm timeinfo1;
  struct tm timeinfo2;
  time_t epochTime;
  extern ESP32Time rtc;

  if(!check_bq32002_status()){
    RTC_error();
  }
  timeinfo1 = read_tm_from_bq32002();
  Serial.println(&timeinfo1, "%A, %B %d %Y %H:%M:%S");
  do{
    timeinfo2 = read_tm_from_bq32002();
    Serial.print(".");
    Serial.println(&timeinfo2, "%A, %B %d %Y %H:%M:%S");
  }
  while(timeinfo2.tm_sec==timeinfo1.tm_sec);
  epochTime = mktime(&timeinfo2);
  rtc.setTime(epochTime);
  Serial.print("\n");
  Serial.println(epochTime);
  // Serial.println(rtc.getMicros());
  Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));
  neopixelWrite38(0,0,0);
  return true;

}

bool check_bq32002_status(){

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_MINUTES_Register); //Set pointer to register 0 (seconds)
  Wire.endTransmission();
  Wire.requestFrom(BQ32002_I2C_ADDRESS, 1);
  if (Wire.read() & 0x80){
    return false;
  }
  return true;

}

void RTC_error(){
  while(1){
    neopixelWrite38(RGB_BRIGHTNESS-50,0,0);
    delay(500);
    neopixelWrite38(0,0,0);
    delay(500);
  }
}

struct tm bq32002tm_to_tm(struct bq32002_tm _bq32002_tm){

  struct tm _timeinfo;

  _timeinfo.tm_sec = (10 * ((_bq32002_tm.secs & 0x70) >> 4)) + (_bq32002_tm.secs & 0x0f);
  _timeinfo.tm_min = (10 * ((_bq32002_tm.mins & 0x70) >> 4)) + (_bq32002_tm.mins & 0x0f);
  _timeinfo.tm_hour = (10 * ((_bq32002_tm.cent_hours & 0x30) >> 4)) + (_bq32002_tm.cent_hours & 0x0f);
  _timeinfo.tm_wday = _bq32002_tm.day & 0x07;
  _timeinfo.tm_mday = (10 * ((_bq32002_tm.date & 0x30) >> 4)) + (_bq32002_tm.date & 0x0f);
  _timeinfo.tm_mon = (10 * ((_bq32002_tm.month & 0x10) >> 4)) + (_bq32002_tm.month & 0x0f);
  _timeinfo.tm_year = (100 + 10 * ((_bq32002_tm.years & 0xf0) >> 4)) + (_bq32002_tm.years & 0x0f);


  return _timeinfo;
}

struct bq32002_tm tm_to_bq32002tm(struct tm _timeinfo){

  struct bq32002_tm _bq32002tm;

  _bq32002tm.secs = (((_timeinfo.tm_sec / 10) << 4) + (_timeinfo.tm_sec % 10)) & 0x7f;
  _bq32002tm.mins = (((_timeinfo.tm_min / 10) << 4) + (_timeinfo.tm_min % 10)) & 0x7f;
  _bq32002tm.cent_hours = ((_timeinfo.tm_hour / 10) << 4) + (_timeinfo.tm_hour % 10);
  if ((_timeinfo.tm_year / 100) % 2 == 0){
    _bq32002tm.cent_hours |= 0x80;
  }
  else{
    _bq32002tm.cent_hours |= 0xc0;
  }
  _bq32002tm.day = _timeinfo.tm_wday;
  _bq32002tm.date = ((_timeinfo.tm_mday / 10) << 4) + (_timeinfo.tm_mday % 10) & 0x3f;
  _bq32002tm.month = ((_timeinfo.tm_mon / 10) << 4) + (_timeinfo.tm_mon % 10) & 0x3f;
  _bq32002tm.years = (((_timeinfo.tm_year / 10) % 10) << 4) + (_timeinfo.tm_year % 10);

  return _bq32002tm;
}

bool write_tm_to_bq32002(struct tm _timeinfo){

  struct bq32002_tm _bq32002tm;

  _bq32002tm = tm_to_bq32002tm(_timeinfo);

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_SECONDS_Register); //Set pointer to register 0 (seconds)
  Wire.write(_bq32002tm.secs); 
  Wire.endTransmission();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_MINUTES_Register); //Set pointer to register 0 (seconds)
  Wire.write(_bq32002tm.mins); 
  Wire.endTransmission();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_CENT_HOURS_Register); //Set pointer to register 0 (seconds)
  Wire.write(_bq32002tm.cent_hours); 
  Wire.endTransmission();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_DAY_Register); //Set pointer to register 0 (seconds)
  Wire.write(_bq32002tm.day); 
  Wire.endTransmission();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_DATE_Register); //Set pointer to register 0 (seconds)
  Wire.write(_bq32002tm.date); 
  Wire.endTransmission();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_MONTH_Register); //Set pointer to register 0 (seconds)
  Wire.write(_bq32002tm.month); 
  Wire.endTransmission();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_YEARS_Register); //Set pointer to register 0 (seconds)
  Wire.write(_bq32002tm.years); 
  Wire.endTransmission();

  return true;
}


struct tm read_tm_from_bq32002(){

  struct bq32002_tm _bq32002tm;

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_SECONDS_Register); //Set pointer to register 0 (seconds)
  Wire.endTransmission();
  Wire.requestFrom(BQ32002_I2C_ADDRESS, 1);
  _bq32002tm.secs = Wire.read();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_MINUTES_Register); //Set pointer to register 0 (seconds)
  Wire.endTransmission();
  Wire.requestFrom(BQ32002_I2C_ADDRESS, 1);
  _bq32002tm.mins = Wire.read();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_CENT_HOURS_Register); //Set pointer to register 0 (seconds)
  Wire.endTransmission();
  Wire.requestFrom(BQ32002_I2C_ADDRESS, 1);
  _bq32002tm.cent_hours = Wire.read();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_DAY_Register); //Set pointer to register 0 (seconds)
  Wire.endTransmission();
  Wire.requestFrom(BQ32002_I2C_ADDRESS, 1);
  _bq32002tm.day = Wire.read();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_DATE_Register); //Set pointer to register 0 (seconds)
  Wire.endTransmission();
  Wire.requestFrom(BQ32002_I2C_ADDRESS, 1);
  _bq32002tm.date = Wire.read();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_MONTH_Register); //Set pointer to register 0 (seconds)
  Wire.endTransmission();
  Wire.requestFrom(BQ32002_I2C_ADDRESS, 1);
  _bq32002tm.month = Wire.read();

  Wire.beginTransmission(BQ32002_I2C_ADDRESS);
  Wire.write(BQ32002_YEARS_Register); //Set pointer to register 0 (seconds)
  Wire.endTransmission();
  Wire.requestFrom(BQ32002_I2C_ADDRESS, 1);
  _bq32002tm.years = Wire.read();

  return bq32002tm_to_tm(_bq32002tm);
}

void data_receive(){
  //    const uint16_t port = 80;
  //    const char * host = "192.168.1.1"; // ip or dns
  unsigned long int sys_epoch;
  unsigned int sys_millis;

  // Use WiFiClient class to create TCP connections
  WiFiClient client;

  while (!client.connect(SeverIP, port)) {
    Serial.print("Connection failed to");
    Serial.println(SeverIP);
    neopixelWrite38(RGB_BRIGHTNESS-50,RGB_BRIGHTNESS-50,0); 
  }

  neopixelWrite38(0,RGB_BRIGHTNESS-50,0);

  if(timestamp_flag){
    sys_epoch = rtc.getEpoch();
    Serial.println(sys_epoch);
    for(uint8_t i = 4; i > 0; i--){
      *data_p = (sys_epoch >> (8 * (i - 1))) & 0xff;
      data_p++;
    }

    sys_millis = rtc.getMillis();
    Serial.println(sys_millis);
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

  client.write(data,data_num);
}

