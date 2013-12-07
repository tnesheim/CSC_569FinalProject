#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/mman.h>
#include <sched.h>
#include <string.h>
#include <bcm2835.h>

//SERVO_ID's
const unsigned char TILT_ID = 230;
const unsigned char PAN_ID  = 220;
const unsigned char INIT_SERVO = 210;
const unsigned char ACK_SERVO = 200;

//Write data to the servos and wait for a response
void writeServo(int uartFD, unsigned char servoID, unsigned char val);

int main(int argc, char *argv[])
{
   int uartFD;
   unsigned char ack;
   struct termios opt;
   int i;

   //Code to turn off the scheduler so this task stays the top priority
   struct sched_param sp;
   memset(&sp, 0, sizeof(sp));
   sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
   sched_setscheduler(0, SCHED_FIFO, &sp);
   mlockall(MCL_CURRENT | MCL_FUTURE);

   if(!bcm2835_init())
   {
      fprintf(stderr, "Problem initiating bcm2835.\n");
      exit(-3);
   }

   //Open the uart serial device
   uartFD = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY); //| O_NDELAY);

   /*Problem opening the uart*/
   if(uartFD == -1)
   {
      perror("Failure opening uart");
      exit(-1);
   }

   /*Get the current options of the uart*/
   tcgetattr(uartFD, &opt);
 
   /*Sets the new options of the uart*/
   opt.c_cflag = B19200 | CS8 | CLOCAL | CREAD;
   opt.c_iflag = IGNPAR;
   opt.c_oflag = 0;
   opt.c_lflag = 0;
   tcflush(uartFD, TCIFLUSH);
   tcsetattr(uartFD, TCSANOW, &opt);

   //Send the init to the servos
   ack = INIT_SERVO;
  
   bcm2835_delay(3000);

   //while(1)
   //{
      write(uartFD, &ack, sizeof(unsigned char));
      bcm2835_delay(1);
   //}

   while(1)
   {
      for(i = 20; i < 140; i++)
      {
         printf("Here first: %d\n", i);
         //Write the servo values
         writeServo(uartFD, TILT_ID, (unsigned char) i);
         writeServo(uartFD, PAN_ID, (unsigned char) i); 
      }

      for(i = 140; i > 20; i--)
      {
         printf("In here: %d\n", i);
         //Write the servo values
         writeServo(uartFD, TILT_ID, (unsigned char) i);
         writeServo(uartFD, PAN_ID, (unsigned char) i); 
      }
   }

   /*Close the uart*/
   close(uartFD);

   return 0;
}

//Write data to the servos and wait for a response
void writeServo(int uartFD, unsigned char servoID, unsigned char val)
{
   unsigned char txBuf[2];
   unsigned char ack;

   //Send the servo id and the desired value
   txBuf[0] = servoID;
w   txBuf[1] = val;

   //Write the data to the UART
   write(uartFD, txBuf, sizeof(unsigned char) * 2);
   bcm2835_delay(1);

   //Wait until you get a response
   do
   {
      read(uartFD, &ack, sizeof(unsigned char));
   } while(ack != ACK_SERVO);
}
