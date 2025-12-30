#include "telemetry_mpu.h"
#include "config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "MPU6050";
static mpu_data_t current_mpu = {0};
static i2c_master_dev_handle_t dev_handle = NULL;

void mpu_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = MPU_I2C_NUM,
        .sda_io_num = MPU_SDA_PIN,
        .scl_io_num = MPU_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
    };
    i2c_master_bus_handle_t bus_handle;
    if (i2c_new_master_bus(&bus_cfg, &bus_handle) != ESP_OK) return;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x68,
        .scl_speed_hz = MPU_I2C_FREQ,
    };
    i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    ESP_LOGI(TAG, "MPU driver initialized (but not active in main)");
}

void mpu_update(void) {} // Dummy update
mpu_data_t mpu_get_data(void) { return current_mpu; }