#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <termios.h>



void clearbuff();
//sup
void* serial_connection_thread(void* arg){
    int index;
    int bytes_read;
    int fd = open("/dev/cu.usbmodem1411", O_RDONLY);
    if (fd == -1){
        printf("error accessing usb\n");
        return 9;
    }
    struct termios options;       // struct to hold options
    tcgetattr(fd, &options);      // associate with this fd
    cfsetispeed(&options, 9600);  // set input baud rate
    cfsetospeed(&options, 9600);  // set output baud rate
    tcsetattr(fd, TCSANOW, &options);  // set options
    
    char buff[100];
    
    while(1){
        
        // clearbuff();
        bytes_read = read(fd, buff, 100);
        while(buff[bytes_read-1] != '\n'){
            bytes_read += read(fd, &buff[bytes_read], 100);
           
        }
        //msg will be a global variable
        char* msg = malloc(strlen(buff));
        strcpy(msg, buff);
        
    }
    
}
// void clearbuff(){
//   for(int i = 0; i< 100; i++){
//     buff[i] = '\0';
//   }
  
// }