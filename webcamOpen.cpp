/**
 * Edited using colorscheme "elflord"
 */
#include <cv.h> 
#include <bcm2835.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <stdio.h>
#include <mpi.h>

using namespace std;
using namespace cv;

#define MPI_CHECK(call) \
   if((call) != MPI_SUCCESS) {      cerr << "MPI error calling \""#call"\"\n";      exit(-1); }




//Initialize Servo position to 90 degrees
unsigned char panServoPos = 90;
unsigned char tiltServoPos = 70;
//Servo control constants
const unsigned char TILT_ID = 230;
const unsigned char PAN_ID  = 220;
const unsigned char INIT_SERVO = 210;
const unsigned char ACK_SERVO = 200;

//Servo max and min values
const unsigned char MIN_SERVO = 20;
const unsigned char MAX_SERVO = 130;

//Image parameters
const int WIDTH_IMG = 320;
const int HEIGHT_IMG = 240;

const int MAX_SIZE_TO_DETECT = 60;
const int MIN_SIZE_TO_DETECT = 20;

//Delta constants
const unsigned char XDELTA = 1;
const unsigned char YDELTA = 1;

//Image bounding box values [using 5% currently]
const int WIDTH_BOUND = WIDTH_IMG * 0.2;
const int HEIGHT_BOUND = HEIGHT_IMG * 0.1;

int setupUART();
void updateServoPositionOrigin(Rect rect, int width, int height); 
void writeServo(int uartFD, unsigned char servoID, unsigned char val);
/** Function Headers */
std::vector<Rect> detect( Mat frame_gray, int min, int max);
void display(Mat frame_gray, Rect * faces, int numFaces);


String face_cascade_name = "/usr/share/opencv/lbpcascades/lbpcascade_frontalface.xml";
CascadeClassifier face_cascade;
string window_name = "Capture - Face detection";
RNG rng(12345);

int uartFD;
/** @function main */
int main(int argc, char** argv )
{
   MPI_Init(&argc, &argv);
   int rank, size;
   MPI_Comm_size(MPI_COMM_WORLD, &size);
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   printf("size: %d, rank: %d\n", size, rank);


   int range = MAX_SIZE_TO_DETECT - MIN_SIZE_TO_DETECT;
   int rangePerNode = size / range;
   int minRange = MIN_SIZE_TO_DETECT + rangePerNode * rank; 
   int maxRange = rank != size - 1 ? MIN_SIZE_TO_DETECT + (rangePerNode * rank) : MAX_SIZE_TO_DETECT;



   char arr[76800];


   Mat frame;
   CvCapture* capture;
   unsigned char ack;   
   Rect * all_faces;

   int * sizesToSend;
   int * displs;
   if(rank==0) {
      all_faces = (Rect *) calloc(size, sizeof(Rect));
      sizesToSend = (int *) calloc(size , sizeof(int)); 
      displs = (int *) calloc(size , sizeof(int));
      cout<<"start\n";

      //Initialize the bcm2835 hardware library
      if(!bcm2835_init())
      {
         fprintf(stderr, "Unable to initialize bcm2835 library.\n");
         exit(-2);
      }

      //Open and initialize the hardware uart
      uartFD = setupUART();
      cout<<"started servos\n";
      //Send a short initialization delay to ensure proper starting
      bcm2835_delay(3000);

      //Send the init to the servos
      ack = INIT_SERVO;
      write(uartFD, &ack, sizeof(unsigned char));
      bcm2835_delay(1);

      //Initialize the servos to their center points
      writeServo(uartFD, TILT_ID, tiltServoPos);
      writeServo(uartFD, PAN_ID, panServoPos);
      cout<<"initialized servos\n";



      //-- 1. Load the cascades
      if( !face_cascade.load( face_cascade_name ) ){ printf("--(!)Error loading\n"); return -1; };

      //-- 2. Read the video stream
      capture = cvCaptureFromCAM( -1 );
      cout<<"starting caputre loop\n";

   }
   if( capture )
   {
      if(rank==0) {
         cvSetCaptureProperty(capture,CV_CAP_PROP_FRAME_WIDTH, WIDTH_IMG);
         cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, HEIGHT_IMG);
      }

      while( true )
      {
         try {
            if(rank==0) {
               frame = cvQueryFrame( capture );
            }
            //-- 3. Apply the classifier to the frame
            if( rank !=0 || !frame.empty() )
            {
               Mat frame_gray;
               int bytesToRecieve;
               std::vector<Rect> faces;

               if(rank==0 && frame.rows == HEIGHT_IMG && frame.cols == WIDTH_IMG) { 
                  cvtColor( frame, frame_gray, CV_BGR2GRAY );
                  equalizeHist( frame_gray, frame_gray );
                  memcpy(arr, frame_gray.data, 76800); 
               } else if(rank == 0) {
                  cerr << "incorrect frame sizes. " << frame.rows << " : "<< frame.cols <<" \n";
               }

               MPI_Bcast(arr, 76800, MPI_CHAR, 0, MPI_COMM_WORLD);
               Mat mat = Mat(HEIGHT_IMG, WIDTH_IMG, CV_8UC1, arr);
               faces = detect( mat, minRange, maxRange);
               int numFaces = faces.size();
               int bytesToSend = (numFaces==0) ? 0 : sizeof(Rect);
               MPI_Gather(&bytesToSend, 1, MPI_INT, sizesToSend, size, MPI_INT, 0, MPI_COMM_WORLD); 
               int bytestoRecieve = 0;
               for(int i =0; i < size; i++) {
                  bytesToRecieve +=sizesToSend[i];
                  if(i+1 < size) {
                     displs[i+1] = displs[i] + sizesToSend[i];
                  }
               }
               MPI_Barrier(MPI_COMM_WORLD);
               MPI_CHECK(MPI_Gatherv(&(faces[0]), bytesToSend, MPI_CHAR, all_faces, sizesToSend, displs, MPI_CHAR, 0, MPI_COMM_WORLD));
               if(rank==0) {
                  try {
                     display(mat, all_faces, bytesToRecieve / sizeof(Rect));
                  } catch(int e) {
                     cerr<<"caught an err: " << e << "\n";
                  }
               }
            }
            else
            { 
               cerr<<" --(!) No captured frame -- Break!\n";
               break;
            }

            if(rank==0) {
               int c = waitKey(100);
               if( (char)c == 'c' ) { 
                  cerr<<"Detected 'c', Exiting!\n";
                  break;
               }
            }
         }catch (int e) {
            cerr << "caught an err: " << e << "\n";
         }
      }
   } else {
      cerr<<"Webcam not initialized!\n";
      exit(1);
   }
   MPI_Finalize();
   return 0;
}

/** @function detectAndDisplay */
std::vector<Rect> detect( Mat frame_gray, int min, int max )
{
   std::vector<Rect> faces;
   //-- Detect faces
   face_cascade.detectMultiScale( frame_gray, faces, 1.1, 2, CV_HAAR_SCALE_IMAGE, Size(min, max));
   return faces;
}

void display(Mat frame_gray, Rect * faces, int numFaces) {
   if(numFaces > 0){
      updateServoPositionOrigin(faces[0], WIDTH_IMG, HEIGHT_IMG);

      rectangle(frame_gray, faces[0], CV_RGB(255,255,255),1);
      imshow( window_name, frame_gray );                                                                                          
   }
}
void updateServoPositionOrigin(Rect rect, int width, int height) {
   int threshold = 20;

   int rectCenterX = rect.x + rect.width/2;
   int rectCenterY = rect.y + rect.height/2;
   int screenCenterX = width/2;
   int screenCenterY = height/2;

   int xOffset = rectCenterX - screenCenterX;
   int yOffset = rectCenterY - screenCenterY;

   if(xOffset > threshold) {
      panServoPos -= XDELTA;
   } else if (xOffset < -1 * threshold) {
      panServoPos += XDELTA;
   }

   if(yOffset > threshold) {
      tiltServoPos += YDELTA;
   } else if (yOffset < -1 * threshold) {
      tiltServoPos -= YDELTA;
   }

   //Check servo bounds and update accordingly
   if(panServoPos < MIN_SERVO)
   {
      panServoPos = MIN_SERVO;
   }
   else if(panServoPos > MAX_SERVO)
   {
      panServoPos = MAX_SERVO;
   }

   if(tiltServoPos < MIN_SERVO)
   {
      tiltServoPos = MIN_SERVO;
   }
   else if(tiltServoPos > MAX_SERVO)
   {
      tiltServoPos = MAX_SERVO;
   }

   //Update the servo positions
   writeServo(uartFD, TILT_ID, tiltServoPos);
   writeServo(uartFD, PAN_ID, panServoPos);
}


void writeServo(int uartFD, unsigned char servoID, unsigned char val)
{
   unsigned char txBuf[2];
   unsigned char ack;

   if(servoID == TILT_ID)
   {
      //printf("Tilt Servo");
   }
   else if(servoID == PAN_ID)
   {
      //printf("Pan Servo");
   }

   //printf(" Cur Val: %d\n", (int) val);

   //Send the servo id and the desired value
   txBuf[0] = servoID;
   txBuf[1] = val;

   //         //Write the data to the UART
   write(uartFD, txBuf, sizeof(unsigned char) * 2);
   bcm2835_delay(1);

   //Wait until you get a response
   do
   {
      read(uartFD, &ack, sizeof(unsigned char));
   } while(ack != ACK_SERVO);
}

//Opens a file descriptor to the hardware UART and initializes it
int setupUART()
{
   int fd;
   struct termios opt;

   //Code to turn off the scheduler so this task stays the top priority
   struct sched_param sp;
   memset(&sp, 0, sizeof(sp));
   sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
   sched_setscheduler(0, SCHED_FIFO, &sp);
   mlockall(MCL_CURRENT | MCL_FUTURE);

   //Open the uart serial device
   fd = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY); //| O_NDELAY);

   /*Problem opening the uart*/
   if(fd == -1)
   {
      perror("Failure opening uart");
      exit(-1);
   }

   /*Get the current options of the uart*/
   tcgetattr(fd, &opt);

   /*Sets the new options of the uart*/
   opt.c_cflag = B19200 | CS8 | CLOCAL | CREAD;
   opt.c_iflag = IGNPAR;
   opt.c_oflag = 0;
   opt.c_lflag = 0;
   tcflush(fd, TCIFLUSH);
   tcsetattr(fd, TCSANOW, &opt);

   return fd;
}

