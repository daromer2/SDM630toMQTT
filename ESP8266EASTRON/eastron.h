/*
 * Eastron bridge. 
 * Universal eastron reader.
 * 
 * (c) Oleg Moiseenko 2017
 */
 
#ifndef __EASTRON_H__
#define __EASTRON_H__

#include <Arduino.h>
#include "etools.h"
#include "emodbus.h"

#define SERIAL_BAUD                 9600       // baudrate
#define MODBUS_POLL_TIMEOUT         1000       // max time to wait for response from SDM

// Poll commands
#define POLL_ALL                    0
#define POLL_HOLDING_REGISTERS      MBReadHoldingRegisters
#define POLL_INPUT_REGISTERS        MBReadInputRegisters

// registers configuration
#define MDB_WORD        1
#define MDB_INT         2
#define MDB_INT64       3
#define MDB_FLOAT       4
#define MDB_FLOAT8      5
#define MDB_INT_HEX     6
#define MDB_INT64_HEX   7
#define MDB_8BYTE_HEX   8
#define MDB_16BYTE_HEX  9
struct mqttMapConfigS {
  const char * mqttTopicName;
  byte command;
  word modbusAddress;
  byte valueType;
};

// devices configuration
#define eastron220Len 5
extern mqttMapConfigS eastron220[eastron220Len];
#define eastron630smallLen 18
extern mqttMapConfigS eastron630small[eastron630smallLen];
//extern mqttMapConfigS eastron630[15];

///////////////////////////////////////////////////////////////////////////
//  EASTRON modbus addresses  
//  original here https://github.com/beireken/SDM220t/blob/master/SDM.h 
///////////////////////////////////////////////////////////////////////////

//SDM 220 registers
#define SDM220T_VOLTAGE                     0x0000                              //V
#define SDM220T_CURRENT                     0x0006                              //A
#define SDM220T_POWER                       0x000C                              //W
#define SDM220T_ACTIVE_APPARENT_POWER       0x0012                              //VA
#define SDM220T_REACTIVE_APPARENT_POWER     0x0018                              //VAR
#define SDM220T_POWER_FACTOR                0x001E                              //
#define SDM220T_PHASE_ANGLE                 0x0024                              //DEGREE
#define SDM220T_FREQUENCY                   0x0046                              //Hz
#define SDM220T_IMPORT_ACTIVE_ENERGY        0x0048                              //Wh
#define SDM220T_EXPORT_ACTIVE_ENERGY        0x004A                              //Wh
#define SDM220T_IMPORT_REACTIVE_ENERGY      0x004C                              //VARh
#define SDM220T_EXPORT_REACTIVE_ENERGY      0x004E                              //VARh
#define SDM220T_TOTAL_ACTIVE_ENERGY         0x0156                              //Wh
#define SDM220T_TOTAL_REACTIVE_ENERGY       0x0158                              //VARh
//SDM 630 registers
#define SDM630_VOLTAGE1                     0x0000                              //V
#define SDM630_VOLTAGE2                     0x0002                              //V
#define SDM630_VOLTAGE3                     0x0004                              //V
#define SDM630_CURRENT1                     0x0006                              //A
#define SDM630_CURRENT2                     0x0008                              //A
#define SDM630_CURRENT3                     0x000A                              //A
#define SDM630_CURRENT_TOTAL                0x0030                              //A
#define SDM630_POWER1                       0x000C                              //W
#define SDM630_POWER2                       0x000E                              //W
#define SDM630_POWER3                       0x0010                              //W
#define SDM630_POWER_TOTAL                  0x0034                              //W
#define SDM630_VOLT_AMPS1                   0x0012                              //VA
#define SDM630_VOLT_AMPS2                   0x0014                              //VA
#define SDM630_VOLT_AMPS3                   0x0016                              //VA
#define SDM630_VOLT_AMPS_TOTAL              0x0038                              //VA
#define SDM630_VOLT_AMPS_REACTIVE1          0x0018                              //VAr
#define SDM630_VOLT_AMPS_REACTIVE2          0x001A                              //VAr
#define SDM630_VOLT_AMPS_REACTIVE3          0x001C                              //VAr
#define SDM630_VOLT_AMPS_REACTIVE_TOTAL     0x003C                              //VAr
#define SDM630_POWER_FACTOR1                0x001E
#define SDM630_POWER_FACTOR2                0x0020
#define SDM630_POWER_FACTOR3                0x0022
#define SDM630_POWER_FACTOR_TOTAL           0x003E
#define SDM630_PHASE_ANGLE1                 0x0024                              //Degrees
#define SDM630_PHASE_ANGLE2                 0x0026                              //Degrees
#define SDM630_PHASE_ANGLE3                 0x0028                              //Degrees
#define SDM630_PHASE_ANGLE_TOTAL            0x0042                              //Degrees
#define SDM630_VOLTAGE_AVERAGE              0x002A                              //V
#define SDM630_CURRENT_AVERAGE              0x002E                              //A
#define SDM630_FREQUENCY                    0x0046                              //HZ
#define SDM630_IMPORT_ACTIVE_ENERGY         0x0048                              //Wh
#define SDM630_EXPORT_ACTIVE_ENERGY         0x004A                              //Wh
#define SDM630_IMPORT_REACTIVE_ENERGY       0x004C                              //VARh
#define SDM630_EXPORT_REACTIVE_ENERGY       0x004E                              //VARh
#define SDM630_TOTAL_SYSTEM_POWER_DEMAND    0x0054                              //W
#define SDM630_MAXIMUM_TOTAL_SYSTEM_POWER   0x0056                              //W

#define MAX_MODBUS_DIAP 20
typedef struct {
  byte Command = 0; 
  word StartDiap = 0; 
  word LengthDiap = 0;
  uint8_t* Address = NULL;
} ModbusDiap; 

class Eastron {
  private:
    ModbusDiap modbusArray[MAX_MODBUS_DIAP];
    ModbusMaster modbusNode;
  public:
    uint8_t* getValueAddress(byte Command, word ModbusAddress);
  
    bool Connected = false;
  
    Eastron();
    int AddModbusDiap(byte Command, word StartDiap, word LengthDiap);
    int getModbusDiapLength();
    void getStrModbusConfig(String &str);
    void ModbusSetup();
    void Poll(byte Command);

    uint16_t getWordValue(byte Command, word ModbusAddress);
    void setWordValue(uint16_t value, byte Command, word ModbusAddress);
    uint32_t getIntValue(byte Command, word ModbusAddress);
    uint64_t getInt64Value(byte Command, word ModbusAddress);
    float getFloatValue(byte Command, word ModbusAddress);
    void getMemoryHex(String &str, byte Command, word ModbusAddress, int len);
    void getValue(String &str, byte Command, word ModbusAddress, byte valueType);
};

#endif // ifndef __EASTRON_H__


