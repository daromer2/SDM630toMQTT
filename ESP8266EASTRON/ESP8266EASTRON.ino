/*
 * Eastron energy counter bridge. 
 * SDM 230, SDM 630
 * Eastron -> RS-485 -> esp-8266 -> WiFi -> MQTT
 * 
 * original repository here https://github.com/merlokk/SmartHome/tree/master/ESP8266EASTRON
 * (c) Oleg Moiseenko 2017
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>        // https://github.com/esp8266/Arduino
#include <xlogger.h>            // logger https://github.com/merlokk/xlogger
// my libraries
#include <etools.h>
#include <pitimer.h>     // timers
#include <modbuspoll.h>
#include <xparam.h>
#include "general.h"

#define               PROGRAM_VERSION   "1.01"

#define               DEBUG                            // enable debugging
#define               DEBUG_SERIAL      logger

// device modbus address
#define               MODBUS_ADDRESS    1
#define               CONNECTED_OBJ     eas.Connected

#define               MQTT_DEFAULT_TOPIC "PowerMeter"

// poll
#define MILLIS_TO_POLL          15*1000       //max time to wait for poll input registers (regular poll)
#define MILLIS_TO_POLL_HOLD_REG 15*60*1000    //max time to wait for poll all
// timers
#define TID_POLL                0x0001        // timer UID for regular poll 
#define TID_HOLD_REG            0x0002        // timer UID for poll all the registers

// LEDs and pins
#define PIN_PGM  0      // programming pin and jumper
#define LED1     12     // led 1
#define LED2     14     // led 2
#define LEDON    LOW
#define LEDOFF   HIGH

// objects
const int        units = 4;
ModbusPoll      eas;
ModbusPoll       eastron[units];
piTimer          ptimer;

///////////////////////////////////////////////////////////////////////////
//   Setup() and loop()
///////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.setDebugOutput(false);
  Serial1.setDebugOutput(true);
  Serial.begin(9600); //74880
  Serial1.begin(230400); // high speed logging port

  generalSetup();

  //Timers
  ptimer.Add(TID_POLL, MILLIS_TO_POLL);
  ptimer.Add(TID_HOLD_REG, MILLIS_TO_POLL_HOLD_REG);

  // LED init
  pinMode(LED1, OUTPUT);    
  pinMode(LED2, OUTPUT);   
  digitalWrite(LED1, LEDOFF);
  digitalWrite(LED2, LEDOFF);

  // eastron setup
  for (int i = 1; i < units; i++) {
    eastron[i].SetDeviceAddress(MODBUS_ADDRESS);
    eastron[i].SetLogger(&logger);
    DEBUG_PRINT(F("DeviceType: "));
    DEBUG_PRINTLN(params[F("device_type")]);
    eastron[i].ModbusSetup(params[F("device_type")].c_str());


  }


 // String str;
 // eastron[.getStrModbusConfig(str);
 // DEBUG_PRINTLN(F("Modbus config:"));
 // DEBUG_PRINTLN(str);

  // set password in work mode
  if (params[F("device_passwd")].length() > 0)
    logger.setPassword(params[F("device_passwd")].c_str());

  ticker.detach();
  digitalWrite(LED1, LEDOFF);
}

// the loop function runs over and over again forever
void loop() {
  digitalWrite(LED2, LEDON);
  if (!generalLoop()) {
    digitalWrite(LED2, LEDOFF);
    return;
  }



  if (ptimer.isArmed(TID_POLL)) { //Lets poll the meters now
    // modbus poll function
    bool pol = false;
    if (ptimer.isArmed(TID_HOLD_REG)) { //Lets ooll all registers instead
      pol = true;
      ptimer.Reset(TID_HOLD_REG);
    } 
      
    // publish some system vars
    mqtt.BeginPublish();
    mqttPublishRegularState();

  
    for (int i = 1; i <units + 1; i++) {  
    
      if (pol) { 
        eastron[i].Poll(POLL_ALL);
      } else {
        eastron[i].Poll(POLL_INPUT_REGISTERS);
      }
    
      yield();
  
      // publish vars from configuration
      if (eastron[i].mapConfigLen && eastron[i].mapConfig && eastron[i].Connected) {
        String str;
        for(int j = 0; j < eastron[i].mapConfigLen; j++) {
          eastron[i].getValue(
            str,
            eastron[i].mapConfig[j].command,
            eastron[i].mapConfig[j].modbusAddress,
            eastron[i].mapConfig[j].valueType);
            String topic = eastron[i].mapConfig[j].mqttTopicName;
            topic = String(i) + "/" + topic;
            mqtt.PublishState(topic, str);  
            DEBUG_PRINTLN(topic);      
        }    
      };
      
    }
    
    ptimer.Reset(TID_POLL);
    mqtt.Commit();
  }
  
    

  digitalWrite(LED2, LEDOFF);
  delay(100);
}


