#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// XDCtools Header files
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <xdc/std.h>

/* TI-RTOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Idle.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/GPIO.h>
#include <ti/net/http/httpcli.h>
#include <ti/drivers/I2C.h>

#include "Board.h"

#include <sys/socket.h>
#include <arpa/inet.h>

#define SOCKETTEST_IP     "192.168.1.16"
#define TIMEIP            "128.138.140.44"
#define TASKSTACKSIZE     4096
#define OUTGOING_PORT     5011
#define INCOMING_PORT     5030

extern Mailbox_Handle mailbox0;
extern Mailbox_Handle mailbox1;
extern Semaphore_Handle semaphore0;     // posted by httpTask and pended by clientTask
extern Semaphore_Handle semaphore1;
extern Semaphore_Handle semaphore2;
extern Semaphore_Handle semaphore3;
char   tempstr[20];                     // temperature string
int   timestr;
I2C_Handle      i2c;
I2C_Params      i2cParams;
I2C_Transaction i2cTransaction;
char takenTime[20];
char allSecondSince1900;

/*
 *  ======== printError ========
 */
uint8_t ctr;

Void Timer_ISR(UArg arg1)
{
    Semaphore_post(semaphore2);
    Semaphore_post(semaphore3);
}
Void SWI_ISR(UArg arg1)
{


    while(1){
        Semaphore_pend(semaphore2, BIOS_WAIT_FOREVER);

        timestr  = takenTime[0]*16777216 +  takenTime[1]*65536 + takenTime[2]*256 + takenTime[3];
        timestr += 10800;
        timestr += ctr++;
        System_printf("TIME: %s", ctime(&timestr));
        System_flush();
        Mailbox_post(mailbox1, &timestr, BIOS_NO_WAIT);

    }
}

void printError(char *errString, int code)
{
    System_printf("Error! code = %d, desc = %s\n", code, errString);
    BIOS_exit(code);
}
bool IIC_OpenComm(void)
{
    bool retval = false;

    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;                 // minimum speed first
    i2c = I2C_open(Board_I2C0, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");   // error, retval is false
    }
    else {
        retval = true;
    }
    System_flush();
    return retval;                                  // return true or false
}

void IIC_CloseComm(void)
{
    I2C_close(i2c);

    System_flush();
}

bool IIC_writeReg(int device_ID, int addr, uint8_t val)
{
    uint8_t txBuffer[2];
    uint8_t rxBuffer[2];
    bool retval=false;

    // place parameters
    txBuffer[0] = addr;                             // register address
    txBuffer[1] = val;                              // value to be written
    i2cTransaction.slaveAddress = device_ID;        // device IIC ID
    i2cTransaction.writeBuf = txBuffer;             // buffer that holds the values to be written
    i2cTransaction.writeCount = 2;                  // 2 bytes will be sent
    i2cTransaction.readBuf = rxBuffer;              // receive buffer (in this case it is not used)
    i2cTransaction.readCount = 0;                   // no bytes to be received

    if (I2C_transfer(i2c, &i2cTransaction)) {       // start the transaction
        retval = true;                              // true will be returned to indicate that transaction is successful
    }
    else {
        System_printf("I2C Bus fault\n");           // there is an error, returns false
    }
    System_flush();

    return retval;
}

bool IIC_readReg(int device_ID, int addr, int no_of_bytes, char *buf)
{
    uint8_t txBuffer[2];
    bool retval=false;

    // addr: register number
    txBuffer[0] = addr;                             // 1 byte: register address
    i2cTransaction.slaveAddress = device_ID;        // Device Id
    i2cTransaction.writeBuf = txBuffer;             // buffer to be sent
    i2cTransaction.writeCount = 1;                  // send just register address
    i2cTransaction.readBuf = buf;                   // read into this buffer
    i2cTransaction.readCount = no_of_bytes;         // number of bytes to be read


    if (I2C_transfer(i2c, &i2cTransaction)) {
        //System_printf("IIC_readReg(%d,%d)\n", addr, buf[0]);
        retval=true;
    }
    else {
        System_printf("I2C Bus fault\n");
    }
    System_flush();

    return retval;
}


Void taskFxn(UArg arg0, UArg arg1)
{
    Semaphore_pend(semaphore1, BIOS_WAIT_FOREVER);
    char buf[10];
    int value;
    int cnt = 0;
    int pulseW = 0;
    int Pulsecount = 0;
    int next1=0;
    IIC_OpenComm();
    int HR;
    char mode;
    int pulStat = 0;
    int DC, DCOld,valuePast = 0;
    int BWOld,BWNew = 0;

    //Bus connection
    IIC_readReg(0x57, 0xFF, 1, buf);



    //Mode select
    IIC_readReg(0x57, 0x06, 1, buf);
    mode = (buf[0] & 0xF8) | 0x02;
    IIC_writeReg(0x57, 0x06, mode);

    //Sampling Rate select
    IIC_readReg(0x57, 0x07, 1, buf);
    mode = (buf[0] & 0xE3) | (0x00<<2);
    IIC_writeReg(0x57,0x07,mode);

    //LED Pulse Width select
    IIC_readReg(0x57, 0x07, 1, buf);
    mode = (buf[0] & 0xFC) | 0x03;
    IIC_writeReg(0x57,0x07,mode);

    //LED Current select

    mode = ( 0x08 << 4) | (0x0F) ;
    IIC_writeReg(0x57,0x09,mode);

    while(1) {
        IIC_readReg(0x57, 0x05, 4, buf);
        next1 = (buf[0] << 8) | buf[1];

        //DC FILTER//
        DC = next1 + (0.75 * DCOld);
        next1 = DC - DCOld;
        DCOld = DC;
        value = next1;



        //BUTTERWORTH FILTER//
        BWNew = (2.452372752527856026e-1 * value) + (0.50952544949442879485 *BWOld);
        BWOld = BWNew;
        value = BWNew;

        Task_sleep(50);
        if(value > valuePast & pulStat == 0){
            pulStat = 1;

        }
        if(value <= valuePast-20 & pulStat == 1){

            pulseW =Pulsecount;
            pulStat = 0;
            Pulsecount = 0;

        }
        if (cnt==20){
            System_printf("HR is  %d\n", HR);
            System_flush();
            cnt=0;
        }
        cnt++;
        Pulsecount++;
        valuePast = value;
        HR = 1200 / pulseW;


        Mailbox_post(mailbox0, &HR, BIOS_NO_WAIT);
    }

    IIC_CloseComm();
}

bool sendData2Server(char *serverIP, int serverPort, char *data, int size)
{
    int sockfd, connStat, numSend;
    bool retval=false;
    struct sockaddr_in serverAddr;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1) {
        System_printf("Socket not created");
        close(sockfd);
        return false;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));  // clear serverAddr structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);     // convert port # to network order
    inet_pton(AF_INET, serverIP, &(serverAddr.sin_addr));

    connStat = connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if(connStat < 0) {
        System_printf("sendData2Server::Error while connecting to server\n");
    }
    else {
        numSend = send(sockfd, data, size, 0);       // send data to the server
        if(numSend < 0) {
            System_printf("sendData2Server::Error while sending data to server\n");
        }
        else {
            retval = true;      // we successfully sent the temperature string
        }
    }
    System_flush();
    close(sockfd);
    return retval;
}

Void clientSocketTask(UArg arg0, UArg arg1)
{
    while(1) {
        // wait for the semaphore that httpTask() will signal
        // when temperature string is retrieved from api.openweathermap.org site
        //
        Semaphore_pend(semaphore0, BIOS_WAIT_FOREVER);

        GPIO_write(Board_LED0, 1); // turn on the LED

        // connect to SocketTest program on the system with given IP/port
        // send hello message whihc has a length of 5.
        //
        if(sendData2Server(SOCKETTEST_IP, OUTGOING_PORT, tempstr, strlen(tempstr))) {
            System_printf("clientSocketTask:: Temperature is sent to the server\n");
            System_flush();
        }

        GPIO_write(Board_LED0, 0);  // turn off the LED
    }
}

void getTimeStr(char *str)
{
    // dummy get time as string function
    // you may need to replace the time by getting time from NTP servers
    //
    strcpy(str, "2021-01-07 12:34:56");
}

float getTemperature(void)
{
    // dummy return
    //
    return atof(tempstr);
}

Void serverSocketTask(UArg arg0, UArg arg1)
{

    static uint8_t hrvalue, averagehr;
    float newHR;

    int serverfd, new_socket, valread, len;
    struct sockaddr_in serverAddr, clientAddr;
    char buffer[30];
    char outstr[30], tmpstr[30];
    bool quit_protocol;

    serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverfd == -1) {
        System_printf("serverSocketTask::Socket not created.. quiting the task.\n");
        return;     // we just quit the tasks. nothing else to do.
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(INCOMING_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Attaching socket to the port
    //
    if (bind(serverfd, (struct sockaddr *)&serverAddr,  sizeof(serverAddr))<0) {
         System_printf("serverSocketTask::bind failed..\n");

         // nothing else to do, since without bind nothing else works
         // we need to terminate the task
         return;
    }
    if (listen(serverfd, 3) < 0) {

        System_printf("serverSocketTask::listen() failed\n");
        // nothing else to do, since without bind nothing else works
        // we need to terminate the task
        return;
    }

    while(1) {

        len = sizeof(clientAddr);
        if ((new_socket = accept(serverfd, (struct sockaddr *)&clientAddr, &len))<0) {
            System_printf("serverSocketTask::accept() failed\n");
            continue;               // get back to the beginning of the while loop
        }

        System_printf("Accepted connection\n"); // IP address is in clientAddr.sin_addr
        System_flush();

        // task while loop
        //
        quit_protocol = false;
        do {

            // let's receive data string
            if((valread = recv(new_socket, buffer, 15, 0))<0) {

                // there is an error. Let's terminate the connection and get out of the loop
                //
                close(new_socket);
                break;
            }

            // let's truncate the received string
            //
            buffer[15]=0;
            if(valread<15) buffer[valread]=0;

            System_printf("message received: %s\n", buffer);

            if(!strcmp(buffer, "SHUTDOWN")) {
                quit_protocol = true;     // it will allow us to get out of while loop
                strcpy(outstr, " ---> SYSTEM CLOSING, STAY HEALTHY <---");
                send(new_socket , outstr , strlen(outstr) , 0);
            }

            else if(!strcmp(buffer, "GETTIME")) {
                int myTime;

                getTimeStr(tmpstr);
                Semaphore_post(semaphore2);
                Semaphore_post(semaphore2);
                Mailbox_pend(mailbox1, &myTime, BIOS_WAIT_FOREVER);
                Mailbox_pend(mailbox1, &myTime, BIOS_WAIT_FOREVER);

                sprintf(outstr, " ---> TIME: %s\n", ctime(&myTime));
                send(new_socket , outstr , strlen(outstr) , 0);
            }
            else if(!strcmp(buffer, "READ HEARTBEAT")) {
                uint16_t totalhr2=0;
                int i;
                for (i = 0; i < 10; i++) {
                    Semaphore_post(semaphore1);
                    Mailbox_pend(mailbox0, &hrvalue, BIOS_WAIT_FOREVER);
                    totalhr2 += hrvalue ;

                    }
                averagehr = totalhr2 /10;
                 newHR = (float)(averagehr);

                sprintf(outstr, "---> HEART BEAT IS =  %5.2f\n", newHR);
                send(new_socket , outstr , strlen(outstr) , 0);
            }


        }
        while(!quit_protocol);

        System_flush();
        close(new_socket);
        BIOS_exit(1);
    }

    close(serverfd);
    return;
}

/*
 *  ======== httpTask ========
 *  Makes a HTTP GET request
 */
void recvTimeStamptFromNTP(char *serverIP, int serverPort, char *data, int size)
{
        System_printf("recvTimeStamptFromNTP start\n");
        System_flush();

        int sockfd, connStat, tri;
        struct sockaddr_in serverAddr;

        sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == -1) {
            System_printf("Socket not created");
            BIOS_exit(-1);
        }
        memset(&serverAddr, 0, sizeof(serverAddr));  // clear serverAddr structure
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(37);     // convert port # to network order
        inet_pton(AF_INET, serverIP , &(serverAddr.sin_addr));

        connStat = connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
        if(connStat < 0) {
            System_printf("sendData2Server::Error while connecting to server\n");
            if(sockfd>0) close(sockfd);
            BIOS_exit(-1);
        }

        tri = recv(sockfd, takenTime, sizeof(takenTime), 0);
        if(tri < 0) {
            System_printf("Error while receiving data from server\n");
            if (sockfd > 0) close(sockfd);
            BIOS_exit(-1);
        }
        if (sockfd > 0) close(sockfd);
}

Void socketTask(UArg arg0, UArg arg1)
{

        // wait for the semaphore that httpTask() will signal
        // when temperature string is retrieved from api.openweathermap.org site
        //
        Semaphore_pend(semaphore3, BIOS_WAIT_FOREVER);

        GPIO_write(Board_LED0, 1); // turn on the LED

        // connect to SocketTest program on the system with given IP/port
        // send hello message whihc has a length of 5.
        //
      // sendData2Server(SOCKETTEST_IP, 5011, tempstr, strlen(tempstr));
        recvTimeStamptFromNTP(TIMEIP, 37,timestr, strlen(timestr));
        GPIO_write(Board_LED0, 0);  // turn off the LED

        // wait for 5 seconds (5000 ms)
        //

}

bool createTasks(void)
{
    static Task_Handle taskHandle1, taskHandle2, taskHandle3, taskHandle4, taskHandle5;
    Task_Params taskParams;
    Error_Block eb;

    Error_init(&eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 1;
    taskHandle1 = Task_create((Task_FuncPtr)SWI_ISR, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 1;
    taskHandle2 = Task_create((Task_FuncPtr)clientSocketTask, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 1;
    taskHandle3 = Task_create((Task_FuncPtr)serverSocketTask, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 1;
    taskHandle4 = Task_create((Task_FuncPtr)taskFxn, &taskParams, &eb);

    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.priority = 1;
    taskHandle5 = Task_create((Task_FuncPtr)socketTask, &taskParams, &eb);

    if (taskHandle1 == NULL ||  taskHandle2 == NULL || taskHandle3 == NULL || taskHandle4 == NULL || taskHandle5 == NULL) {
        printError("netIPAddrHook: Failed to create HTTP, Socket and Server Tasks\n", -1);
        return false;
    }

    return true;
}

//  This function is called when IP Addr is added or deleted
//
void netIPAddrHook(unsigned int IPAddr, unsigned int IfIdx, unsigned int fAdd)
{
    // Create a HTTP task when the IP address is added
    if (fAdd) {
        createTasks();
    }
}


int main(void)

{
    /* Call board init functions */
    Board_initGeneral();
    Board_initGPIO();
    Board_initEMAC();
    Board_initI2C();

    /* Turn on user LED */
    GPIO_write(Board_LED0, Board_LED_ON);

    System_printf("Starting the HTTP GET example\nSystem provider is set to "
            "SysMin. Halt the target to view any SysMin contents in ROV.\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();


    /* Start BIOS */
    BIOS_start();

    return (0);
}
