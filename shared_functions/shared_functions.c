
/*
 shared functions used all accessorys
 
 */


#include <shared_functions.h>

#define CHECK_INTERVAL 30000
#define WIFI_ISSUE                  (blinking_params_t){10,0}


bool accessory_paired = false;


bool wifi_connected;


void wifi_check_interval_set (homekit_value_t value){
    
    wifi_check_interval.value.int_value = value.int_value;
    save_characteristic_to_flash (&wifi_check_interval, wifi_check_interval.value);
    printf ("%s Wifi Check Interval: %d\n", __func__, wifi_check_interval.value.int_value);
    
}

void checkWifiTask(void *pvParameters)
{
    uint8_t status ;
    
    wifi_connected = false;
    
    while (1)
    {
        if (wifi_check_interval.value.int_value != 0) {
            /* only check if no zero */
            printf("\n%s WiFi: check interval %d\n", __func__, wifi_check_interval.value.int_value);
            status = sdk_wifi_station_get_connect_status();
            switch (status)
            {
                case STATION_WRONG_PASSWORD:
                    printf("\n%s WiFi: wrong password\n\r", __func__);
                    led_code (status_led_gpio, WIFI_ISSUE);
                    wifi_connected = false;
                    break;
                case STATION_NO_AP_FOUND:
                    printf("\n%s WiFi: AP not found\n\r", __func__);
                    led_code (status_led_gpio, WIFI_ISSUE);
                    wifi_connected = false;
                    break;
                case STATION_CONNECT_FAIL:
                    printf("\n%s WiFi: connection failed\n\r", __func__);
                    led_code (status_led_gpio, WIFI_ISSUE);
                    wifi_connected = false;
                    break;
                case STATION_GOT_IP:
                    printf("\n%s WiFi: connection ok\n\r", __func__);
                    wifi_connected = true;
                    led_code (status_led_gpio,  WIFI_CONNECTED);
                    break;
                default:
                    printf("\n%s WiFi: status = %d\n\r", __func__, status);
                    led_code (status_led_gpio, WIFI_ISSUE);
                    break;
                    
            }
        }
        else {
            printf("\n%s WiFi: no check performed\n", __func__);

        }
        
        vTaskDelay((1000*wifi_check_interval.value.int_value) / portTICK_PERIOD_MS);
    }
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
    xTaskCreate(identify_task, "Identify", 128, NULL, tskIDLE_PRIORITY, NULL);
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
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, tskIDLE_PRIORITY, NULL);
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
            printf("on_homekit_event: Server initialised\n");
            if (homekit_is_paired()){
                /* if server has started and we already have a pairing then initialise*/
                accessory_paired = true;
                printf("on_homekit_event: Acessory is paired on initialisation\n");
                accessory_init ();
                led_code( status_led_gpio, WIFI_CONNECTED);
            }
            else
            {
                printf("on_homekit_event: Acessory is NOT paired on initialisation\n");
                accessory_init_not_paired ();

            }
            break;
        case HOMEKIT_EVENT_CLIENT_CONNECTED:
            printf("on_homekit_event: Client connected\n");
            break;
        case HOMEKIT_EVENT_CLIENT_VERIFIED:
            printf("on_homekit_event: Client verified\n");
            /* we weren't paired on started up but we now are */
            if (!accessory_paired ){
                accessory_paired = true;
                printf("on_homekit_event: Acessory is paired on after client validaiton\n");
                accessory_init();
                led_code( status_led_gpio, WIFI_CONNECTED);
            }
            break;
        case HOMEKIT_EVENT_CLIENT_DISCONNECTED:
            printf("on_homekit_event: Client disconnected\n");
            break;
        case HOMEKIT_EVENT_PAIRING_ADDED:
            printf("on_homekit_event: Pairing added\n");
            break;
        case HOMEKIT_EVENT_PAIRING_REMOVED:
            printf("on_homekit_event: Pairing removed\n");
            if (!homekit_is_paired()){
            /* if we have no more pairings then restart */
                printf("on_homekit_event: no more pairings so restart\n");
                sdk_system_restart();
            }
            break;
        default:
            printf("on_homekit_event: Default event %d ", event);
    }
    
}

void on_wifi_ready ( void) {
    
    printf("on_wifi_ready\n");
    reset_information = sdk_system_get_rst_info();
    switch (reset_information->reason){
        case DEFAULT_RST:
        case WDT_RST:
        case EXCEPTION_RST:
        case SOFT_RST:
            printf ("%s Reset Reason: %d\nException Cause: %d\nEPC 1: %d\nEPC 2: %d\nEPC 3: %d\nExv virtul address: %d\nDEPC: %d\nReturn Address:%d\n", __func__, reset_information->reason, reset_information->exccause, reset_information->epc1, reset_information->epc2, reset_information->epc3, reset_information->excvaddr, reset_information->depc,reset_information->rtn_addr);
            break;
        default:
            printf ("%s Unknown Reset Reason: %d\nException Cause: %d\nEPC 1: %d\nEPC 2: %d\nEPC 3: %d\nExv virtul address: %d\nDEPC: %d\nReturn Address:%d\n", __func__, reset_information->reason, reset_information->exccause, reset_information->epc1, reset_information->epc2, reset_information->epc3, reset_information->excvaddr, reset_information->depc,reset_information->rtn_addr);
    }
    homekit_server_init(&config);
    
}


void standard_init (homekit_characteristic_t *name, homekit_characteristic_t *manufacturer, homekit_characteristic_t *model, homekit_characteristic_t *serial, homekit_characteristic_t *revision){
    
    printf ("Standard init\n");
    uart_set_baud(0, 115200);
    
    udplog_init(3);
    get_sysparam_info();
    
    load_characteristic_from_flash (&wifi_check_interval);
    
    create_accessory_name(name->value.string_value, model->value.string_value, name, serial);
    
    int c_hash=ota_read_sysparam(&manufacturer->value.string_value,&serial->value.string_value,
                                 &model->value.string_value,&revision->value.string_value);
    if (c_hash==0) c_hash=1;
    config.accessories[0]->config_number=c_hash;
    
    xTaskCreate (checkWifiTask, "Check WiFi Task", 256, NULL, tskIDLE_PRIORITY, NULL);

        
    
}
