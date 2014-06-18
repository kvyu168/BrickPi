/*
 * Dexter Industries 
 *
 * Firmware originally written by Matthew Richardson
 *  matthewrichardson37<at>gmail.com
 *  http://mattallen37.wordpress.com/
 *  
 *  Jaikrishna T S
 *  t.s.jaikrishna<at>gmail.com
 * 
 *  Initial date: June 1, 2013
 *  Last updated: June 18, 2014
 *
 *  You may use this code as you wish, provided you give credit where it's due.
 *
 *  This program is specifically to be used with the BrickPi.
 *
 *  This is the BrickPi FW.
 *
 *  NOTE: The BrickPiM Library conflicts with the SoftwareSerial Library that is included in the BrickPiEV3 Library. 
 *  Hence it is required to modify the default SoftwareSerial library that comes with Arduino.
 *  To do this, open Arduino/libraries/SoftwareSerial/SoftwareSerial.h and add this line - "#undef PCINT2_vect"
 *
 */

/*

  Tasks that would require constant updating:
    RCX rotation sensor
    Motor speed/position regulation
  
  Could potentially use a timout of 2ms with UART_Read, and do the above in the main loop

*/

/*
  message type 1 byte
    if message type == MSG_TYPE_SENSOR_TYPE  
      sensor 1 type 1 byte
      sensor 2 type 1 byte
        for each sensor port
          if sensor type is I2C
            speed 8 bits
            devices 3 bits
              for devices
                address 7 bits
                settings 2 bits
                if settngs & BIT_I2C_SAME
                  out bytes 4 bits
                  in bytes 4 bits
                  for out bytes
                    out array 8 bits
      
      reply MSG_TYPE_SENSOR_TYPE 1 byte
    
    if message type == MSG_TYPE_VALUES
      for ports
        if offset encoder 1 bit
          offset length 5 bits
          offset (offset length + 1)
      
      for ports
        motor control 10 bits
      
      for sensor ports
        if sensor port type == TYPE_SENSOR_I2C
          for devices
            if not same mode
              out bytes 4 bits
              in bytes 4 bits
              for out bytes
                out array 8 bits
      
      reply
        MSG_TYPE_VALUES 1 byte
        
        for motor ports
          encoder length 5 bits
        
        for motor ports
          encoder value (encoder length)
        
        for sensor port
          switch sensor type
            case TYPE_SENSOR_TOUCH:
              sensor value 1 bit
            
            case TYPE_SENSOR_ULTRASONIC_CONT:
            case TYPE_SENSOR_ULTRASONIC_SS:
              sensor value 8 bits
            
            case TYPE_SENSOR_COLOR_FULL:
              sensor value 3 bits
              blank value 10 bits
              red value 10 bits
              green value 10 bits
              blue value 10 bits
            
            case TYPE_SENSOR_I2C:
              sensor value (success states) I2C_Devices bits
              for I2C_Devices
                if success
                  for in_byte
                    I2C_In_Array 8 bits
            
            case TYPE_SENSOR_LIGHT_OFF:
            case TYPE_SENSOR_LIGHT_ON:
            case TYPE_SENSOR_RCX_LIGHT:
            case TYPE_SENSOR_COLOR_RED:
            case TYPE_SENSOR_COLOR_GREEN:
            case TYPE_SENSOR_COLOR_BLUE:
            case TYPE_SENSOR_COLOR_NONE:
              sensor value 10 bits
*/

#include "EEPROM.h"              // Arduino EEPROM library
#include "BrickPiUART.h"         // BrickPi UART library
#include "BrickPiI2C.h"          // BrickPi I2C library
#include "BrickPiA.h"            // BrickPi Analog sensor library
#include "BrickPiUS.h"           // BrickPi Ultrasonic sensor library
#include "BrickPiCS.h"           // BrickPi Color sensor library
#include "BrickPiM.h"            // BrickPi Motor library
#include "SoftwareSerial.h"
#include "BrickPiEV3.h"

#define BYTE_MSG_TYPE               0 // MSG_TYPE is the first byte.
  #define MSG_TYPE_CHANGE_ADDR      1 // Change the UART address.
  #define MSG_TYPE_SENSOR_TYPE      2 // Change/set the sensor type.
  #define MSG_TYPE_VALUES           3 // Set the motor speed and direction, and return the sesnors and encoders.
  #define MSG_TYPE_E_STOP           4 // Float motors immediately.
  #define MSG_TYPE_TIMEOUT_SETTINGS 5 // Set the timeout.

// RPi to BrickPi
  
  // New UART address (MSG_TYPE_CHANGE_ADDR)
    #define BYTE_NEW_ADDRESS     1
  
  // Sensor setup (MSG_TYPE_SENSOR_TYPE)
    #define BYTE_SENSOR_1_TYPE   1
    #define BYTE_SENSOR_2_TYPE   2
  
  // Timeout setup (MSG_TYPE_TIMEOUT_SETTINGS)
    #define BYTE_TIMEOUT 1

//#define TYPE_SENSOR_RAW                0 // - 31
#define TYPE_SENSOR_LIGHT_OFF          0
#define TYPE_SENSOR_LIGHT_ON           (MASK_D0_M | MASK_D0_S)
#define TYPE_SENSOR_TOUCH              32
#define TYPE_SENSOR_ULTRASONIC_CONT    33
#define TYPE_SENSOR_ULTRASONIC_SS      34
#define TYPE_SENSOR_RCX_LIGHT          35 // tested minimally
#define TYPE_SENSOR_COLOR_FULL         36
#define TYPE_SENSOR_COLOR_RED          37
#define TYPE_SENSOR_COLOR_GREEN        38
#define TYPE_SENSOR_COLOR_BLUE         39
#define TYPE_SENSOR_COLOR_NONE         40
#define TYPE_SENSOR_I2C                41
#define TYPE_SENSOR_I2C_9V             42

#define TYPE_SENSOR_EV3_US_M0          43
#define TYPE_SENSOR_EV3_US_M1          44
#define TYPE_SENSOR_EV3_US_M2          45
#define TYPE_SENSOR_EV3_US_M3          46
#define TYPE_SENSOR_EV3_US_M4          47
#define TYPE_SENSOR_EV3_US_M5          48
#define TYPE_SENSOR_EV3_US_M6          49

#define TYPE_SENSOR_EV3_COLOR_M0       50
#define TYPE_SENSOR_EV3_COLOR_M1       51
#define TYPE_SENSOR_EV3_COLOR_M2       52
#define TYPE_SENSOR_EV3_COLOR_M3       53
#define TYPE_SENSOR_EV3_COLOR_M4       54
#define TYPE_SENSOR_EV3_COLOR_M5       55

#define TYPE_SENSOR_EV3_GYRO_M0        56
#define TYPE_SENSOR_EV3_GYRO_M1        57
#define TYPE_SENSOR_EV3_GYRO_M2        58
#define TYPE_SENSOR_EV3_GYRO_M3        59
#define TYPE_SENSOR_EV3_GYRO_M4        60

#define TYPE_SENSOR_EV3_INFRARED_M0    61
#define TYPE_SENSOR_EV3_INFRARED_M1    62
#define TYPE_SENSOR_EV3_INFRARED_M2    63
#define TYPE_SENSOR_EV3_INFRARED_M3    64
#define TYPE_SENSOR_EV3_INFRARED_M4    65
#define TYPE_SENSOR_EV3_INFRARED_M5    66

#define TYPE_SENSOR_EV3_TOUCH_0	       67


#define BIT_I2C_MID  0x01  // defined for each device
#define BIT_I2C_SAME 0x02  // defined for each device

unsigned long COMM_TIMEOUT = 250; // How many ms since the last communication, before timing out (and floating the motors).

// Run this once on setup.
void setup(){
  UART_Setup(500000);	// Start UART
  M_Setup();			// Motor Setup
  A_Setup();			// Analog Setup
}

int8_t Result;
byte Bytes;
byte Array[128];

byte SensorType[2];        // Sensor type (raw ADC, touch, light off, light flash, light on, ultrasonic normal, ultrasonic ping, ultrasonic ping full)
byte SensorSettings[2][8]; // For specifying the I2C details

long ENC[2];         // For storing the encoder values
long SEN[2];         // For storing sensor values
long ENC_Offset[2];

uint8_t I2C_Speed    [2];         // How fast to run the I2C bus.
uint8_t I2C_Devices  [2];         // How many I2C devices are on each bus.
uint8_t I2C_Addr     [2][8];      // The I2C address for each port. If non-same I2C mode, this could change constantly.
uint8_t I2C_Out_Bytes[2][8];      // How many bytes to write.
uint8_t I2C_In_Bytes [2][8];      // How many bytes to read.
uint8_t I2C_Out_Array[2][8][16];  // Data to write to I2C sensor 1 and 2.
uint8_t I2C_In_Array [2][8][16];  // Data read from I2C sensor 1 and 2.

byte debug_array[5] = {'d','e','b','u','g'};

unsigned long LastUpdate;


void debug_funtions(){
  
  /*  DEBUG WORK - Delete when done. */

			/*UART_Write_Debug(5, debug_array);
			byte value [4] = {0,0,0,0};
			value[3] = (byte) (SEN[0] & (0xFF));
			value[2] = (byte) ((SEN[0] >> 8) & 0xFF);
			value[1] = (byte) ((SEN[0] >> 16) & 0xFF);
			value[0] = (byte) ((SEN[0] >> 24) & 0xFF);
			// Print sensor values back
			UART_Write_Debug(4, value);
			value[3] = (byte) (SEN[1] & (0xFF));
			value[2] = (byte) ((SEN[1] >> 8) & 0xFF);
			value[1] = (byte) ((SEN[1] >> 16) & 0xFF);
			value[0] = (byte) ((SEN[1] >> 24) & 0xFF);
			// Print sensor values back
			UART_Write_Debug(4, value);			  
			// Print the word debug.
			UART_Write_Debug(5, debug_array);
			*/
			  //SensorType[0] = TYPE_SENSOR_EV3_TOUCH_0;
			  //SensorType[1] = TYPE_SENSOR_EV3_TOUCH_0;
			  Array[BYTE_SENSOR_1_TYPE] = TYPE_SENSOR_EV3_TOUCH_0;	// Set the value of Port_1 (or Port 3?) 
			  Array[BYTE_SENSOR_2_TYPE] = TYPE_SENSOR_EV3_TOUCH_0;	// Set the value of Port_2 (or Port 4?)
			  
			// Alternate back and forth between setup and reading values.  
			  if(Array[BYTE_MSG_TYPE] == MSG_TYPE_SENSOR_TYPE){
					Array[BYTE_MSG_TYPE] = MSG_TYPE_VALUES;
			  } else {
					Array[BYTE_MSG_TYPE] = MSG_TYPE_SENSOR_TYPE;
			  }
			  
			  Result = 1;
			// UART_Write_Debug(1, &Array[BYTE_MSG_TYPE]);
			  
  /*  DEBUG WORK - Delete when done. */

}

// Main loop runs over and over again.  

void loop(){   
  Result = UART_ReadArray(Bytes, Array, 1);		// Get an update from the Raspberry Pi.
  
/*  if(Result != (-2)){
    Serial.write(Result);
  }*/
	/*  DEBUG WORK - Delete when done. */
	// debug_funtions();
	/*  DEBUG WORK - Delete when done. */
	
  if(Result == 0){			//0  Destination address was BROADCAST
    LastUpdate = millis();
	// Emergency Stop code
	if(Array[BYTE_MSG_TYPE] == MSG_TYPE_E_STOP){	// Float motors immediately.
      M_Float();									// Float motors immediately.
    }
	// Reset firmware address (address stored in EEPROM) with a touch sensor.
    else if(Array[BYTE_MSG_TYPE] == MSG_TYPE_CHANGE_ADDR && Bytes == 2){
      A_Config(PORT_1, 0);                            // Setup PORT_1 for touch sensor
      if(A_ReadRaw(PORT_1) < 250){                    // Change address if touch sensor on port 1 is pressed.
        if(Array[BYTE_NEW_ADDRESS] != 0 && Array[BYTE_NEW_ADDRESS] != 255){
          UART_Set_Addr(Array[BYTE_NEW_ADDRESS]);     // Set new address
          Array[0] = MSG_TYPE_CHANGE_ADDR;
          UART_WriteArray(1, Array);
        }
      }
      SetupSensors();                                 // Change PORT_1 settings back
    }
  }
  else if(Result == 1){	 	//1  Destination address was mine

    LastUpdate = millis();	// Update the timeout clock.
    // Stop all motors.
	if(Array[BYTE_MSG_TYPE] == MSG_TYPE_E_STOP){
      M_Float();
      Array[0] = MSG_TYPE_E_STOP;
      UART_WriteArray(1, Array);      
    }
	// Reset the chips address.
    else if(Array[BYTE_MSG_TYPE] == MSG_TYPE_CHANGE_ADDR && Bytes == 2){
      A_Config(PORT_1, 0);                            // Setup PORT_1 for touch sensor
      if(A_ReadRaw(PORT_1) < 250){                    // Change address if touch sensor on port 1 is pressed.    
        if(Array[BYTE_NEW_ADDRESS] != 0 && Array[BYTE_NEW_ADDRESS] != 255){
          UART_Set_Addr(Array[BYTE_NEW_ADDRESS]);     // Set new address
          Array[0] = MSG_TYPE_CHANGE_ADDR;
          UART_WriteArray(1, Array);
        }
      }
      SetupSensors();                                 // Implement sensor settings.  
    }
	// Change/set the sensor type.
	else if(Array[BYTE_MSG_TYPE] == MSG_TYPE_SENSOR_TYPE){
		// UART_Write_Debug(5, debug_array);
      ParseSensorSettings();
      SetupSensors();
      Array[0] = MSG_TYPE_SENSOR_TYPE;	// Send back what we received: a command to ste the sensor type.
      UART_WriteArray(1, Array);
    }
	// Set the motor speed and direction, and return the sensors and encoders.
    else if(Array[BYTE_MSG_TYPE] == MSG_TYPE_VALUES){
      ParseHandleValues();							// Parse the values sent.
      UpdateSensors();								// Update the sensors values.
      M_Encoders(ENC[PORT_A], ENC[PORT_B]);      	// Get encoder values.
      EncodeValues();								// 
      Array[0] = MSG_TYPE_VALUES;
      UART_WriteArray(Bytes, Array);
    }
	// Set the timeout
    else if(Array[BYTE_MSG_TYPE] == MSG_TYPE_TIMEOUT_SETTINGS){
      COMM_TIMEOUT = Array[BYTE_TIMEOUT] + (Array[(BYTE_TIMEOUT + 1)] * 256) + (Array[(BYTE_TIMEOUT + 2)] * 65536) + (Array[(BYTE_TIMEOUT + 3)] * 16777216);
      Array[0] = MSG_TYPE_TIMEOUT_SETTINGS;
      UART_WriteArray(1, Array);
    }
  }
  
  // In case of a timeout, shut motors down.
  if(COMM_TIMEOUT && (millis() > (LastUpdate + COMM_TIMEOUT))){   // If it timed out, float the motors
    M_Float();	// Shutdown motors.
  }
  
  // Keep the NXT Color Sensor Alive
  byte i = 0;
  while(i < 2){
    if(SensorType[i] == TYPE_SENSOR_COLOR_FULL){
      CS_KeepAlive(i);                           // Simulate reading the color sensor, so that it doesn't timeout.
    }
    i++;
  }
}

unsigned int Bit_Offset = 0;

void AddBits(unsigned char byte_offset, unsigned char bit_offset, unsigned char bits, unsigned long value){
  unsigned char i = 0;
  while(i < bits){
    if(value & 0x01){
      Array[(byte_offset + ((bit_offset + Bit_Offset + i) / 8))] |= (0x01 << ((bit_offset + Bit_Offset + i) % 8));
    }
    value /= 2;
    i++;
  }
  Bit_Offset += bits;
}

unsigned long GetBits(unsigned char byte_offset, unsigned char bit_offset, unsigned char bits){
  unsigned long Result = 0;
  char i = bits;
  while(i){
    Result *= 2;
    Result |= ((Array[(byte_offset + ((bit_offset + Bit_Offset + (i - 1)) / 8))] >> ((bit_offset + Bit_Offset + (i - 1)) % 8)) & 0x01);    
    i--;
  }
  Bit_Offset += bits;
  return Result;
}

unsigned char BitsNeeded(unsigned long value){
  unsigned char i = 0;
  while(i < 32){
    if(!value)
      return i;
    value /= 2;
    i++;
  }
  return 31;
}

void ParseSensorSettings(){
  SensorType[PORT_1] = Array[BYTE_SENSOR_1_TYPE];	// Set the value of Port_1 (or Port 3?) 
  SensorType[PORT_2] = Array[BYTE_SENSOR_2_TYPE];	// Set the value of Port_2 (or Port 4?)
  Bit_Offset = 0;
  // Setup custom I2C sensors.
  for(byte port = 0; port < 2; port++){
    if(SensorType[port] == TYPE_SENSOR_I2C
    || SensorType[port] == TYPE_SENSOR_I2C_9V){
      I2C_Speed[port] = GetBits(3, 0, 8);
      I2C_Devices[port] = (GetBits(3, 0, 3) + 1);
      for(byte device = 0; device < I2C_Devices[port]; device++){
        I2C_Addr[port][device] = (GetBits(3, 0, 7) << 1);
        SensorSettings[port][device] = GetBits(3, 0, 2);        
        if(SensorSettings[port][device] & BIT_I2C_SAME){          
          I2C_Out_Bytes[port][device] = GetBits(3, 0, 4);
          I2C_In_Bytes[port][device] = GetBits(3, 0, 4);
          for(byte out_byte = 0; out_byte < I2C_Out_Bytes[port][device]; out_byte++){
            I2C_Out_Array[port][device][out_byte] = GetBits(3, 0, 8);
          }
        }
      }         
    }
  }
}

void EncodeValues(){
  for(byte Byte = 0; Byte < 128; Byte++){
    Array[Byte] = 0;
  }
  
  long Temp_Values[2];
  unsigned char Temp_ENC_DIR[2] = {0, 0};
  unsigned char Temp_BitsNeeded[2] = {0, 0};
  Bit_Offset = 0;
  
  for(byte port = 0; port < 2; port++){
    Temp_Values[port] = ENC[port];  
    if(Temp_Values[port] < 0){
      Temp_ENC_DIR[port] = 1;
      Temp_Values[port] *= (-1);
    }
    Temp_BitsNeeded[port] = BitsNeeded(Temp_Values[port]);
    if(Temp_BitsNeeded[port])
      Temp_BitsNeeded[port]++;
    AddBits(1, 0, 5, Temp_BitsNeeded[port]);
  }
  
  for(byte port = 0; port < 2; port++){
    Temp_Values[port] *= 2;
    Temp_Values[port] |= Temp_ENC_DIR[port];     
    AddBits(1, 0, Temp_BitsNeeded[port], Temp_Values[port]);
  }

  for(byte port = 0; port < 2; port++){
    switch(SensorType[port]){
      case TYPE_SENSOR_TOUCH:
        AddBits(1, 0, 1, SEN[port]);
      break;
      case TYPE_SENSOR_ULTRASONIC_CONT:
      case TYPE_SENSOR_ULTRASONIC_SS:
        AddBits(1, 0, 8, SEN[port]);
      break;
      case TYPE_SENSOR_COLOR_FULL:
        AddBits(1, 0, 3, SEN[port]);
        AddBits(1, 0, 10, CS_Values[port][BLANK_INDEX]);
        AddBits(1, 0, 10, CS_Values[port][RED_INDEX  ]);
        AddBits(1, 0, 10, CS_Values[port][GREEN_INDEX]);
        AddBits(1, 0, 10, CS_Values[port][BLUE_INDEX ]);
      break;
      case TYPE_SENSOR_I2C:
      case TYPE_SENSOR_I2C_9V:
        AddBits(1, 0, I2C_Devices[port], SEN[port]);
        for(byte device = 0; device < I2C_Devices[port]; device++){
          if((SEN[port] >> device) & 0x01){
            for(byte in_byte = 0; in_byte < I2C_In_Bytes[port][device]; in_byte++){
              AddBits(1, 0, 8, I2C_In_Array[port][device][in_byte]);
            }
          }
        }        
      break;
      case TYPE_SENSOR_EV3_US_M0       :
      case TYPE_SENSOR_EV3_US_M1       :
      case TYPE_SENSOR_EV3_US_M2       :
      case TYPE_SENSOR_EV3_US_M3       :
      case TYPE_SENSOR_EV3_US_M4       :
      case TYPE_SENSOR_EV3_US_M5       :
      case TYPE_SENSOR_EV3_US_M6       :
      case TYPE_SENSOR_EV3_COLOR_M0    :
      case TYPE_SENSOR_EV3_COLOR_M1    :
      case TYPE_SENSOR_EV3_COLOR_M2    :
      case TYPE_SENSOR_EV3_COLOR_M4    :
      case TYPE_SENSOR_EV3_COLOR_M5    :
      case TYPE_SENSOR_EV3_GYRO_M0     :
      case TYPE_SENSOR_EV3_GYRO_M1     :
      case TYPE_SENSOR_EV3_GYRO_M2     :
      case TYPE_SENSOR_EV3_GYRO_M4     :
      case TYPE_SENSOR_EV3_INFRARED_M0 :
      case TYPE_SENSOR_EV3_INFRARED_M1 :
      case TYPE_SENSOR_EV3_INFRARED_M3 :
      case TYPE_SENSOR_EV3_INFRARED_M4 :
      case TYPE_SENSOR_EV3_INFRARED_M5 :
        AddBits(1, 0, 16, SEN[port]);
      break; 
	  case TYPE_SENSOR_EV3_TOUCH_0 	   :		// Send 16 bits or two bytes with the touch result.
		AddBits(1, 0, 16, SEN[port]);	
		break;
      case TYPE_SENSOR_EV3_COLOR_M3    :
      case TYPE_SENSOR_EV3_GYRO_M3     :
      case TYPE_SENSOR_EV3_INFRARED_M2 :
        AddBits(1, 0, 32, SEN[port]);
      break;
      case TYPE_SENSOR_RCX_LIGHT:
      case TYPE_SENSOR_COLOR_RED:
      case TYPE_SENSOR_COLOR_GREEN:
      case TYPE_SENSOR_COLOR_BLUE:
      case TYPE_SENSOR_COLOR_NONE:
      default:
        AddBits(1, 0, 10, SEN[port]);
    }
  }
  
  Bytes = (1 + ((Bit_Offset + 7) / 8));      // How many bytes to send
}

void ParseHandleValues(){
  Bit_Offset = 0;
  
  for(byte port = 0; port < 2; port++){
    if(GetBits(1, 0, 1)){
      ENC_Offset[port] = GetBits(1, 0, (GetBits(1, 0, 5) + 1));
      if(ENC_Offset[port] & 0x01){
        ENC_Offset[port] *= (-1);
      }
      ENC_Offset[port] /= 2;
    }
    else{
      ENC_Offset[port] = 0;
    }
  }
  
  if(ENC_Offset[PORT_A] || ENC_Offset[PORT_B]){
    M_EncodersSubtract(ENC_Offset[PORT_A], ENC_Offset[PORT_B]);
  }
  
  for(byte port = 0; port < 2; port++){
    M_PWM(port, GetBits(1, 0, 10));              // 8 bits of PWM, 1 bit dir, 1 bit enable
  }
  
  for(byte port = 0; port < 2; port++){
    if(SensorType[port] == TYPE_SENSOR_I2C
    || SensorType[port] == TYPE_SENSOR_I2C_9V){  
      for(byte device = 0; device < I2C_Devices[port]; device ++){
        if(!(SensorSettings[port][device] & BIT_I2C_SAME)){           // not same
          I2C_Out_Bytes[port][device]       = GetBits(1, 0, 4);
          I2C_In_Bytes [port][device]       = GetBits(1, 0, 4);
          for(byte ii = 0; ii < I2C_Out_Bytes[port][device]; ii++){
            I2C_Out_Array[port][device][ii] = GetBits(1, 0, 8);
          }
        }
      }
    }
  }
}

void SetupSensors(){
  EV3_Reset();
  //
  // byte test_case = (byte)SensorType[0];		// For debug purposes only
  // UART_Write_Debug(1, &test_case);
  // Setup the sensors for a certain configuration.  Goes through each of the sensor ports,
  // and configures them based on the array values "SensorType[port]"
  for(byte port = 0; port < 2; port++){  
    switch(SensorType[port]){
      case TYPE_SENSOR_TOUCH:
        A_Config(port, 0);
      break;
      case TYPE_SENSOR_ULTRASONIC_CONT:
        US_Setup(port);
      break;
      case TYPE_SENSOR_ULTRASONIC_SS:
                                                             // FIXME add support for SS mode
      break;
      case TYPE_SENSOR_RCX_LIGHT:
        A_Config(port, MASK_9V);
      break;
      case TYPE_SENSOR_COLOR_FULL:
        CS_Begin(port, TYPE_COLORFULL);
      break;
      case TYPE_SENSOR_COLOR_RED:
        CS_Begin(port, TYPE_COLORRED);
      break;
      case TYPE_SENSOR_COLOR_GREEN:
        CS_Begin(port, TYPE_COLORGREEN);
      break;
      case TYPE_SENSOR_COLOR_BLUE:
        CS_Begin(port, TYPE_COLORBLUE);
      break;
      case TYPE_SENSOR_COLOR_NONE:
        CS_Begin(port, TYPE_COLORNONE);
      break;
      case TYPE_SENSOR_I2C:
        A_Config(port, 0);
        I2C_Setup(port, I2C_Addr[port][0], I2C_Speed[port]);
      break;
      case TYPE_SENSOR_I2C_9V:
        A_Config(port, MASK_9V);
        I2C_Setup(port, I2C_Addr[port][0], I2C_Speed[port]);
      break;
      case TYPE_SENSOR_EV3_US_M0       :
      case TYPE_SENSOR_EV3_US_M1       :
      case TYPE_SENSOR_EV3_US_M2       :
      case TYPE_SENSOR_EV3_US_M3       :
      case TYPE_SENSOR_EV3_US_M4       :
      case TYPE_SENSOR_EV3_US_M5       :
      case TYPE_SENSOR_EV3_US_M6       :
      case TYPE_SENSOR_EV3_COLOR_M0    :
      case TYPE_SENSOR_EV3_COLOR_M1    :
      case TYPE_SENSOR_EV3_COLOR_M2    :
      case TYPE_SENSOR_EV3_COLOR_M3    :
      case TYPE_SENSOR_EV3_COLOR_M4    :
      case TYPE_SENSOR_EV3_COLOR_M5    :
      case TYPE_SENSOR_EV3_GYRO_M0     :
      case TYPE_SENSOR_EV3_GYRO_M1     :
      case TYPE_SENSOR_EV3_GYRO_M2     :
      case TYPE_SENSOR_EV3_GYRO_M3     :
      case TYPE_SENSOR_EV3_GYRO_M4     :
      case TYPE_SENSOR_EV3_INFRARED_M0 :
      case TYPE_SENSOR_EV3_INFRARED_M1 :
      case TYPE_SENSOR_EV3_INFRARED_M2 :
      case TYPE_SENSOR_EV3_INFRARED_M3 :
      case TYPE_SENSOR_EV3_INFRARED_M4 :
      case TYPE_SENSOR_EV3_INFRARED_M5 :
        EV3_Setup(port, SensorType[port]);			// For any EV3 this is called.
      break;     
      case TYPE_SENSOR_EV3_TOUCH_0 :
        EV3_Setup_Touch(port);			// For any EV3 this is called.
	    // UART_Write_Debug(5, debug_array);		
      break;
      default:										// Default is analog value.
        A_Config(port, SensorType[port]);			// Almost everyone that's not defined above is setup this line.
    }
  }  
}

void UpdateSensors(){
	byte test_case = 0;		// For debug purposes only
  // UART_Write_Debug(1, &test_case);

  for(byte port = 0; port < 2; port++){
    switch(SensorType[port]){
      case TYPE_SENSOR_TOUCH:
        if(A_ReadRaw(port) < 400) SEN[port] = 1;
        else                      SEN[port] = 0;
      break;
      case TYPE_SENSOR_ULTRASONIC_CONT:
        SEN[port] = US_ReadByte(port);
      break;
      case TYPE_SENSOR_ULTRASONIC_SS:
        SEN[port] = 37;                 // FIXME add support for SS mode
      break;
      case TYPE_SENSOR_RCX_LIGHT:
        A_Config(port, 0);
        delayMicroseconds(20);
        SEN[port] = A_ReadRaw(port);
        A_Config(port, MASK_9V);
      break;
      case TYPE_SENSOR_COLOR_FULL:
      case TYPE_SENSOR_COLOR_RED:
      case TYPE_SENSOR_COLOR_GREEN:
      case TYPE_SENSOR_COLOR_BLUE:
      case TYPE_SENSOR_COLOR_NONE:
        SEN[port] = CS_Update(port);      // If the mode is FULL, the 4 raw values will be stored in CS_Values
		break;
      case TYPE_SENSOR_I2C:
      case TYPE_SENSOR_I2C_9V:
        SEN[port] = 0;
        for(byte device = 0; device < I2C_Devices[port]; device++){
          SEN[port] |= ((I2C_Transfer(port, I2C_Addr[port][device], I2C_Speed[port], (SensorSettings[port][device] & BIT_I2C_MID), I2C_Out_Bytes[port][device], I2C_Out_Array[port][device], I2C_In_Bytes[port][device], I2C_In_Array[port][device]) & 0x01) << device); // The success/failure result of the I2C transaction(s) is stored as 1 bit in SEN.
        }
        break;
      case TYPE_SENSOR_EV3_US_M0       :
      case TYPE_SENSOR_EV3_US_M1       :
      case TYPE_SENSOR_EV3_US_M2       :
      case TYPE_SENSOR_EV3_US_M3       :
      case TYPE_SENSOR_EV3_US_M4       :
      case TYPE_SENSOR_EV3_US_M5       :
      case TYPE_SENSOR_EV3_US_M6       :
      case TYPE_SENSOR_EV3_COLOR_M0    :
      case TYPE_SENSOR_EV3_COLOR_M1    :
      case TYPE_SENSOR_EV3_COLOR_M2    :
      case TYPE_SENSOR_EV3_COLOR_M3    :
      case TYPE_SENSOR_EV3_COLOR_M4    :
      case TYPE_SENSOR_EV3_COLOR_M5    :
      case TYPE_SENSOR_EV3_GYRO_M0     :
      case TYPE_SENSOR_EV3_GYRO_M1     :
      case TYPE_SENSOR_EV3_GYRO_M2     :
      case TYPE_SENSOR_EV3_GYRO_M3     :
      case TYPE_SENSOR_EV3_GYRO_M4     :
      case TYPE_SENSOR_EV3_INFRARED_M0 :
      case TYPE_SENSOR_EV3_INFRARED_M1 :
      case TYPE_SENSOR_EV3_INFRARED_M2 :
      case TYPE_SENSOR_EV3_INFRARED_M3 :
      case TYPE_SENSOR_EV3_INFRARED_M4 :
      case TYPE_SENSOR_EV3_INFRARED_M5 :
        SEN[port] = EV3_Update(port);		
        break;
      case TYPE_SENSOR_EV3_TOUCH_0 :
        SEN[port] = EV3_Update_Touch(port);	
	    // UART_Write_Debug(5, debug_array);		
        break;
		
      default:
        SEN[port] = A_ReadRaw(port);		
    }
  }
}
