#include <stdio.h>

// FreeRTOS things 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// GPIO
#include "driver/gpio.h"

// PWM things
#include "driver/ledc.h"

// SPI
#include "driver/spi_master.h"

// more
#include "esp_log.h"

// TLC5916 pin definitions
#define LE_PIN 0 // latch enable (treat as spi cs)
#define OE_PIN 1 // output enable for pwm brightness control (active low)
#define SDO_PIN 19

// SPI pin definitions
#define SPI_CLK_PIN 6
#define SPI_MOSI_PIN 7
#define SPI_MISO_PIN -1 // unused
#define SPI_CS_PIN -1 // unused

// System config
#define NUM_TLC5916 7 // number of tlc5916 chips on spi bus
#define TOTAL_BYTES NUM_TLC5916 // 1 tlc5916 = 1 byte

spi_device_handle_t spi_device;

void init_tlc5916(void){

    // configure the spi bus
    spi_bus_config_t buscfg = {
        .miso_io_num        = SPI_MISO_PIN,
        .mosi_io_num        = SPI_MOSI_PIN,
        .sclk_io_num        = SPI_CLK_PIN,
        .quadwp_io_num      = -1,
        .quadhd_io_num      = -1,
        .max_transfer_sz    = 64
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // configure the spi device
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz     = 1000000, // 1MHz, double check tlc capability
        .mode               = 0,
        .spics_io_num       = LE_PIN,
        .queue_size         = 1,
        .flags              = SPI_DEVICE_NO_DUMMY,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi_device));

    // configure pwm timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode         = LEDC_LOW_SPEED_MODE,
        .timer_num          = LEDC_TIMER_0,
        .duty_resolution    = LEDC_TIMER_8_BIT, // duty cycle range from 0 to 255
        .freq_hz            = 5000, // 5kHz
        .clk_cfg            = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // configure pwm channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = OE_PIN,
        .duty       = 0, // 0 = full brightness since pin is active low
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void set_brightness(uint8_t brightness){
    // oe is active low, need to invert duty value
    uint32_t duty = 255 - brightness;
    
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

void update_tlc5916(uint8_t *data_buffer){
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    t.length = TOTAL_BYTES * 8; // 7 bytes * 8 bits = 56 bits
    t.tx_buffer = data_buffer;
    t.rx_buffer = NULL;

    ESP_ERROR_CHECK(spi_device_transmit(spi_device, &t));
}

void app_main(void){

    init_tlc5916();

    // tx_data[0] = Chip 7 (end of chain)
    // tx_data[6] = Chip 1 (directly connected to ESP32 MOSI)
    uint8_t tx_data[TOTAL_BYTES] = {0};

    // Turn all 56 outputs ON for the demo
    memset(tx_data, 0xFF, TOTAL_BYTES);
    update_tlc5916(tx_data);

    while(1){
        
        // fade up from dim to full brightness
        for (int b = 0; b <= 255; b++) {
            set_brightness(b);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // fade down from bright to dim
        for (int b = 255; b >= 0; b--) {
            set_brightness(b);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        // delay
        vTaskDelay(pdMS_TO_TICKS(500));
    }

}
