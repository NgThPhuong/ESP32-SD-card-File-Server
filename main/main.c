/* HTTP File Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_err.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
#include <sys/dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
//#include "esp_spiffs.h"       // from file_serving example
#include "sdmmc_cmd.h"          // now Phuong using sdcard as primary file storage

/* This example demonstrates how to create file server
 * using esp_http_server. This file has only startup code.
 * Look in file_server.c for the implementation */

/* The example uses simple WiFi configuration that you can set via
 * 'make menuconfig'.
 * If you'd rather not, just change the below entries to strings
 * with the config you want -
 * ie. #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD

static const char *TAG = "example";

bool ledState = 0;
/* Wi-Fi event handler */
static esp_err_t event_handler(void *ctx, system_event_t *event)
{

    switch (event->event_id)
    {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        ESP_LOGI(TAG, "Got IP: '%s'",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
        ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    default:
        break;
    }
    return ESP_OK;
}

/* Function to initialize Wi-Fi at station */
static void initialise_wifi(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* Function to initialize SPIFFS */
// static esp_err_t init_spiffs(void)
// {
//     ESP_LOGI(TAG, "Initializing SPIFFS");

//     esp_vfs_spiffs_conf_t conf = {
//         .base_path = "/spiffs",
//         .partition_label = NULL,
//         .max_files = 5, // This decides the maximum number of files that can be created on the storage
//         .format_if_mount_failed = true};

//     esp_err_t ret = esp_vfs_spiffs_register(&conf);
//     if (ret != ESP_OK)
//     {
//         if (ret == ESP_FAIL)
//         {
//             ESP_LOGE(TAG, "Failed to mount or format filesystem");
//         }
//         else if (ret == ESP_ERR_NOT_FOUND)
//         {
//             ESP_LOGE(TAG, "Failed to find SPIFFS partition");
//         }
//         else
//         {
//             ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
//         }
//         return ESP_FAIL;
//     }

//     size_t total = 0, used = 0;
//     ret = esp_spiffs_info(NULL, &total, &used);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
//         return ESP_FAIL;
//     }

//     ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
//     return ESP_OK;
// }

/* Declare the function which starts the file server.
 * Implementation of this function is to be found in
 * file_server.c */
esp_err_t start_file_server(const char *base_path);

// This example can use SDMMC and SPI peripherals to communicate with SD card.
// By default, SDMMC peripheral is used.
// To enable SPI mode, uncomment the following line:

#define USE_SPI_MODE

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.

#ifdef USE_SPI_MODE
// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK 14
#define PIN_NUM_CS 13
#endif //USE_SPI_MODE

//================================================================================================
#define MAX_BUFSIZE 16384
unsigned long IRAM_ATTR millis() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static int rng_func(uint8_t *dest, unsigned size) {

    uint32_t rand=0;
    unsigned pos=0;
    uint8_t step=0;
    uint8_t rand8=0;

    while (pos<size) {
    	if (step>=4) {
    		step=0;
    	}
    	if (step==0) {
    		rand=esp_random();
        	// ESP_LOGI(TAG, "rand 0x%08X",rand);
    	}
    	// faster then 8*step ?
        switch(step) {
    		case 0:
    		   	rand8=rand&0xFF;
    		   	break;
    		case 1:
    		   	rand8=(rand>>8)&0xFF;
    		   	break;
    		case 2:
    		   	rand8=(rand>>16)&0xFF;
    		   	break;
    		case 3:
    		   	rand8=(rand>>24)&0xFF;
    		   	break;
        }
    	// ESP_LOGI(TAG, "%d) rand 8 0x%02X",pos,rand8);
		*dest++=rand8;
    	step++;
    	pos++;
    }

    return 1; // random data was generated
}

void testFileIO(const char *filename,uint32_t bufsize) {
    FILE* f;
    static uint8_t buf[MAX_BUFSIZE];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    uint32_t loops;
    struct stat _stat;

    if (bufsize > MAX_BUFSIZE)  {
    	bufsize = MAX_BUFSIZE;
    }
    rng_func(buf, bufsize);

    f = fopen(filename, "w");
    if(!f){
    	ESP_LOGE(TAG,"Failed to open file for writing");
        return;
    }

    size_t i;
    loops = 1048576 / bufsize;
    // ESP_LOGI(TAG,"loops %u bufsize (%d)", loops, bufsize);

    start = millis();
    for(i=0; i<loops; i++){
    	fwrite(buf, bufsize, 1, f);
    }
    end = millis() - start;
    ESP_LOGI(TAG,"%u bytes (%u) written in %u ms", loops*bufsize, bufsize, end);
    fclose(f);

    f = fopen(filename, "r");
    if(f){
        stat(filename,&_stat);

        len = _stat.st_size;
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > bufsize){
                toRead = bufsize;
            }
            fread(buf, toRead, 1, f);
            len -= toRead;
        }
        end = millis() - start;
        ESP_LOGI(TAG,"%u bytes read in %u ms", flen, end);
        fclose(f);
    } else {
    	ESP_LOGE(TAG,"Failed to open file for reading");
    }


}


//======================================================================================================


void app_main()
{
    ESP_LOGI(TAG, "Initializing SD card");
    #ifndef USE_SPI_MODE
    ESP_LOGI(TAG, "Using SDMMC peripheral");
   // sdmmc_host_t host = SDMMC_HOST_DEFAULT();
     sdmmc_host_t host = {
    .flags = SDMMC_HOST_FLAG_8BIT | 
             SDMMC_HOST_FLAG_4BIT |
             SDMMC_HOST_FLAG_1BIT | 
             SDMMC_HOST_FLAG_DDR, 
    .slot = SDMMC_HOST_SLOT_1, 
    .max_freq_khz = SDMMC_FREQ_52M,
    .io_voltage = 3.3f, 
    .init = &sdmmc_host_init, 
    .set_bus_width = &sdmmc_host_set_bus_width, 
    .get_bus_width = &sdmmc_host_get_slot_width, 
    .set_bus_ddr_mode = &sdmmc_host_set_bus_ddr_mode, 
    .set_card_clk = &sdmmc_host_set_card_clk, 
    .do_transaction = &sdmmc_host_do_transaction, 
    .deinit = &sdmmc_host_deinit, 
    .io_int_enable = sdmmc_host_io_int_enable, 
    .io_int_wait = sdmmc_host_io_int_wait, 
    .command_timeout_ms = 0, 
    };

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    // To use 1-line SD mode, uncomment the following line:
    // slot_config.width = 1;
    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY); // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);  // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);  // D1, needed in 4-line mode only
    gpio_set_pull_mode(12, GPIO_PULLUP_ONLY); // D2, needed in 4-line mode only
    gpio_set_pull_mode(13, GPIO_PULLUP_ONLY); // D3, needed in 4- and 1-line modes
    // GPIOs 15, 2, 4, 12, 13 should have external 10k pull-ups.
    // Internal pull-ups are not sufficient. However, enabling internal pull-ups
    // does make a difference some boards, so we do that here.
   

#else
    ESP_LOGI(TAG, "Using SPI peripheral");

    //sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdmmc_host_t host = {
    .flags = SDMMC_HOST_FLAG_SPI, 
    .slot = HSPI_HOST, 
    .max_freq_khz = SDMMC_FREQ_26M, 
    .io_voltage = 3.3f, 
    .init = &sdspi_host_init, 
    .set_bus_width = NULL, 
    .get_bus_width = NULL, 
    .set_bus_ddr_mode = NULL, 
    .set_card_clk = &sdspi_host_set_card_clk, 
    .do_transaction = &sdspi_host_do_transaction, 
    .deinit = &sdspi_host_deinit, 
    .io_int_enable = NULL, 
    .io_int_wait = NULL, 
    .command_timeout_ms = 0, 
    };
    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = PIN_NUM_MISO;
    slot_config.gpio_mosi = PIN_NUM_MOSI;
    slot_config.gpio_sck = PIN_NUM_CLK;
    slot_config.gpio_cs = PIN_NUM_CS;
    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
#endif //USE_SPI_MODE

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
    // Please check its source code and implement error recovery when developing
    // production applications.
    sdmmc_card_t *card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/spiffs", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set format_if_mount_failed = true.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    initialise_wifi();  // use for file_serving_sdcard_ex
    /* Initialize file storage */
    //ESP_ERROR_CHECK(init_spiffs()); // use for file_serving_spiffs_ex
    // ESP_LOGI(TAG, "Opening file");
    // FILE* f = fopen("/sdcard/hello.txt", "w");
    // if (f == NULL) {
    //     ESP_LOGE(TAG, "Failed to open file for writing");
    //     return;
    // }
    // fprintf(f, "Hello %s!\n", card->cid.name);
    // fclose(f);
    // ESP_LOGI(TAG, "File written");

    // // Check if destination file exists before renaming
    // struct stat st;
    // if (stat("/sdcard/foo.txt", &st) == 0) {
    //     // Delete it if it exists
    //     unlink("/sdcard/foo.txt");
    // }

    // // Rename original file
    // ESP_LOGI(TAG, "Renaming file");
    // if (rename("/sdcard/hello.txt", "/sdcard/foo.txt") != 0) {
    //     ESP_LOGE(TAG, "Rename failed");
    //     return;
    // }

    // // Open renamed file for reading
    // ESP_LOGI(TAG, "Reading file");
    // f = fopen("/sdcard/foo.txt", "r");
    // if (f == NULL) {
    //     ESP_LOGE(TAG, "Failed to open file for reading");
    //     return;
    // }
    // char line[64];
    // fgets(line, sizeof(line), f);
    // fclose(f);
    // // strip newline
    // char* pos = strchr(line, '\n');
    // if (pos) {
    //     *pos = '\0';
    // }
    // ESP_LOGI(TAG, "Read from file: '%s'", line);

    // struct dirent *pDirent;
    // DIR *pDir;
    // struct stat _stat;
    // char cPath[1024];

    // mkdir("/sdcard/mydir",0777);

    // pDir = opendir("/sdcard");
    // if (pDir == NULL) {
    //     printf ("Cannot open directory '/sdcard'\n");
    //     return;
    // }

    // while ((pDirent = readdir(pDir)) != NULL) {
    // 	sprintf(cPath,"/sdcard/%s",pDirent->d_name);
    // 	stat(cPath,&_stat);
    // 	if(S_ISDIR(_stat.st_mode)) {
    // 		printf ("[%s] DIR %ld\n", pDirent->d_name,_stat.st_size);
    // 	} else {
    // 		printf ("[%s] FILE %ld\n", pDirent->d_name,_stat.st_size);
    // 	}
    // }
    // closedir (pDir);
    // testFileIO("/sdcard/TEST1.txt",512);
    // testFileIO("/sdcard/TEST2.txt",1024);
    // testFileIO("/sdcard/TEST3.txt",2048);
    // testFileIO("/sdcard/TEST4.txt",4096);
    // testFileIO("/sdcard/TEST5.txt",8192);
    // testFileIO("/sdcard/TEST6.txt",16384);
    // // All done, unmount partition and disable SDMMC or SPI peripheral
    // esp_vfs_fat_sdmmc_unmount();
    // ESP_LOGI(TAG, "Card unmounted");
    /* Start the file server */
   ESP_ERROR_CHECK(start_file_server("/spiffs")); // use for file_serving_sdcard_ex
}
