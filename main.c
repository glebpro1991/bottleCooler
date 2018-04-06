#include <Hardware/CS_Driver.h>
#include <Devices/MSPIO.h>
#include <Devices/ESP8266.h>
#include <ti/devices/msp432p4xx/driverlib/driverlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>

/***** GLOBAL VARIABLES *****/
uint_fast16_t TEMP = 15;
uint_fast16_t TARGET = 2;
uint_fast16_t STATEFLAG = 1;
uint_fast16_t ADDTARGET,SUBTARGET;
uint_fast16_t MAXTEMP = 19;
uint_fast16_t MINTEMP = 0;

// DEPLOYMENT
//char HTTP_WebPage[] = "devweb2017.cis.strath.ac.uk/";
// LOCAL
// char HTTP_Request[] = "GET /CX318/index.php?status=ok HTTP/1.0\r\n\r\n";

// char YOUR_SSID[] = "phone";
// char YOUR_SSID_PASSWORD[] = "password";
// char YOUR_SSID[] = "TALKTALK7C01A8";
// char YOUR_SSID_PASSWORD[] = "VDK9JYXD";

eUSCI_UART_Config UART0Config = {
     EUSCI_A_UART_CLOCKSOURCE_SMCLK,
     13,
     0,
     37,
     EUSCI_A_UART_NO_PARITY,
     EUSCI_A_UART_LSB_FIRST,
     EUSCI_A_UART_ONE_STOP_BIT,
     EUSCI_A_UART_MODE,
     EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION
};

eUSCI_UART_Config UART2Config = {
     EUSCI_A_UART_CLOCKSOURCE_SMCLK,
     13,
     0,
     37,
     EUSCI_A_UART_NO_PARITY,
     EUSCI_A_UART_LSB_FIRST,
     EUSCI_A_UART_ONE_STOP_BIT,
     EUSCI_A_UART_MODE,
     EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION
};

Timer_A_PWMConfig pwmConfig = {
//    TIMER_A_CLOCKSOURCE_SMCLK,
//    TIMER_A_CLOCKSOURCE_DIVIDER_1,
//    32000,
//    TIMER_A_CAPTURECOMPARE_REGISTER_1,
//    TIMER_A_OUTPUTMODE_RESET_SET,
//    3200

    TIMER_A_CLOCKSOURCE_SMCLK,
    TIMER_A_CLOCKSOURCE_DIVIDER_64,
    32000,
    TIMER_A_CAPTURECOMPARE_REGISTER_1,
    TIMER_A_OUTPUTMODE_RESET_SET,
    3200
};

void compareTempTarget(unsigned int temp, unsigned int target) {
    if(TARGET < TEMP){
        STATEFLAG = 0;
        GPIO_High(GPIO_PORT_P2, GPIO_PIN6);
    } else if(TARGET > TEMP){
        STATEFLAG = 2;
        GPIO_Low(GPIO_PORT_P2, GPIO_PIN6);
    } else {
        STATEFLAG = 1;
    }
}

void setDutyCycle() {
    uint_fast16_t delta = abs(TEMP - TARGET);
    if(delta > abs(MAXTEMP)) {
        delta = MAXTEMP;
    }

    delta = (32000/abs(MAXTEMP - MINTEMP)) * delta;
    pwmConfig.dutyCycle = delta;
}

void updateTarget() {
    TARGET += ADDTARGET - SUBTARGET;
    ADDTARGET = 0;
    SUBTARGET = 0;

    if(TARGET < MINTEMP) {
        TARGET = MINTEMP;
    } else if (TARGET > MAXTEMP) {
        TARGET = MAXTEMP;
    }
}

void PORT1_IRQHandler(uint8_t index) {
    uint32_t status;
    status = MAP_GPIO_getEnabledInterruptStatus(GPIO_PORT_P1);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P1, status);

    if(status & GPIO_PIN1){
        SUBTARGET++;
        setDutyCycle();
    } else if (status & GPIO_PIN4) {
        ADDTARGET++;
        setDutyCycle();
    }

    MAP_Timer_A_generatePWM(TIMER_A0_BASE, &pwmConfig);
}

void initLEDs() {
   GPIO_Init(GPIO_PORT_P6, GPIO_PIN1);
   GPIO_Init(GPIO_PORT_P1, GPIO_PIN0);
   GPIO_Init(GPIO_PORT_P2, GPIO_PIN0);
   GPIO_Init(GPIO_PORT_P2, GPIO_PIN1);
   GPIO_Init(GPIO_PORT_P2, GPIO_PIN2);
   GPIO_Init(GPIO_PORT_P2, GPIO_PIN6);
}

void setSWInterrupts() {
    MAP_GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN1 | GPIO_PIN4);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P1, GPIO_PIN1 | GPIO_PIN4);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P1, GPIO_PIN1);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P1, GPIO_PIN4);
    MAP_Interrupt_enableInterrupt(INT_PORT1);
}

void configurePWM() {
    MAP_GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P2, GPIO_PIN4, GPIO_PRIMARY_MODULE_FUNCTION);
    MAP_GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P1, GPIO_PIN1);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P1, GPIO_PIN1);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P1, GPIO_PIN1);
}

void updateTemperature() {
    updateTarget();
    compareTempTarget(TEMP,TARGET);
    MSPrintf(EUSCI_A0_BASE, "TEMP: %i / TARGET: %i / STATEFLAG: %i\r\n",
             TEMP, TARGET, STATEFLAG);
}

void parseTemperature() {
    char *Data = ESP8266_GetBuffer();
    char *temp1 = strstr(Data, "entry_id");
    char *temp2 = strstr(temp1, "field1");

    char buffer[3];
    memcpy(buffer, &temp2[9], 2);
    buffer[2] = '\0';

    int temperature = atoi(buffer);
    MSPrintf(EUSCI_A0_BASE, "Temperature obtained: %i\r\n", temperature);

    TEMP = temperature;
}

void connectToAP() {
    char *ESP8266_Data = ESP8266_GetBuffer();

     ESP8266_HardReset();
     __delay_cycles(48000000);
    UART_Flush(EUSCI_A2_BASE);

    if(!ESP8266_CheckConnection()){
        MSPrintf(EUSCI_A0_BASE, "Failed MSP432 UART connection\r\n");
        while(1);
    }

    MSPrintf(EUSCI_A0_BASE, "Successfully connected to MSP432\r\n\r\n");
    MSPrintf(EUSCI_A0_BASE, "Checking available Access Points\r\n\r\n");

    if(!ESP8266_AvailableAPs()) {
        MSPrintf(EUSCI_A0_BASE, "Failed to obtain Access Points\n\r ERROR: %s \r\n",ESP8266_Data);
        while(1);
    }

    MSPrintf(EUSCI_A0_BASE, "Here are the available Access Points: %s\r\n\r\n", ESP8266_Data);

    if(!ESP8266_EnableMultipleConnections(true)) {
        MSPrintf(EUSCI_A0_BASE, "Failed to set multiple connections\r\n");
        while(1);
    }

    MSPrintf(EUSCI_A0_BASE, "Multiple connections enabled\r\n\r\n");
}

void connectToServer() {
    char HTTP_WebPage[] = "192.168.1.6";
    char *ESP8266_Data = ESP8266_GetBuffer();

    if(!ESP8266_EstablishConnection('1', TCP, HTTP_WebPage, "80")) {
        MSPrintf(EUSCI_A0_BASE, "Failed to connect to: %s\r\nERROR: %s\r\n",
                 HTTP_WebPage, ESP8266_Data);
        while(1);
    }

    MSPrintf(EUSCI_A0_BASE, "Connected to: %s\r\n\r\n", HTTP_WebPage);
}

void connectToRemoteAPI() {
    char remoteTempAPI[] = "api.thingspeak.com";

    if(!ESP8266_EstablishConnection('0', TCP, remoteTempAPI, "80")) {
        MSPrintf(EUSCI_A0_BASE, "Failed to connect to: %s\r\nERROR: %s\r\n",
                 remoteTempAPI, ESP8266_GetBuffer());
        while(1);
    }
}

void sendGetRequest() {
    char remoteTempAPI[] = "api.thingspeak.com";
    char GET_Request[] = "GET /channels/466727/fields/1.json?api_key=KZA9WE5D9PJ5F66G&results=1 HTTP/1.0\r\n\r\n";
    uint32_t GET_size = sizeof(GET_Request) - 1;

    if(!ESP8266_SendData('0', GET_Request, GET_size)) {
        MSPrintf(EUSCI_A0_BASE, "Failed to send: %s to %s \r\nError: %s\r\n", GET_Request, remoteTempAPI, ESP8266_GetBuffer());
    }

    MSPrintf(EUSCI_A0_BASE, "Data sent: %sto %s\r\n\r\nESP8266 Data Received: %s\r\n", GET_Request, remoteTempAPI, ESP8266_GetBuffer());
    parseTemperature();
}

void sendPostRequest() {
    char HTTP_WebPage[] = "192.168.1.6";
    char POST_Request[] = "POST /CX318/index.php HTTP/1.0\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length:15\r\n\r\n temperature=13";
    uint32_t POST_size = sizeof(POST_Request) - 1;
    char *ESP8266_Data = ESP8266_GetBuffer();

    if(!ESP8266_SendData('1', POST_Request, POST_size)) {
        MSPrintf(EUSCI_A0_BASE, "Failed to send: %s to %s \r\nError: %s\r\n", POST_Request, HTTP_WebPage, ESP8266_Data);
    }
    MSPrintf(EUSCI_A0_BASE, "Data sent: %s to %s\r\n\r\nESP8266 Data Received: %s\r\n", POST_Request, HTTP_WebPage, ESP8266_Data);
}

void main(void) {
	MAP_WDT_A_holdTimer(); /* Halting the Watchdog */
	CS_Init(); /*MSP432 Running at 24 MHz*/
	initLEDs();
	setSWInterrupts();
	configurePWM();
	UART_Init(EUSCI_A0_BASE, UART0Config);
	UART_Init(EUSCI_A2_BASE, UART2Config);
	MAP_Timer_A_generatePWM(TIMER_A0_BASE, &pwmConfig);
	MAP_Interrupt_enableMaster();
	connectToAP();

    // setUpServer();
    // ESP8266_Terminal();

    while(1) {
         connectToRemoteAPI();
         sendGetRequest();
        // connectToServer();
        // sendPostRequest();

        updateTemperature();
        GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);
        __delay_cycles(10000000);
    }
}
