#include "wiced.h"
#include "stdio.h"
#include "string.h"
#include "mqtt_api.h"
#include "resources.h"
#include "cJSON.h"
#include "stdbool.h"

/************************* User defines *******************************************/
#define RX_BUFFER_SIZE 20
#define RxDataSize 5
#define ShadowUpdateMsg "{ \"state\": {\"reported\": { \"status\": \"%s\" , \"doorLock\":\"%s\"} } }"
#define RX_BUFFER_SIZE 20
#define countMsgIn "count = %d\r\n"
#define keyMsgIn "key = %c"

#define WICED3 WICED_GPIO_28
#define WICED4 WICED_GPIO_32
#define WICED5 WICED_GPIO_29
#define WICED6 WICED_GPIO_30
#define RS      WICED4
#define Enable  WICED5
#define D4      ARD_GPIO0
#define D5      ARD_GPIO1
#define D6      ARD_GPIO2
#define D7      ARD_GPIO3
#define ARD_GPIO10 ARD_SS
#define ARD_GPIO11 ARD_MOSI
#define ARD_GPIO12 ARD_MISO
#define ARD_GPIO13 ARD_SCK

/************************* (Copy from publisher.c) *******************************************/
#define MQTT_BROKER_ADDRESS                 "a21yyexai8eunn-ats.iot.us-east-1.amazonaws.com"
#define MQTT_BROKER_PEER_COMMON_NAME        "*.iot.us-east-1.amazonaws.com"
#define WICED_TOPIC                         "$aws/things/%s/shadow/update"
#define Shadow_Document_TOPIC               "$aws/things/%s/shadow/update/documents"
#define CLIENT_ID                           "wiced_publisher_aws"
#define MQTT_REQUEST_TIMEOUT                (5000)
#define MQTT_DELAY_IN_MILLISECONDS          (1000)
#define MQTT_MAX_RESOURCE_SIZE              (0x7fffffff)
#define MQTT_PUBLISH_RETRY_COUNT            (3)
#define MQTT_SUBSCRIBE_RETRY_COUNT          (3)
/********************************************************************/

/************************* User global variables *******************************************/
char receiveData[20]="";
static wiced_thread_t receiveUartHandle , PublishMessageHandle , PeripheralHandle;
wiced_timer_t timer1_handle;
char                  *msg = "Hello World!!";
wiced_gpio_t writes[4] = {ARD_GPIO4,ARD_GPIO5,ARD_GPIO6,ARD_GPIO7};
wiced_gpio_t reads[4] = {ARD_GPIO8,ARD_GPIO9,ARD_GPIO10,ARD_GPIO11};
const char keymap[4][4] = {     // 設定按鍵的「行、列」代表值
    {'*','0','#','D'},
    {'7','8','9','C'},
    {'4','5','6','B'},
    {'1','2','3','A'}
};
int i = 0;


/*Use for Shadow*/
char                  ShadowUpdateStr[100] = "";
char                  *out;
cJSON                 *root;
cJSON *json, *current, *state, *desired, *reported;
char *status="OFF", *doorLock="ON";
char *JSON_temp;
int shadowSW = 0;   //Use this variable to choose the data of shadow report


/*Use for MQTT*/
wiced_mqtt_object_t   mqtt_object;
uint32_t              size_out = 0;
int                   retries = 0;
char                  *IotThing = "KEVIN_IoT_Thing";
char                  IotShadowTopic[100] = "" , IotShadowDocumentTopic[100] = "" , *IotControlTopic = "Control";
char                  *on = "ON", *off = "OFF";
wiced_mqtt_topic_msg_t receivedMsg;
bool shadowReceive_flag = false;

/******************************************************
 *               Variable Definitions (Copy from publisher.c)
 ******************************************************/
static wiced_ip_address_t                   broker_address;
static wiced_mqtt_event_type_t              expected_event;
static wiced_semaphore_t                    msg_semaphore;
static wiced_semaphore_t                    wake_semaphore;
static wiced_semaphore_t                    MQTTend_semaphore;
static wiced_mqtt_security_t                security;
static wiced_bool_t                         is_connected = WICED_FALSE;

/******************************************************
 *               Static Function Definitions
 ******************************************************/
void send_to_lcd(char, int);
void lcd_send_cmd (char);
void lcd_send_data (char);
void lcd_put_cur(int, int);
void lcd_init (void);
void string_ini(char*);
void lcd_send_string (char *);
void kevin_gpio_write(wiced_gpio_t,int);

void kevin_gpio_write(wiced_gpio_t gpio, int logic)
{
    if(logic)
        wiced_gpio_output_high( gpio );
    else
        wiced_gpio_output_low( gpio );
}

void setWrites(int a)
{
  for(i=0;i<4;i++)
  {
    if(i!=a)
        kevin_gpio_write(writes[i],0);
    else
        kevin_gpio_write(writes[i],1);
  }
}

void send_to_lcd(char data, int rs)
{
    kevin_gpio_write(RS, rs);
    kevin_gpio_write(Enable, 1);

    wiced_rtos_delay_milliseconds( 1 );

    kevin_gpio_write(D7, (data>>3)&0x01);
    kevin_gpio_write(D6, (data>>2)&0x01);
    kevin_gpio_write(D5, (data>>1)&0x01);
    kevin_gpio_write(D4, (data)&0x01);

    //kevin_gpio_write(Enable, 1);
    wiced_rtos_delay_milliseconds( 1 );

    kevin_gpio_write(Enable, 0);
    wiced_rtos_delay_milliseconds( 1 );
}

void lcd_send_cmd (char cmd)
{
    char datatosend;
    // send upper nibble first
    datatosend = ((cmd>>4)&0x0f);
    send_to_lcd(datatosend,0);  // RS must be while sending command
    // send Lower Nibble
    datatosend = ((cmd)&0x0f);
    send_to_lcd(datatosend, 0);
}

void lcd_send_data (char data)
{
    char datatosend;

    // send higher nibble
    datatosend = ((data>>4)&0x0f);
    send_to_lcd(datatosend, 1);  // rs =1 for sending data
    // send Lower nibble
    datatosend = ((data)&0x0f);
    send_to_lcd(datatosend, 1);
}


void lcd_put_cur(int row, int col)
{
    switch (row)
    {
        case 0:
            col |= 0x80;
            break;
        case 1:
            col |= 0xC0;
            break;
    }
    lcd_send_cmd (col);
}

void lcd_init (void)
{
    // 4 bit initialisation
    wiced_rtos_delay_milliseconds(50);  // wait for >40ms
    lcd_send_cmd (0x30);
    wiced_rtos_delay_milliseconds(5);  // wait for >4.1ms
    lcd_send_cmd (0x30);
    wiced_rtos_delay_milliseconds(1);  // wait for >100us
    lcd_send_cmd (0x30);
    wiced_rtos_delay_milliseconds(10);
    lcd_send_cmd (0x20);  // 4bit mode
    wiced_rtos_delay_milliseconds(10);

  // dislay initialisation
    lcd_send_cmd (0x28); // Function set --> DL=0 (4 bit mode), N = 1 (2 line display) F = 0 (5x8 characters)
    wiced_rtos_delay_milliseconds(1);
    lcd_send_cmd (0x08); //Display on/off control --> D=0,C=0, B=0  ---> display off
    wiced_rtos_delay_milliseconds(1);
    lcd_send_cmd (0x01);  // clear display
    wiced_rtos_delay_milliseconds(1);
    wiced_rtos_delay_milliseconds(1);
    lcd_send_cmd (0x06); //Entry mode set --> I/D = 1 (increment cursor) & S = 0 (no shift)
    wiced_rtos_delay_milliseconds(1);
    lcd_send_cmd (0x0C); //Display on/off control --> D = 1, C and B = 0. (Cursor and blink, last two bits)
    string_ini((char *)"        ");    //if I take these code out, LCD will
    string_ini((char *)"        ");
    string_ini((char *)"        ");
}

void string_ini(char *display_string)
{
  int length = strlen(display_string);
  int i = 0;
  int col = 0, row = 0;
  for (i = 0 ; i < length ; i++)
  {
    lcd_put_cur(row , col);
    lcd_send_data(display_string[i]);
    col++;
      if(col > 15)
      {
        row++;
        col = 0;
      }
      if(row>1)
      {
        row = 0;
      }
  }
}

void lcd_send_string(char *display_string)
{
  int length = strlen(display_string);
  int i = 0;
  for (i = 0 ; i < length ; i++)
  {
    lcd_send_data(display_string[i]);
  }
}
bool led_state = 0;
int count = 0;
void timer1Function(void *arg)
{
    if(count >= 100)
    {
        count = 0;
        led_state = !led_state;
        kevin_gpio_write(ARD_GPIO6, led_state);
        kevin_gpio_write(ARD_GPIO7, led_state);
        kevin_gpio_write(ARD_GPIO8, led_state);
        kevin_gpio_write(ARD_GPIO9, led_state);
        kevin_gpio_write(ARD_GPIO10, led_state);
        kevin_gpio_write(ARD_GPIO11, led_state);
        kevin_gpio_write(ARD_GPIO12, led_state);
        kevin_gpio_write(ARD_GPIO13, led_state);
    }
    else
    {
        count++;
    }

}

/*
 * A blocking call to an expected event.
 * 
 */
static wiced_result_t wait_for_response( wiced_mqtt_event_type_t event, uint32_t timeout )
{
    if ( wiced_rtos_get_semaphore( &msg_semaphore, timeout ) != WICED_SUCCESS )
    {
        return WICED_ERROR;
    }
    else
    {
        if ( event != expected_event )
        {
            return WICED_ERROR;
        }
    }
    return WICED_SUCCESS;
}

/*
 * Call back function to handle connection events.
 * (How to know we need what kind of parameter?
 *  Check by the parameter type of "mqtt_conn_open", you will see the data type of call back function)
 */
static wiced_result_t mqtt_connection_event_cb( wiced_mqtt_object_t mqtt_object, wiced_mqtt_event_info_t *event )
{
    switch ( event->type )
    {
        case WICED_MQTT_EVENT_TYPE_DISCONNECTED:
        {
            is_connected = WICED_FALSE; //Use this variable to check the status of MQTT connection
        }
        case WICED_MQTT_EVENT_TYPE_CONNECT_REQ_STATUS:
        case WICED_MQTT_EVENT_TYPE_PUBLISHED:
        case WICED_MQTT_EVENT_TYPE_SUBCRIBED:
        case WICED_MQTT_EVENT_TYPE_UNSUBSCRIBED:
        {
            expected_event = event->type;
            wiced_rtos_set_semaphore( &msg_semaphore );
        }
            break;
        case WICED_MQTT_EVENT_TYPE_PUBLISH_MSG_RECEIVED:
        {
            receivedMsg = event->data.pub_recvd;

            JSON_temp = (char*)receivedMsg.data;
            JSON_temp[receivedMsg.data_len] = '\0';
            debugMsg(IotShadowDocumentTopic);
            debugMsg("\r\n");
            debugMsg(receivedMsg.topic);
            debugMsg("\r\n");
            debugMsg("46546\r\n");
            if(strncmp((char *)receivedMsg.topic, IotShadowDocumentTopic, receivedMsg.topic_len) == 0)
            {

                if(strstr(JSON_temp,"desired") != NULL)
                {
                    json = cJSON_Parse(JSON_temp);
                    current = cJSON_GetObjectItem(json, "current");
                    state = cJSON_GetObjectItem(current, "state");
                    desired = cJSON_GetObjectItem(state, "desired");
                    reported = cJSON_GetObjectItem(state, "reported");
                    debugMsg("\r\n");
                    debugMsg(cJSON_Print(desired));
                    debugMsg("\r\n");
                    debugMsg(cJSON_Print(reported));
                    debugMsg("\r\n");
                    if(strcmp(cJSON_Print(desired) , cJSON_Print(reported)) != 0)
                    {
                        debugMsg("\r\n789798\r\n");
                        status = cJSON_GetObjectItem(desired, "status")->valuestring;
                        doorLock = cJSON_GetObjectItem(desired, "doorLock")->valuestring;
                        sprintf(ShadowUpdateStr, (char *)"{ \"state\": {\"reported\": { \"status\": \"%s\" , \"doorLock\":\"%s\"} } }", status, doorLock);
                        debugMsg("status: ");
                        debugMsg(status);
                        debugMsg("\r\ndoorLock: ");
                        debugMsg(doorLock);
                        debugMsg("\r\n");
                        root = cJSON_Parse(ShadowUpdateStr);
                        out=cJSON_Print(root);
                        //Send what data? Copy to msg (We will publish msg to Topic)
                        strcpy(msg,out);
                        //wiced_uart_transmit_bytes(WICED_UART_1 , msg , strlen(msg));
                        wiced_rtos_set_semaphore( &wake_semaphore );
                    }
                }
                else
                    debugMsg("\r\nNot desired request!!\r\n");
            }
            //Use topic "control" to give some instruction to IoT device, then in the embedded I will publish relative results
            else if(strncmp((char *)receivedMsg.topic, "Control", receivedMsg.topic_len) == 0)
            {
                debugMsg((char *)receivedMsg.data);
            }
        }
        break;
        default:
            break;
    }
    return WICED_SUCCESS;
}

/*
 * Open a connection and wait for MQTT_REQUEST_TIMEOUT period to receive a connection open OK event
 */
static wiced_result_t mqtt_conn_open( wiced_mqtt_object_t mqtt_obj, wiced_ip_address_t *address, wiced_interface_t interface, wiced_mqtt_callback_t callback, wiced_mqtt_security_t *security )
{
    wiced_mqtt_pkt_connect_t conninfo;
    wiced_result_t ret = WICED_SUCCESS;

    memset( &conninfo, 0, sizeof( conninfo ) );
    conninfo.port_number = 0;                           /*Select the port we want to connect. 0 means default values(8883 or 1883)*/
    conninfo.mqtt_version = WICED_MQTT_PROTOCOL_VER4;   /*MQTT version, supported versions are 3 and 4.*/
    conninfo.clean_session = 1;                         /*Indicates if the session to be cleanly started(?)*/
    conninfo.client_id = (uint8_t*) CLIENT_ID;
    conninfo.keep_alive = 5;                            /*Indicates keep alive interval to Broker(?)*/
    conninfo.password = NULL;                           /* Password to connect to Broker */
    conninfo.username = NULL;                           /* User name to connect to Broker */
    conninfo.peer_cn = (uint8_t*) MQTT_BROKER_PEER_COMMON_NAME;

    /*
     * Because we have already save relational security information in the variable security, so we can create a safety MQTT connection
     * address : IP address of Broker. In the few steps before, we have already get the IP address of Broker by using wiced_hostname_lookup
     * callback : The call back function will be triggered when some connection event happen
     * conninfo : Some information related to connection of MQTT 
     */
    ret = wiced_mqtt_connect( mqtt_obj, address, interface, callback, security, &conninfo );
    if ( ret != WICED_SUCCESS )
    {
        return WICED_ERROR;
    }
    if ( wait_for_response( WICED_MQTT_EVENT_TYPE_CONNECT_REQ_STATUS, MQTT_REQUEST_TIMEOUT ) != WICED_SUCCESS )
    {
        return WICED_ERROR;
    }
    return WICED_SUCCESS;
}

/*
 * Close a connection and wait for 5 seconds to receive a connection close OK event
 */
static wiced_result_t mqtt_conn_close( wiced_mqtt_object_t mqtt_obj )
{
    if ( wiced_mqtt_disconnect( mqtt_obj ) != WICED_SUCCESS )
    {
        return WICED_ERROR;
    }
    if ( wait_for_response( WICED_MQTT_EVENT_TYPE_DISCONNECTED, MQTT_REQUEST_TIMEOUT ) != WICED_SUCCESS )
    {
        return WICED_ERROR;
    }
    return WICED_SUCCESS;
}

/*
 * Subscribe to WICED_TOPIC and wait for 5 seconds to receive an ACM.
 */
static wiced_result_t mqtt_app_subscribe( wiced_mqtt_object_t mqtt_obj, char *topic, uint8_t qos )
{
    wiced_mqtt_msgid_t pktid;
    pktid = wiced_mqtt_subscribe( mqtt_obj, topic, qos );
    if ( pktid == 0 )
    {
        return WICED_ERROR;
    }
    if ( wait_for_response( WICED_MQTT_EVENT_TYPE_SUBCRIBED, MQTT_REQUEST_TIMEOUT ) != WICED_SUCCESS )
    {
        return WICED_ERROR;
    }
    return WICED_SUCCESS;
}

/*
 * Publish (send) message to WICED_TOPIC and wait for 5 seconds to receive a PUBCOMP (as it is QoS=2).
 */
static wiced_result_t mqtt_app_publish( wiced_mqtt_object_t mqtt_obj, uint8_t qos, uint8_t *topic, uint8_t *data, uint32_t data_len )
{
    wiced_mqtt_msgid_t pktid;   //The variable used for check connection result

    pktid = wiced_mqtt_publish( mqtt_obj, topic, data, data_len, qos );

    if ( pktid == 0 )
    {
        return WICED_ERROR;
    }

    if ( wait_for_response( WICED_MQTT_EVENT_TYPE_PUBLISHED, MQTT_REQUEST_TIMEOUT ) != WICED_SUCCESS )
    {
        return WICED_ERROR;
    }
    return WICED_SUCCESS;
}

/******************* User Function *********************************/
void peripheralFunction(wiced_thread_arg_t arg)
{

    int i = 0;
    int x,y;
    bool readInput1 = 0, readInput2 = 0;
    int count = 0;
    char  countMsgOut[50];
    while(1)
    {
        debugMsg("465\r\n");
        kevin_gpio_write(ARD_GPIO12, 0);
        kevin_gpio_write(ARD_GPIO12, 1);
        wiced_rtos_delay_microseconds(20);
        kevin_gpio_write(ARD_GPIO12, 0);
        while(!wiced_gpio_input_get(ARD_GPIO13));
        count = 0;
        while(wiced_gpio_input_get(ARD_GPIO13))
        {
            count++;
            wiced_rtos_delay_microseconds(1);
        }
        sprintf(countMsgOut, (char*)countMsgIn, count);
        debugMsg(countMsgOut);

       // wiced_rtos_delay_milliseconds(500);
        for(x=0;x<4;x++)
        {
          for(y=0;y<4;y++)
          {
            setWrites(y);////
            //digitalWrite(writes[x],1);
            readInput2 = readInput1;
            readInput1 = wiced_gpio_input_get(reads[x]);
            if(readInput1 && !readInput2)
            {
              sprintf(countMsgOut, (char*)keyMsgIn, keymap[y][x]);
              lcd_put_cur(0, 0);
              lcd_send_cmd(0x01);
              lcd_send_string(countMsgOut);
              //
              wiced_rtos_delay_milliseconds(300);
              //readInput = 0;
              break;
            }
          }
        }
    }
}


void receiveUART(wiced_thread_arg_t arg)
{
    uint32_t dataSize = RxDataSize;

    while(wiced_uart_receive_bytes(WICED_UART_3,receiveData,&dataSize,WICED_WAIT_FOREVER ) == WICED_SUCCESS)
    {

            //Set the condition determine when does MQTT should send the data to Rule Engine
            if(strcmp(receiveData , "12345") > 0)
            {
                if(strcmp(receiveData , "55555") > 0)
                    shadowSW = 1;
                else
                    shadowSW = 0;
                //shadowSW is used to choose shadow state
                switch(shadowSW)
                {
                    case 0:
                        sprintf(ShadowUpdateStr, (char *)"{ \"state\": {\"reported\": { \"status\": \"%s\" , \"doorLock\":\"%s\"} } }", off, off);
                        break;
                    case 1:
                        sprintf(ShadowUpdateStr, (char *)"{ \"state\": {\"reported\": { \"status\": \"%s\" , \"doorLock\":\"%s\"} } }", off, on);
                        break;
                    case 2:
                        sprintf(ShadowUpdateStr, (char *)"{ \"state\": {\"reported\": { \"status\": \"%s\" , \"doorLock\":\"%s\"} } }", on, off);
                        break;
                    case 3:
                        sprintf(ShadowUpdateStr, (char *)"{ \"state\": {\"reported\": { \"status\": \"%s\" , \"doorLock\":\"%s\"} } }", on, on);
                        break;
                    default:
                        break;
                }

                root = cJSON_Parse(ShadowUpdateStr);
                out=cJSON_Print(root);
                //Send what data? Copy to msg (We will publish msg to Topic)
                strcpy(msg,out);
                //wiced_uart_transmit_bytes(WICED_UART_1 , msg , strlen(msg));
                wiced_rtos_set_semaphore( &wake_semaphore );
            }
    }

}

//Publish Message to
void PublishMessage(wiced_thread_arg_t arg)
{
    /******************** Copy from publisher.c ***************/
    wiced_result_t        ret = WICED_SUCCESS;
    /**********************************************************/

    while(1)
    {
        retries = 0;

        //Wait for sending data(Maybe when receive correct data)
        wiced_rtos_get_semaphore( &wake_semaphore,WICED_WAIT_FOREVER );
        if ( is_connected == WICED_FALSE)
        {
            break;
        }

        //Publish the data
        debugMsg(("[MQTT] Publishing..."));
        do
        {
            /*
             * Publish message by using mqtt_object we set before.
             * It has already create a safe connection to AWS IoT Broker
             * WICED_MQTT_QOS_DELIVER_AT_LEAST_ONCE : This means QoS = 1 (proof your message will arrive, but maybe many times)
             */
            ret = mqtt_app_publish( mqtt_object, WICED_MQTT_QOS_DELIVER_AT_LEAST_ONCE, (uint8_t*)/* "Control"*/IotShadowTopic, (uint8_t*) msg, strlen( msg ) );
            retries++ ;
        } while ( ( ret != WICED_SUCCESS ) && ( retries < MQTT_PUBLISH_RETRY_COUNT ) );
        if ( ret != WICED_SUCCESS )
        {
            debugMsg((" Failed\r\n"));
            break;
        }
        else
        {
            debugMsg((" Success\r\n"));
        }

        wiced_rtos_delay_milliseconds( 100 );

        wiced_gpio_output_low( WICED_LED2 );
        wiced_rtos_delay_milliseconds( 100 );
        wiced_gpio_output_high( WICED_LED2 );
        wiced_rtos_delay_milliseconds( 100 );
    }
    /*
     * If run to here means MQTT disconnect, set the semaphore will let main function keep execute
     * So it will deinit some resources
     */
    wiced_rtos_set_semaphore( &MQTTend_semaphore );
}

void debugMsg(char debugMessage[50])
{
    wiced_uart_transmit_bytes(WICED_UART_1 , debugMessage , strlen(debugMessage));
}

/*
 *  main function
 */
void application_start()
{
    wiced_result_t        ret = WICED_SUCCESS;
    int                   connection_retries = 0, i = 0;

    wiced_init();

    for(i=0;i<4;i++)
    {
      wiced_gpio_init(writes[i],OUTPUT_PUSH_PULL);
      wiced_gpio_init(reads[i],INPUT_PULL_UP);
      kevin_gpio_write(writes[3],0);
    }
    wiced_gpio_init(ARD_GPIO0, OUTPUT_PUSH_PULL);
    wiced_gpio_init(ARD_GPIO1, OUTPUT_PUSH_PULL);
    wiced_gpio_init(ARD_GPIO2, OUTPUT_PUSH_PULL);
    wiced_gpio_init(ARD_GPIO3, OUTPUT_PUSH_PULL);
    wiced_gpio_init(WICED3, OUTPUT_PUSH_PULL);
    wiced_gpio_init(WICED4, OUTPUT_PUSH_PULL);
    wiced_gpio_init(WICED5, OUTPUT_PUSH_PULL);
    wiced_gpio_init(WICED6, OUTPUT_PUSH_PULL);
    wiced_gpio_init(ARD_GPIO12 , OUTPUT_PUSH_PULL);
    wiced_gpio_init(ARD_GPIO13 , INPUT_PULL_UP);
   // wiced_gpio_init(ARD_GPIO11 , OUTPUT_PUSH_PULL);

    //Customize UART
    wiced_uart_config_t uart_config=
    {
            .baud_rate = 115200,
            .data_width = DATA_WIDTH_8BIT,
            .parity = STOP_BITS_1,
            .flow_control = FLOW_CONTROL_DISABLED
    };

    //Initialize LCD
    lcd_init();
    lcd_put_cur(0, 0);
    lcd_send_cmd(0x01);
    lcd_send_string("Hello world!!");

    //Create a buffer for UART rx data, then initial UART3, which is routed to P6:38(RX)�BP6:39(TX)
    wiced_ring_buffer_t rx_buffer;
    uint8_t rx_data[RX_BUFFER_SIZE];
    ring_buffer_init(&rx_buffer, rx_data, RX_BUFFER_SIZE );
    wiced_uart_init(WICED_UART_3,&uart_config,&rx_buffer);

    //Initial UART1, this is use for debugging
    wiced_uart_init(WICED_UART_1,&uart_config,NULL);
    debugMsg("Application start\r\n");

    /* 
     * Get AWS root certificate, client certificate and private key respectively 
     * So we can safely connect to AWS IoT core by using MQTT in future steps
     */

    //Root CA save to variable security.ca_cert, also save the length of Root CA to variable security.ca_cert_len
    resource_get_readonly_buffer( &resources_apps_DIR_aws_iot_DIR_rootca_cer, 0, MQTT_MAX_RESOURCE_SIZE, &size_out, (const void **) &security.ca_cert );
    security.ca_cert_len = size_out;

    //Device CA save to variable security.cert, also save the length of Device CA to variable security.cert_len
    resource_get_readonly_buffer( &resources_apps_DIR_aws_iot_DIR_client_cer, 0, MQTT_MAX_RESOURCE_SIZE, &size_out, (const void **) &security.cert );
    if(size_out < 64)
    {
        debugMsg( "\nNot a valid Certificate! Please replace the dummy certificate file 'resources/app/aws_iot/client.cer' with the one got from AWS\n\n" );
        return;
    }
    security.cert_len = size_out;

    //Private key save to security.key, also save the length of Private key to variable security.key_len
    resource_get_readonly_buffer( &resources_apps_DIR_aws_iot_DIR_privkey_cer, 0, MQTT_MAX_RESOURCE_SIZE, &size_out, (const void **) &security.key );
    if(size_out < 64)
    {
        debugMsg( "\nNot a valid Private Key! Please replace the dummy private key file 'resources/app/aws_iot/privkey.cer' with the one got from AWS\n\n" ) ;
        return;
    }
    security.key_len = size_out;

    /* Disable roaming to other access points */
    wiced_wifi_set_roam_trigger( -99 ); /* -99dBm ie. extremely low signal level */

    /*Connect to Wi-Fi*/
    while(wiced_network_up(WICED_STA_INTERFACE,WICED_USE_EXTERNAL_DHCP_SERVER,NULL) != WICED_SUCCESS )
    {
        debugMsg("Connect to Wi-Fi is failed\r\n");
    }
    debugMsg("Connect to Wi-Fi is succeeded\r\n");

    /* 
     * Allocate memory for MQTT object
     * mqtt_object is used for any action relative to MQTT connection like connect, publish
     */
    mqtt_object = (wiced_mqtt_object_t) malloc( WICED_MQTT_OBJECT_MEMORY_SIZE_REQUIREMENT );
    if ( mqtt_object == NULL )
    {
        debugMsg("Don't have memory to allocate for mqtt object...\r\n");
        return;
    }

    debugMsg( "Resolving IP address of MQTT broker...\r\n" );
    // Use this API to get the IP address of my IoT endpoint
    /*
     * MQTT_BROKER_ADDRESS : The DNS name we define at the begining. It is define as AWS IoT endpoint.
     * broker_address : We will get corresponding IP address by using this API
     * 10000 : Timeout value
     * WICED_STA_INTERFACE : As our device is as station so choose STA interface (STA is stand for station)
     */
    ret = wiced_hostname_lookup( MQTT_BROKER_ADDRESS, &broker_address, 10000, WICED_STA_INTERFACE );
    if ( ret == WICED_ERROR || broker_address.ip.v4 == 0 )
    {
        debugMsg(("Error in resolving DNS\r\n"));
        return;
    }

    wiced_gpio_output_low( WICED_LED2 );
    /* If we want to use mqtt, semaphore we need to do the initialzation */
    wiced_rtos_init_semaphore( &wake_semaphore );
    wiced_mqtt_init( mqtt_object );
    wiced_rtos_init_semaphore( &msg_semaphore );
    wiced_rtos_init_semaphore( &MQTTend_semaphore );
    sprintf(IotShadowTopic,(char *)WICED_TOPIC,IotThing);
    sprintf(IotShadowDocumentTopic,(char *)Shadow_Document_TOPIC,IotThing);
    wiced_rtos_init_timer(&timer1_handle, 1, timer1Function, NULL);
    wiced_rtos_start_timer(&timer1_handle);

    /*
     * Why do I use a while(1) here?
     * Because if connection of MQTT is failed, then continue to here.
     * Try to connect again.
     */
    while(1)
    {
        connection_retries = 0;
        is_connected = WICED_FALSE;
        retries = 0;

        debugMsg("[MQTT] Opening connection...");
        do
        {
            /*
             * create the connection to AWS IoT broker
             * WICED_STA_INTERFACE : Because our device is used to be Wi-Fi station
             * mqtt_connection_event_cb : A call back function to handle connection events
             */
            ret = mqtt_conn_open( mqtt_object, &broker_address, WICED_STA_INTERFACE, mqtt_connection_event_cb, &security );
            wiced_rtos_delay_milliseconds( 100 );
            connection_retries++ ;
        } while ( ( ret != WICED_SUCCESS ) && ( connection_retries < WICED_MQTT_CONNECTION_NUMBER_OF_RETRIES ) );

        if ( ret != WICED_SUCCESS )
        {
            WPRINT_APP_INFO(("Failed\r\n"));
            wiced_rtos_delay_milliseconds( MQTT_DELAY_IN_MILLISECONDS * 5 );
            continue;
        }
        debugMsg("Success\r\n");
        is_connected = WICED_TRUE;

        debugMsg(("[MQTT] Subscribing to shadow Topic..."));
        do
        {
            ret = mqtt_app_subscribe( mqtt_object, IotShadowDocumentTopic, WICED_MQTT_QOS_DELIVER_AT_MOST_ONCE );
            retries++ ;
        } while ( ( ret != WICED_SUCCESS ) && ( retries < MQTT_SUBSCRIBE_RETRY_COUNT ) );
        if ( ret != WICED_SUCCESS )
        {
            debugMsg((" Failed\r\n"));
        }
        else
        {
            debugMsg(("Success...\r\n"));
            retries = 0;
        }

        debugMsg(("[MQTT] Subscribing to control Topic..."));
        do
        {
            ret = mqtt_app_subscribe( mqtt_object, IotControlTopic, WICED_MQTT_QOS_DELIVER_AT_MOST_ONCE );
            retries++ ;
        } while ( ( ret != WICED_SUCCESS ) && ( retries < MQTT_SUBSCRIBE_RETRY_COUNT ) );
        if ( ret != WICED_SUCCESS )
        {
            debugMsg((" Failed\r\n"));
        }
        else
        {
            debugMsg(("Success...\r\n"));
            retries = 0;
        }

        //Start to run Publishing function
        wiced_rtos_create_thread(&PublishMessageHandle,9,"PublishMessageThread",PublishMessage,1024,NULL);
        //Start to run RX function
        wiced_rtos_create_thread(&receiveUartHandle,10,"UartRxThread",receiveUART,1024,NULL);
        //Start to run RX function
        wiced_rtos_create_thread(&PeripheralHandle,11,"PeripheralFunctionThread",peripheralFunction,1024,NULL);

        sprintf(ShadowUpdateStr, (char *)"{ \"state\": {\"reported\": { \"status\": \"%s\" , \"doorLock\":\"%s\"} } }", status, doorLock);
        debugMsg("status: ");
        debugMsg(status);
        debugMsg("\r\ndoorLock: ");
        debugMsg(doorLock);
        debugMsg("\r\n");
        root = cJSON_Parse(ShadowUpdateStr);
        out=cJSON_Print(root);
        //Send what data? Copy to msg (We will publish msg to Topic)
        strcpy(msg,out);
        //wiced_uart_transmit_bytes(WICED_UART_1 , msg , strlen(msg));
        wiced_rtos_set_semaphore( &wake_semaphore );
        //When MQTT disconnected (before MQTT disconnect, this thread will suspend)
        wiced_rtos_get_semaphore( &MQTTend_semaphore,WICED_WAIT_FOREVER );

        // When connect or publish failed, close the connection
        wiced_rtos_delete_thread(&receiveUartHandle);
        debugMsg(("[MQTT] Closing connection...\r\n"));
        mqtt_conn_close( mqtt_object );
    }
    wiced_rtos_deinit_semaphore( &msg_semaphore );
    debugMsg(("[MQTT] Deinit connection...\r\n"));
    ret = wiced_mqtt_deinit( mqtt_object );
    wiced_rtos_deinit_semaphore( &wake_semaphore );
    wiced_rtos_deinit_semaphore( &MQTTend_semaphore );
    free( mqtt_object );
    cJSON_Delete(json);
    cJSON_Delete(root);
    mqtt_object = NULL;

    //return;
}
