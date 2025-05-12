#include "GCU.h"


void normalizedCalibrationInit(unsigned char method)
{

  unsigned long calibration_num = (calibration_duration / 1000) * device_frequency;
  unsigned long calibration_delaytimes_ms = 1000 / device_frequency;
  Serial.print("calibration_num: ");Serial.print(calibration_num); Serial.println("  couts");
  Serial.print("calibration_delaytimes_ms: ");Serial.print(calibration_delaytimes_ms); Serial.println("  ms");

  if(method == normalized_calibration_method_peak){

    neopixelWrite(GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS,0);
    for(uint8_t i = 0; i < sensors_num; i++){
      maxMillVolts[i] = 0; // Initialize max values to 0
      minMillVolts[i] = 3300; // Initialize min values to maximum possible reading (assuming 10-bit ADC)
    }
    delay(5000);
    neopixelWrite(GCU_RGB_BRIGHTNESS,0,GCU_RGB_BRIGHTNESS);

    // Begin sampling for 5 seconds
    unsigned long startTime = millis();
    while(millis() - startTime < calibration_duration){
      for(uint8_t i = 0; i < sensors_rows_num; i++){
        for(uint8_t j = 0; j < sensors_columns_num; j++){
          pinMode(SelectIO[j],OUTPUT);
          digitalWrite(SelectIO[j], LOW);

          uint16_t currentReading = analogReadMilliVolts(analogReadIO[i]); // Read the current value

          pinMode(SelectIO[j],INPUT);
          
          // Update max and min arrays
          uint8_t index = i * 10 + j;
          if(currentReading > maxMillVolts[index]){
            maxMillVolts[index] = currentReading * (1 + normalized_calibration_max_factor);
          }
          if(currentReading < minMillVolts[index]){
            minMillVolts[index] = currentReading * (1 - normalized_calibration_min_factor);
          }
        }
      }
      delay(calibration_delaytimes_ms); // Delay between samples
    }

    // Print max and min values for debugging
    Serial.println("maxMillVolts(peak): ");
    for(uint8_t i = 0; i < sensors_num; i++){
      Serial.print(maxMillVolts[i]); Serial.print("  ");
    }
    Serial.println("  ");
    Serial.println("minMillVolts(peak): ");
    for(uint8_t i = 0; i < sensors_num; i++){
      Serial.print(minMillVolts[i]); Serial.print("  ");
    }
    Serial.println("  ");




  }
  else if(method == normalized_calibration_method_mean){
    neopixelWrite(GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS,0);
    delay(3000);
    neopixelWrite(GCU_RGB_BRIGHTNESS,0,GCU_RGB_BRIGHTNESS);//Purple

    for(uint16_t calibration_count = 0; calibration_count < calibration_num; calibration_count++){
      for(uint8_t i = 0; i < sensors_rows_num; i++){
        for(uint8_t j = 0; j < sensors_columns_num; j++){
          pinMode(SelectIO[j],OUTPUT);
          digitalWrite(SelectIO[j], LOW);
          
          maxMillVolts[i * 10 + j] += ((((float)analogReadMilliVolts(analogReadIO[i])) / ((float)calibration_num)) * (1 + normalized_calibration_max_factor));

          pinMode(SelectIO[j],INPUT);
        }
      }
      delay(calibration_delaytimes_ms);
      // Serial.print("calibration_counts: ");Serial.print(calibration_count); Serial.println("  times");
    }


    Serial.print("maxMillVolts: ");
    for(uint16_t i = 0; i < sensors_num; i++){
      Serial.print(maxMillVolts[i]); Serial.print("  ");
    }
    Serial.println("  ");

    neopixelBlink(GCU_RGB_BRIGHTNESS, GCU_RGB_BRIGHTNESS, 0, 5, 1000);
    neopixelWrite(GCU_RGB_BRIGHTNESS, 0, GCU_RGB_BRIGHTNESS);//Purple

    for(uint16_t calibration_count = 0; calibration_count < calibration_num; calibration_count++){
      for(uint8_t i = 0; i < sensors_rows_num; i++){
        for(uint8_t j = 0; j < sensors_columns_num; j++){
          pinMode(SelectIO[j],OUTPUT);
          digitalWrite(SelectIO[j], LOW);
          
          minMillVolts[i * 10 + j] += ((((float)analogReadMilliVolts(analogReadIO[i])) / ((float)calibration_num)) * (1 - normalized_calibration_min_factor));

          pinMode(SelectIO[j],INPUT);
        }
      }
      delay(calibration_delaytimes_ms);
      // Serial.print("calibration_counts: ");Serial.print(calibration_count); Serial.println("  times");
    }

    Serial.print("minMillVolts: ");
    for(uint16_t i = 0; i < sensors_num; i++){
      Serial.print(minMillVolts[i]); Serial.print("  ");
    }
    Serial.println("  ");
  }

  for(uint16_t i = 0; i < sensors_num; i++){
    if(maxMillVolts[i] <= minMillVolts[i]){
       while(1){
         neopixelWrite(GCU_RGB_BRIGHTNESS, GCU_RGB_BRIGHTNESS, 0);
         delay(500);
         neopixelWrite(GCU_RGB_BRIGHTNESS, 0, GCU_RGB_BRIGHTNESS);
         delay(500);
         Serial.println("normalizedCalibrationInit: Error");
       }
     }
  }

  neopixelWrite(GCU_RGB_BRIGHTNESS, GCU_RGB_BRIGHTNESS, 0);
  delay(3000);

  
}





float normalizedCalibrationCalculate(float milliVolts, uint16_t sensors_number)
{
  return ((milliVolts - (minMillVolts[sensors_number])) / (maxMillVolts[sensors_number] - minMillVolts[sensors_number]));
}

void on_timer() {
  data_ready = true;
}


void dataReceive(){
  unsigned long t_start = micros();
  //    const uint16_t port = 80;
  //    const char * host = "192.168.1.1"; // ip or dns
  unsigned int sys_epoch;
  unsigned short sys_millis;

  
  
  extern DFRobot_BMX160 bmx160;
  

  // Use WiFiClient class to create TCP connections
  if(TCP_UDP_Flag){  
    if(!client.connected()){
      if(!client.connect(SeverIP, port)) {
        Serial.print("Connection failed to");
        Serial.println(SeverIP);
        if(working_flag){
          neopixelWrite(GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS,0); 
          working_flag = 0;
        }
        return;
      }
    }
  }


  if(!working_flag){
    neopixelWrite(0,GCU_RGB_BRIGHTNESS,0);
    working_flag = 1;
  }

  if(timestamp_flag){
    sys_epoch = rtc.getEpoch();
    // Serial.println(sys_epoch);
    memcpy(data_p, &sys_epoch, sizeof(sys_epoch));
    data_p += sizeof(sys_epoch);


    sys_millis = rtc.getMillis();
    // Serial.println(sys_millis);
    memcpy(data_p, &sys_millis, sizeof(sys_millis));
    data_p += sizeof(sys_millis);

  }
  
  for(uint8_t i = 0; i < sensors_rows_num; i++){
    for(uint8_t j = 0; j < sensors_columns_num; j++){
      pinMode(SelectIO[j],OUTPUT);
      digitalWrite(SelectIO[j], LOW);
      
      if(normalized_calibration_flag){
        float sensorData = normalizedCalibrationCalculate(((float)analogReadMilliVolts(analogReadIO[i])), (i * 10 + j));
        Serial.println(sensorData);
        memcpy(data_p, &sensorData, sizeof(sensorData));
        data_p+=sizeof(sensorData);
      }
      else if (sensors_dataformat == Four_Bytes_Sensors_Data){
        unsigned long sensorData = analogReadMilliVolts(analogReadIO[i]);
        memcpy(data_p, &sensorData, sizeof(sensorData));
        data_p+=sizeof(sensorData);
      }
      else{
        unsigned short sensorData = analogReadMilliVolts(analogReadIO[i]);
        memcpy(data_p, &sensorData, sizeof(sensorData));
        data_p+=sizeof(sensorData);
      }


      pinMode(SelectIO[j],INPUT);
      yield();
    }
  }


  if (IMU_flag){
    if (IMU_chip){
      

      

      memcpy(data_p, &BMI270_BMM150_magn_x, sizeof(BMI270_BMM150_magn_x));
      data_p += sizeof(BMI270_BMM150_magn_x);
      memcpy(data_p, &BMI270_BMM150_magn_y, sizeof(BMI270_BMM150_magn_y));
      data_p += sizeof(BMI270_BMM150_magn_y);
      memcpy(data_p, &BMI270_BMM150_magn_z, sizeof(BMI270_BMM150_magn_z));
      data_p += sizeof(BMI270_BMM150_magn_z);

      memcpy(data_p, &BMI270_BMM150_gyro_x, sizeof(BMI270_BMM150_gyro_x));
      data_p += sizeof(BMI270_BMM150_gyro_x);
      memcpy(data_p, &BMI270_BMM150_gyro_y, sizeof(BMI270_BMM150_gyro_y));
      data_p += sizeof(BMI270_BMM150_gyro_y);
      memcpy(data_p, &BMI270_BMM150_gyro_z, sizeof(BMI270_BMM150_gyro_z));
      data_p += sizeof(BMI270_BMM150_gyro_z);

      memcpy(data_p, &BMI270_BMM150_accel_x, sizeof(BMI270_BMM150_accel_x));
      data_p += sizeof(BMI270_BMM150_accel_x);
      memcpy(data_p, &BMI270_BMM150_accel_y, sizeof(BMI270_BMM150_accel_y));
      data_p += sizeof(BMI270_BMM150_accel_y);
      memcpy(data_p, &BMI270_BMM150_accel_z, sizeof(BMI270_BMM150_accel_z));
      data_p += sizeof(BMI270_BMM150_accel_z);



    }
    else{
      

      memcpy(data_p, &Omagn.x, sizeof(Omagn.x));
      data_p += sizeof(Omagn.x);
      memcpy(data_p, &Omagn.y, sizeof(Omagn.y));
      data_p += sizeof(Omagn.y);
      memcpy(data_p, &Omagn.z, sizeof(Omagn.z));
      data_p += sizeof(Omagn.z);

      memcpy(data_p, &Ogyro.x, sizeof(Ogyro.x));
      data_p += sizeof(Ogyro.x);
      memcpy(data_p, &Ogyro.y, sizeof(Ogyro.y));
      data_p += sizeof(Ogyro.y);
      memcpy(data_p, &Ogyro.z, sizeof(Ogyro.z));
      data_p += sizeof(Ogyro.z);

      memcpy(data_p, &Oaccel.x, sizeof(Oaccel.x));
      data_p += sizeof(Oaccel.x);
      memcpy(data_p, &Oaccel.y, sizeof(Oaccel.y));
      data_p += sizeof(Oaccel.y);
      memcpy(data_p, &Oaccel.z, sizeof(Oaccel.z));
      data_p += sizeof(Oaccel.z);
    }
  }
  

  if(sensors_dataformat == Four_Bytes_Sensors_Data){
    data_p -= sensors_num * 4 + timestamp_flag * 6 + IMU_flag * 36;
  }
  else{
    data_p -= sensors_num * 2 + timestamp_flag * 6 + IMU_flag * 36;
  }
  

  if(TCP_UDP_Flag){
    client.write(data,data_num);
  }
  else{
    udp.beginPacket(SeverIP,port);
    udp.write(data,data_num);
    udp.endPacket();
  }


/*
  if (IMU_flag){
    if (IMU_chip){
      // Display the magnetometer results (magn is magnetometer in uTesla)
      Serial.print("M ");
      Serial.print("X: "); Serial.print(BMI270_BMM150_magn_x); Serial.print("  ");
      Serial.print("Y: "); Serial.print(BMI270_BMM150_magn_y); Serial.print("  ");
      Serial.print("Z: "); Serial.print(BMI270_BMM150_magn_z); Serial.print("  ");
      Serial.println("uT");

      // Display the gyroscope results (gyroscope data is in degrees/second)
      Serial.print("G ");
      Serial.print("X: "); Serial.print(BMI270_BMM150_gyro_x); Serial.print("  ");
      Serial.print("Y: "); Serial.print(BMI270_BMM150_gyro_y); Serial.print("  ");
      Serial.print("Z: "); Serial.print(BMI270_BMM150_gyro_z); Serial.print("  ");
      Serial.println("degrees/second");
      
      // Display the accelerometer results (accelerometer data is in G's)
      Serial.print("A ");
      Serial.print("X: "); Serial.print(BMI270_BMM150_accel_x); Serial.print("  ");
      Serial.print("Y: "); Serial.print(BMI270_BMM150_accel_y); Serial.print("  ");
      Serial.print("Z: "); Serial.print(BMI270_BMM150_accel_z); Serial.print("  ");
      Serial.println("G's");

      Serial.println("");
    }
    else{
      // Display the magnetometer results (magn is magnetometer in uTesla)
      Serial.print("M ");
      Serial.print("X: "); Serial.print(Omagn.x); Serial.print("  ");
      Serial.print("Y: "); Serial.print(Omagn.y); Serial.print("  ");
      Serial.print("Z: "); Serial.print(Omagn.z); Serial.print("  ");
      Serial.println("uT");

      // Display the gyroscope results (gyroscope data is in g)
      Serial.print("G ");
      Serial.print("X: "); Serial.print(Ogyro.x); Serial.print("  ");
      Serial.print("Y: "); Serial.print(Ogyro.y); Serial.print("  ");
      Serial.print("Z: "); Serial.print(Ogyro.z); Serial.print("  ");
      Serial.println("g");
      
      // Display the accelerometer results (accelerometer data is in m/s^2)
      Serial.print("A ");
      Serial.print("X: "); Serial.print(Oaccel.x); Serial.print("  ");
      Serial.print("Y: "); Serial.print(Oaccel.y); Serial.print("  ");
      Serial.print("Z: "); Serial.print(Oaccel.z); Serial.print("  ");
      Serial.println("m/s^2");

      Serial.println("");
    } 
  }
  */
  unsigned long t_end = micros();
Serial.print("dataReceive 执行耗时：");
Serial.print(t_end - t_start);
Serial.println(" µs");
}


