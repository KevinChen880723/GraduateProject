#include "wiced.h"
#include "stdio.h"
#include "string.h"
#include "mqtt_api.h"
#include "resources.h"

/************************* User global variables *******************************************/
char receiveData[20]="";
static wiced_thread_t receiveUartHandle;
char* msg = "Hello World!!";

/************************* User defines *******************************************/
#define RX_BUFFER_SIZE 20
#define RxDataSize 5

/************************* (Copy from publisher.c) *******************************************/
#define MQTT_BROKER_ADDRESS                 "a21yyexai8eunn-ats.iot.us-east-1.amazonaws.com"
#define MQTT_BROKER_PEER_COMMON_NAME        "*.iot.us-east-1.amazonaws.com"
#define WICED_TOPIC                         "Kevin_Topic"
#define CLIENT_ID                           "wiced_publisher_aws"
#define MQTT_REQUEST_TIMEOUT                (5000)
#define MQTT_DELAY_IN_MILLISECONDS          (1000)
#define MQTT_MAX_RESOURCE_SIZE              (0x7fffffff)
#define MQTT_PUBLISH_RETRY_COUNT            (3)
#define MSG_ON                              "LIGHT ON"
#define MSG_OFF                             "LIGHT OFF"
/********************************************************************/
/******************************************************
 *               Variable Definitions (Copy from publisher.c)
 ******************************************************/
static wiced_ip_address_t                   broker_address;
static wiced_mqtt_event_type_t              expected_event;
static wiced_semaphore_t                    msg_semaphore;
static wiced_semaphore_t                    wake_semaphore;
static wiced_mqtt_security_t                security;
static wiced_bool_t                         is_connected = WICED_FALSE;
static uint8_t                              pub_in_progress = 0;

/******************************************************
 *               Static Function Definitions
 ******************************************************/
/*
 * A blocking call to an expected event.
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
 */
static wiced_result_t mqtt_connection_event_cb( wiced_mqtt_object_t mqtt_object, wiced_mqtt_event_info_t *event )
{
    switch ( event->type )
    {
        case WICED_MQTT_EVENT_TYPE_DISCONNECTED:
        {
            is_connected = WICED_FALSE; //Use this variable to check the status of MQTT connection
        }
            break;
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
    conninfo.port_number = 0;
    conninfo.mqtt_version = WICED_MQTT_PROTOCOL_VER4;
    conninfo.clean_session = 1;
    conninfo.client_id = (uint8_t*) CLIENT_ID;
    conninfo.keep_alive = 5;
    conninfo.password = NULL;
    conninfo.username = NULL;
    conninfo.peer_cn = (uint8_t*) MQTT_BROKER_PEER_COMMON_NAME;
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
 * Publish (send) message to WICED_TOPIC and wait for 5 seconds to receive a PUBCOMP (as it is QoS=2).
 */
static wiced_result_t mqtt_app_publish( wiced_mqtt_object_t mqtt_obj, uint8_t qos, uint8_t *topic, uint8_t *data, uint32_t data_len )
{
    wiced_mqtt_msgid_t pktid;

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

void receiveUART(wiced_thread_arg_t arg)
{
    uint32_t dataSize = RxDataSize;

    while(wiced_uart_receive_bytes(WICED_UART_3,receiveData,&dataSize,WICED_WAIT_FOREVER ) == WICED_SUCCESS)
    {

            if(strcmp(receiveData , "12345") > 0)
            {wiced_uart_transmit_bytes(WICED_UART_3 , receiveData , strlen(receiveData));
                strcpy(msg,receiveData);

                if(pub_in_progress == 0)
                    pub_in_progress = 1;
                wiced_uart_transmit_bytes(WICED_UART_1 , msg , strlen(msg));
                wiced_rtos_set_semaphore( &wake_semaphore );
            }
    }

}

void debug(char debugMessage[50])
{
    wiced_uart_transmit_bytes(WICED_UART_1 , debugMessage , strlen(debugMessage));
}

/*
 *  main function
 */
void application_start()
{
    wiced_result_t result = WICED_ERROR ;
    char debugMessage[50];
    /******************** Copy from publisher.c ***************/
    wiced_mqtt_object_t   mqtt_object;
    wiced_result_t        ret = WICED_SUCCESS;
    uint32_t              size_out = 0;
    int                   connection_retries = 0;
    int                   retries = 0;
    /**********************************************************/

    wiced_init();

    //Costomize UART
    wiced_uart_config_t uart_config=
    {
            .baud_rate = 115200,
            .data_width = DATA_WIDTH_8BIT,
            .parity = STOP_BITS_1,
            .flow_control = FLOW_CONTROL_DISABLED
    };

    //Create a buffer for UART rx data, then initial UART3, which is routed to P6:38(RX)¡BP6:39(TX)
    wiced_ring_buffer_t rx_buffer;
    uint8_t rx_data[RX_BUFFER_SIZE];
    ring_buffer_init(&rx_buffer, rx_data, RX_BUFFER_SIZE );
    wiced_uart_init(WICED_UART_3,&uart_config,&rx_buffer);

    //Initial UART1, this is use for debugging
    wiced_uart_init(WICED_UART_1,&uart_config,NULL);

    /* Get AWS root certificate, client certificate and private key respectively */
    resource_get_readonly_buffer( &resources_apps_DIR_aws_iot_DIR_rootca_cer, 0, MQTT_MAX_RESOURCE_SIZE, &size_out, (const void **) &security.ca_cert );
    security.ca_cert_len = size_out;

    resource_get_readonly_buffer( &resources_apps_DIR_aws_iot_DIR_client_cer, 0, MQTT_MAX_RESOURCE_SIZE, &size_out, (const void **) &security.cert );
    if(size_out < 64)
    {
        debug( "\nNot a valid Certificate! Please replace the dummy certificate file 'resources/app/aws_iot/client.cer' with the one got from AWS\n\n" );
        return;
    }
    security.cert_len = size_out;

    resource_get_readonly_buffer( &resources_apps_DIR_aws_iot_DIR_privkey_cer, 0, MQTT_MAX_RESOURCE_SIZE, &size_out, (const void **) &security.key );
    if(size_out < 64)
    {
        debug( "\nNot a valid Private Key! Please replace the dummy private key file 'resources/app/aws_iot/privkey.cer' with the one got from AWS\n\n" ) ;
        return;
    }
    security.key_len = size_out;

    /* Disable roaming to other access points */
    wiced_wifi_set_roam_trigger( -99 ); /* -99dBm ie. extremely low signal level */

    /*Connect to Wi-Fi*/
    while(wiced_network_up(WICED_STA_INTERFACE,WICED_USE_EXTERNAL_DHCP_SERVER,NULL) != WICED_SUCCESS )
    {
        debug("Connect to Wi-Fi is failed\r\n");
    }
    debug("Connect to Wi-Fi is succeeded\r\n");

    /* Allocate memory for MQTT object*/
    mqtt_object = (wiced_mqtt_object_t) malloc( WICED_MQTT_OBJECT_MEMORY_SIZE_REQUIREMENT );
    if ( mqtt_object == NULL )
    {
        debug("Don't have memory to allocate for mqtt object...\r\n");
        return;
    }

    debug( "Resolving IP address of MQTT broker...\r\n" );
    //Use this API to get the IP address of my IoT endpoint
    ret = wiced_hostname_lookup( MQTT_BROKER_ADDRESS, &broker_address, 10000, WICED_STA_INTERFACE );
    if ( ret == WICED_ERROR || broker_address.ip.v4 == 0 )
    {
        debug(("Error in resolving DNS\r\n"));
        return;
    }

    wiced_gpio_output_low( WICED_LED2 );

    wiced_rtos_init_semaphore( &wake_semaphore );
    wiced_mqtt_init( mqtt_object );
    wiced_rtos_init_semaphore( &msg_semaphore );

    while(1)
    {
        connection_retries = 0;
        is_connected = WICED_FALSE;
        retries = 0;

        debug("[MQTT] Opening connection...");
        do
        {
            //mqtt_connection_event_cb is the call back function to handle connection events
            //create the connection to AWS iot broker
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
        debug("Success\r\n");
        is_connected = WICED_TRUE;

        //Start to run RX function
        wiced_rtos_create_thread(&receiveUartHandle,10,"UartRxThread",receiveUART,1024,NULL);

        while(1)
        {
            retries = 0;

            //Wait for sending data(Maybe when receive correct data)
            wiced_rtos_get_semaphore( &wake_semaphore,WICED_WAIT_FOREVER );
            if ( is_connected == WICED_FALSE )
            {
                break;
            }

            //Publish the data
            debug(("[MQTT] Publishing..."));
            do
            {
                ret = mqtt_app_publish( mqtt_object, WICED_MQTT_QOS_DELIVER_AT_LEAST_ONCE, (uint8_t*) WICED_TOPIC, (uint8_t*) msg, strlen( msg ) );
                retries++ ;
            } while ( ( ret != WICED_SUCCESS ) && ( retries < MQTT_PUBLISH_RETRY_COUNT ) );
            if ( ret != WICED_SUCCESS )
            {
                debug((" Failed\r\n"));
                break;
            }
            else
            {
                debug((" Success\r\n"));
            }
            pub_in_progress = 0;

            wiced_rtos_delay_milliseconds( 100 );

            wiced_gpio_output_low( WICED_LED2 );
            wiced_rtos_delay_milliseconds( 100 );
            wiced_gpio_output_high( WICED_LED2 );
            wiced_rtos_delay_milliseconds( 100 );
        }
        wiced_rtos_delete_thread(&receiveUartHandle);
        debug(("[MQTT] Closing connection...\r\n"));
        mqtt_conn_close( mqtt_object );
    }
    wiced_rtos_deinit_semaphore( &msg_semaphore );
    debug(("[MQTT] Deinit connection...\r\n"));
    ret = wiced_mqtt_deinit( mqtt_object );
    wiced_rtos_deinit_semaphore( &wake_semaphore );
    free( mqtt_object );
    mqtt_object = NULL;

    return;
}
