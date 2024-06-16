#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "ssd1306.h" 

#define WIFI_CONNECTED_BIT BIT0
//change the API key to your own 
//if you don't have one, register on OpenWeatherMap and you can get it for free!
#define API_KEY "41a7528f55243a115e6dcdd9dc8cd093"
//change the latitude and longitude to your desired location
#define LATITUDE "40.44"
#define LONGITUDE "-79.99"
#define WEATHER_API_URL "http://api.openweathermap.org/data/2.5/weather?lat=" LATITUDE "&lon=" LONGITUDE "&appid=" API_KEY "& units=metric"
#define SSID "WhiteSky-Centre"
#define PASSWORD "w224ap2k"

static const char *TAG_WIFI = "wifi station";  // TAG for Wi-Fi
static const char *TAG_WEATHER = "weather_fetch";  // TAG for weather fetch
static const char *TAG_OLED = "oled_display";  // TAG for OLED display

EventGroupHandle_t s_wifi_event_group;  
char *response_data = NULL;
size_t response_len = 0;
bool all_chunks_received = false;

// Event handler for Wi-Fi and IP events
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG_WIFI, "Disconnected. Reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        char ip_str[IP4ADDR_STRLEN_MAX];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, IP4ADDR_STRLEN_MAX);
        ESP_LOGI(TAG_WIFI, "Got IP: %s", ip_str);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialize Wi-Fi with the given SSID and password
void wifi_init_sta(const char *ssid, const char *password) {
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
        },
    };

    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    //log debug message based on the connection status
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_WIFI, "Connected to AP");
    } else {
        ESP_LOGI(TAG_WIFI, "Failed to connect to AP");
    }
}

// Event handler for HTTP events
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            response_data = realloc(response_data, response_len + evt->data_len + 1);
            memcpy(response_data + response_len, evt->data, evt->data_len);
            response_len += evt->data_len;
            response_data[response_len] = '\0';
            break;
        case HTTP_EVENT_ON_FINISH:
            all_chunks_received = true;
            ESP_LOGI(TAG_WEATHER, "Received data: %s", response_data);
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Parse JSON data to get weather data
void get_weather_data(const char *json_string, double *temp_min, double *temp_max, char *city_name, char *weather_description)
{
    cJSON *root = cJSON_Parse(json_string);
    if (!root) {
        ESP_LOGE(TAG_WEATHER, "JSON parse error");
        return;
    }

    // Get city name
    cJSON *name = cJSON_GetObjectItem(root, "name");
    if (name && cJSON_IsString(name)) {
        strncpy(city_name, name->valuestring, 50);
    } else {
        ESP_LOGE(TAG_WEATHER, "Failed to get city name");
    }

    // Get weather description
    cJSON *weather_array = cJSON_GetObjectItem(root, "weather");
    if (weather_array) {
        cJSON *weather = cJSON_GetArrayItem(weather_array, 0);
        if (weather) {
            cJSON *description = cJSON_GetObjectItem(weather, "description");
            if (description && cJSON_IsString(description)) {
                strncpy(weather_description, description->valuestring, 50);
            } else {
                ESP_LOGE(TAG_WEATHER, "Failed to get weather description");
            }
        } else {
            ESP_LOGE(TAG_WEATHER, "Failed to get weather data");
        }
    } else {
        ESP_LOGE(TAG_WEATHER, "Failed to get weather array");
    }

    // Get temp min and max
    cJSON *main = cJSON_GetObjectItem(root, "main");
    if (main) {
        cJSON *temp_min_item = cJSON_GetObjectItem(main, "temp_min");
        cJSON *temp_max_item = cJSON_GetObjectItem(main, "temp_max");

        if (temp_min_item && cJSON_IsNumber(temp_min_item)) {
            *temp_min = temp_min_item->valuedouble;
        } else {
            ESP_LOGE(TAG_WEATHER, "Failed to get temp_min");
        }
        if (temp_max_item && cJSON_IsNumber(temp_max_item)) {
            *temp_max = temp_max_item->valuedouble;
        } else {
            ESP_LOGE(TAG_WEATHER, "Failed to get temp_max");
        }
    } else {
        ESP_LOGE(TAG_WEATHER, "Failed to get 'main' from JSON");
    }

    cJSON_Delete(root);
}

void fetch_weather_data(void *pvParameters)
{
    esp_http_client_config_t config = {
        .url = WEATHER_API_URL,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    // Initialize OLED
    SSD1306_t dev;

#if CONFIG_I2C_INTERFACE
	ESP_LOGI(TAG_OLED, "INTERFACE is i2c");
	ESP_LOGI(TAG_OLED, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(TAG_OLED, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(TAG_OLED, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_I2C_INTERFACE

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG_WEATHER, "HTTP status code: %d", status_code);

        if (status_code == 200) {
            ESP_LOGI(TAG_WEATHER, "HTTP GET request successful");

            double temp_min = 0.0;
            double temp_max = 0.0;
            char city_name[50] = {0};
            char weather_description[50] = {0};

            get_weather_data(response_data, &temp_min, &temp_max, city_name, weather_description);
            ESP_LOGI(TAG_WEATHER, "City: %s, Weather: %s, Min Temp: %.1fC, Max Temp: %.1fC", city_name, weather_description, temp_min, temp_max);
            free(response_data);

            ssd1306_init(&dev, 128, 64);
            char temp_min_str[30];
            char temp_max_str[30];
            sprintf(temp_min_str, "Min Temp: %.1fC", temp_min);
            sprintf(temp_max_str, "Max Temp: %.1fC", temp_max);
            ssd1306_clear_screen(&dev, false);
            ssd1306_display_text(&dev, 0, city_name, strlen(city_name), false);
            ssd1306_display_text(&dev, 2, weather_description, strlen(weather_description), false);
            ssd1306_display_text(&dev, 4, temp_min_str, strlen(temp_min_str), false);
            ssd1306_display_text(&dev, 6, temp_max_str, strlen(temp_max_str), false);

        } else {
            ESP_LOGE(TAG_WEATHER, "HTTP GET request failed with status code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG_WEATHER, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}


void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //wifi initialization
    wifi_init_sta(SSID, PASSWORD);

    // Wait for Wi-Fi connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_WEATHER, "Connected to Wi-Fi, starting weather data fetch");
        xTaskCreate(&fetch_weather_data, "fetch_weather_data", 8192, NULL, 5, NULL);
    } else {
        ESP_LOGE(TAG_WEATHER, "Failed to connect to Wi-Fi");
    }
}
