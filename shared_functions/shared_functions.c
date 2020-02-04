
/*
 shared functions used all accessorys
 
 */


#include <shared_functions.h>

#define CHECK_INTERVAL 30000
#define WIFI_CHECK_INTERVAL_SAVE_DELAY 30000
#define TASK_STATS_INTERVAL 50000
#define WIFI_ISSUE                  (blinking_params_t){10,0}


bool accessory_paired = false;
TaskHandle_t task_stats_task_handle = NULL;
TaskHandle_t wifi_check_interval_task_handle = NULL;
ETSTimer save_timer;


bool wifi_connected;


void task_stats_task ( void *args)
{
    TaskStatus_t *pxTaskStatusArray;
    UBaseType_t uxArraySize, x;
    uint32_t ulTotalRunTime;
    
    printf ("%s", __func__);
    
    while (1) {
        /* Take a snapshot of the number of tasks in case it changes while this
         function is executing. */
        uxArraySize = uxTaskGetNumberOfTasks();
        
        printf (", uxTaskGetNumberOfTasks %ld", uxArraySize);
        /* Allocate a TaskStatus_t structure for each task.  An array could be
         allocated statically at compile time. */
        pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );
        
        printf (", pvPortMalloc");
        
        if( pxTaskStatusArray != NULL )
        {
            /* Generate raw status information about each task. */
            uxArraySize = uxTaskGetSystemState( pxTaskStatusArray,
                                               uxArraySize,
                                               &ulTotalRunTime );
            
            printf (", uxTaskGetSystemState, ulTotalRunTime %d, array size %ld\n", ulTotalRunTime, uxArraySize);
            
            /* Avoid divide by zero errors. */
            /*        if( ulTotalRunTime > 0 )
             {*/
            /* For each populated position in the pxTaskStatusArray array,
             format the raw data as human readable ASCII data. */
            for( x = 0; x < uxArraySize; x++ )
            {

                printf ( "Name:%-20s,  Runtime Counter:%-3d, Current State:%-3d, Current Priority:%-5ld, Base Priority:%-5ld, High Water Mark (bytes) %-5d\n",
                        pxTaskStatusArray[ x ].pcTaskName,
                        pxTaskStatusArray[ x ].ulRunTimeCounter,
                        pxTaskStatusArray[ x ].eCurrentState,
                        pxTaskStatusArray[ x ].uxCurrentPriority ,
                        pxTaskStatusArray[ x ].uxBasePriority,
                        pxTaskStatusArray[x].usStackHighWaterMark);
                
            }
            /*        }*/
            
            /* The array is no longer needed, free the memory it consumes. */
            vPortFree( pxTaskStatusArray );
            printf ("%s, vPortFree\n", __func__);
        }
        vTaskDelay(TASK_STATS_INTERVAL/ portTICK_PERIOD_MS);
    }
    
}

void task_stats_set (homekit_value_t value) {
    
    printf("Task Stats\n");
    if (value.bool_value)
        {
            if (task_stats_task_handle == NULL){
                xTaskCreate(task_stats_task, "task_stats_task", 512 , NULL, tskIDLE_PRIORITY+1, &task_stats_task_handle);
            } else {
                printf ("%s task_Status_set TRUE, but task pointer not NULL\n", __func__);
            }
        }
    else
    {
        if (task_stats_task_handle != NULL){
            vTaskDelete (task_stats_task_handle);
            task_stats_task_handle = NULL;
        } else {
            printf ("%s task_Status_set FALSE, but task pointer is NULL\n", __func__);
        }
    }
}


void checkWifiTask(void *pvParameters)
{
    uint8_t status ;
    
    wifi_connected = false;
    
    while (1)
    {
        if (wifi_check_interval.value.int_value != 0) {
            /* only check if no zero */
            printf("\n%s WiFi: check interval %d, Status: ", __func__, wifi_check_interval.value.int_value);
            status = sdk_wifi_station_get_connect_status();
            switch (status)
            {
                case STATION_WRONG_PASSWORD:
                    printf("wrong password: ");
                    led_code (status_led_gpio, WIFI_ISSUE);
                    wifi_connected = false;
                    break;
                case STATION_NO_AP_FOUND:
                    printf("AP not found: ");
                    led_code (status_led_gpio, WIFI_ISSUE);
                    wifi_connected = false;
                    break;
                case STATION_CONNECT_FAIL:
                    printf("connection failed: ");
                    led_code (status_led_gpio, WIFI_ISSUE);
                    wifi_connected = false;
                    break;
                case STATION_GOT_IP:
                    printf("connection ok: ");
                    wifi_connected = true;
                    led_code (status_led_gpio,  WIFI_CONNECTED);
                    break;
                default:
                    printf(" default = %d: ", status);
                    led_code (status_led_gpio, WIFI_ISSUE);
                    break;
                    
            }
        }
        else {
            printf("\n%s : no check performed ", __func__);
            
        }
        printf ("Free Heap=%d, Free Stack=%lu\n", xPortGetFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL)/4);
        /*uxTaskGetStackHighWaterMark returns a number in bytes, stack is created in words, so device by 4 to get nujber of words left on stack */
        
        
        if ( wifi_check_interval.value.int_value==0) {
            vTaskDelay((10000) / portTICK_PERIOD_MS);
        } else {
            vTaskDelay((1000*wifi_check_interval.value.int_value) / portTICK_PERIOD_MS);
        }
    }
}



void wifi_check_interval_set (homekit_value_t value){
    
    wifi_check_interval.value.int_value = value.int_value;
    printf ("%s Wifi Check Interval: %d\n", __func__, wifi_check_interval.value.int_value);
    if (wifi_check_interval.value.int_value==0){
        /* check interval is 0 so make sure the task is not running */
        if (wifi_check_interval_task_handle != NULL)
        {
            printf ("%s Stopping Task\n", __func__);
            vTaskDelete(wifi_check_interval_task_handle);
            wifi_check_interval_task_handle = NULL;
        }
    } else {
        /* check interval > 0 so make sure the task is running */
        if (wifi_check_interval_task_handle == NULL)
        {
            printf ("%s Starting Task\n", __func__);
            xTaskCreate (checkWifiTask, "Check WiFi Task", 256, NULL, tskIDLE_PRIORITY+1, &wifi_check_interval_task_handle);
        }
    }
    sdk_os_timer_arm (&save_timer, WIFI_CHECK_INTERVAL_SAVE_DELAY, 0);
}


void wifi_reset_set(homekit_value_t value){
    printf("Resetting Wifi Config\n");
    wifi_config_reset();
    printf("Restarting\n");
    sdk_system_restart();
}

void identify_task(void *_args) {
    // We identify the Device by Flashing it's LED.
    led_code( status_led_gpio, IDENTIFY_ACCESSORY);
    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    printf("Identify\n");
    xTaskCreate(identify_task, "Identify", 128, NULL, tskIDLE_PRIORITY+1, NULL);
}


void relay_write(bool on, int gpio) {
    gpio_write(gpio, on ? 1 : 0);
}


void led_write(bool on, int gpio) {
    gpio_write(gpio, on ? 0 : 1);
}


void reset_configuration_task() {
    //Flash the LED first before we start the reset
    led_code (status_led_gpio, WIFI_CONFIG_RESET);
    printf("Resetting Wifi Config\n");
    
    wifi_config_reset();
    led_code( status_led_gpio, WIFI_CONFIG_RESET);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    printf("Resetting HomeKit Config\n");
    
    homekit_server_reset();
    led_code( status_led_gpio, EXTRA_CONFIG_RESET);
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    printf("Restarting\n");
    led_code( status_led_gpio, RESTART_DEVICE);
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}


void reset_configuration() {
    printf("Resetting Device configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, tskIDLE_PRIORITY+1, NULL);
}


void reset_button_callback(uint8_t gpio, void* args, uint8_t param) {
    printf("Reset Button event long press on GPIO : %d\n", gpio);
    reset_configuration();
    
}


void create_accessory_name(const char* name, const char* model, homekit_characteristic_t *accessory_name, homekit_characteristic_t *accessory_serial){
    
    printf ("Create accessory name\n|");
    
    int serialLength = snprintf(NULL, 0, "%d", sdk_system_get_chip_id());
    
    char *serialNumberValue = malloc(serialLength + 1);
    
    snprintf(serialNumberValue, serialLength + 1, "%d", sdk_system_get_chip_id());
    
    int name_len = snprintf(NULL, 0, "%s-%s-%s",
                            name,
                            model,
                            serialNumberValue);
    
    if (name_len > 63) {
        name_len = 63;
    }
    
    char *name_value = malloc(name_len + 1);
    
    snprintf(name_value, name_len + 1, "%s-%s-%s",
             name, model, serialNumberValue);
    
    
    accessory_name->value = HOMEKIT_STRING(name_value);
    accessory_serial->value = HOMEKIT_STRING(serialNumberValue);
}


void on_homekit_event(homekit_event_t event) {
    
    switch (event) {
        case HOMEKIT_EVENT_SERVER_INITIALIZED:
            printf("on_homekit_event: Server initialised, Free Heap=%d\n", xPortGetFreeHeapSize());
            if (homekit_is_paired()){
                /* if server has started and we already have a pairing then initialise*/
                accessory_paired = true;
                printf("on_homekit_event: Acessory is paired on initialisation, Free Heap=%d\n", xPortGetFreeHeapSize());
                accessory_init ();
                led_code( status_led_gpio, WIFI_CONNECTED);
            }
            else
            {
                printf("on_homekit_event: Acessory is NOT paired on initialisation, Free Heap=%d\n", xPortGetFreeHeapSize());
                accessory_init_not_paired ();

            }
            break;
        case HOMEKIT_EVENT_CLIENT_CONNECTED:
            printf("on_homekit_event: Client connected, Free Heap=%d\n", xPortGetFreeHeapSize());

            break;
        case HOMEKIT_EVENT_CLIENT_VERIFIED:
            printf("on_homekit_event: Client verified, Free Heap=%d\n", xPortGetFreeHeapSize());
            /* we weren't paired on started up but we now are */
            if (!accessory_paired ){
                accessory_paired = true;
                printf("on_homekit_event: Acessory is paired on after client validaiton\n");
                accessory_init();
                led_code( status_led_gpio, WIFI_CONNECTED);
            }
            break;
        case HOMEKIT_EVENT_CLIENT_DISCONNECTED:
            printf("on_homekit_event: Client disconnected, Free Heap=%d\n", xPortGetFreeHeapSize());
            break;
        case HOMEKIT_EVENT_PAIRING_ADDED:
            printf("on_homekit_event: Pairing added, Free Heap=%d\n", xPortGetFreeHeapSize());
            break;
        case HOMEKIT_EVENT_PAIRING_REMOVED:
            printf("on_homekit_event: Pairing removed, Free Heap=%d\n", xPortGetFreeHeapSize());
            if (!homekit_is_paired()){
            /* if we have no more pairings then restart */
                printf("on_homekit_event: no more pairings so restart\n");
                sdk_system_restart();
            }
            break;
        default:
            printf("on_homekit_event: Default event %d,  Free Heap=%d\n", event, xPortGetFreeHeapSize());
    }
    
}

void on_wifi_ready ( void) {
    
    printf("on_wifi_ready\n");
    get_sysparam_info();
    reset_information = sdk_system_get_rst_info();
    switch (reset_information->reason){
        case DEFAULT_RST:
            printf ("%s Reset Reason: Default Reset\n", __func__);
            break;
        case WDT_RST:
            printf ("%s Reset Reason: Whatchdog Rest\n", __func__);
            recover_from_reset (reset_information->reason);
            break;
        case EXCEPTION_RST:
            printf ("%s Reset Reason: Excepton Rest\n", __func__);
            recover_from_reset (reset_information->reason);
            break;
        case SOFT_RST:
            printf ("%s Reset Reason: Soft Reeet\n", __func__);
            break;
        default:
            printf ("%s Reset Reason: Unknown\n", __func__);
            
    }
    printf ("%s: Exception Cause: %d\nEPC 1: %d\nEPC 2: %d\nEPC 3: %d\nExv virtul address: %d\nDEPC: %d\nReturn Address:%d\n", __func__,  reset_information->exccause, reset_information->epc1, reset_information->epc2, reset_information->epc3, reset_information->excvaddr, reset_information->depc,reset_information->rtn_addr);
    homekit_server_init(&config);
    
}


void standard_init (homekit_characteristic_t *name, homekit_characteristic_t *manufacturer, homekit_characteristic_t *model, homekit_characteristic_t *serial, homekit_characteristic_t *revision){
    
   
    uart_set_baud(0, 115200);
    udplog_init(tskIDLE_PRIORITY+1);
    printf("%s:SDK version: %s, free heap %u\n", __func__, sdk_system_get_sdk_version(),
           xPortGetFreeHeapSize());

    get_sysparam_info();
    
    load_characteristic_from_flash (&wifi_check_interval);
    
    create_accessory_name(name->value.string_value, model->value.string_value, name, serial);
    
    int c_hash=ota_read_sysparam(&manufacturer->value.string_value,&serial->value.string_value,
                                 &model->value.string_value,&revision->value.string_value);
    if (c_hash==0) c_hash=1;
    config.accessories[0]->config_number=c_hash;
    
    if (wifi_check_interval.value.int_value!=0){
        xTaskCreate (checkWifiTask, "Check WiFi Task", 256, NULL, tskIDLE_PRIORITY+1, &wifi_check_interval_task_handle);
    }
    sdk_os_timer_setfn(&save_timer, save_characteristics, NULL);

}

void standard_init_no_ota (homekit_characteristic_t *name, homekit_characteristic_t *manufacturer, homekit_characteristic_t *model, homekit_characteristic_t *serial, homekit_characteristic_t *revision){
    
   
    uart_set_baud(0, 115200);
    udplog_init(tskIDLE_PRIORITY+1);
    printf("%s:SDK version: %s, free heap %u\n", __func__, sdk_system_get_sdk_version(),
           xPortGetFreeHeapSize());

    get_sysparam_info();
    
    load_characteristic_from_flash (&wifi_check_interval);
    
    create_accessory_name(name->value.string_value, model->value.string_value, name, serial);

    char **serial_str = &serial->value.string_value;
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    *serial_str=malloc(18);
    sprintf(*serial_str,"%02X:%02X:%02X:%02X:%02X:%02X",macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
    
    printf ("%s: name = %s, manufacturer = %s, model = %s, serial = %s, revision = %s\n", __func__, 
        name->value.string_value,
        manufacturer->value.string_value,
        model->value.string_value,
        serial->value.string_value,
        revision->value.string_value);

    if (wifi_check_interval.value.int_value!=0){
        xTaskCreate (checkWifiTask, "Check WiFi Task", 256, NULL, tskIDLE_PRIORITY+1, &wifi_check_interval_task_handle);
    }
    sdk_os_timer_setfn(&save_timer, save_characteristics, NULL);

}


