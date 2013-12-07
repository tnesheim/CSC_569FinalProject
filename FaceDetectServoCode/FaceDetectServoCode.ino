/*This program will take commands from the raspberryPi to move the servos accordingly.
 *Command structure is as follows: 2Bytes -> 1st Byte = SERVO_ID, 2nd Byte = SERVO_POSITION*/

#include <SoftwareSerial.h>
#include <Servo.h> 
 
Servo panServo;  // create servo object to control a servo 
                // a maximum of eight servo objects can be created 
Servo tiltServo;

//Create the SoftwareSerial
SoftwareSerial servoSerial(2,3);

//SERVO_ID's
const unsigned char TILT_ID = 230;
const unsigned char PAN_ID  = 220;
const unsigned char INIT_SERVO = 210;
const unsigned char ACK_SERVO = 200;

unsigned char tiltPos = 90;
unsigned char panPos  = 90;
 
unsigned char data;  
 
void setup() 
{ 
  //Begin Serial transmission
  servoSerial.begin(19200);
  Serial.begin(19200);
  
  panServo.attach(9);  // attaches the servo on pin 9 to the servo object 
  tiltServo.attach(8);
 
 /* while(1)
 {
    if(Serial.available())
    {
    data = Serial.read();
    Serial.write(data);
    }
 } */
    
  /*Wait for the init from the raspberryPi*/  
  while((data = Serial.read()) != INIT_SERVO)
     ;
} 
 
 
void loop() 
{ 
  //If there is data, read it 
  if(Serial.available() > 0)
  {
     //Read the data to see if it is an ID
     data = Serial.read();
        
     //Depending on the ID found, set each servo accordingly
     if(data == TILT_ID)
     {
       //Read the data, and write it to the servo
       data = Serial.read();
       
       if(data >= 0 && data <= 180)
       {
          tiltPos = data;
       } 
     }
     else if(data == PAN_ID)
     {
        //Read the data, and write it to the servo
       data = Serial.read();
       
       if(data >= 0 && data <= 180)
       {
          panPos = data;
       }
     }

     //Acknowledge the serial communication
     Serial.write(ACK_SERVO);
  } 
  
  tiltServo.write(tiltPos);
  panServo.write(panPos);
  //Wait for servos to catch up
  delay(15);
} 
