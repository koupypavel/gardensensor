/*Copyright 2022 Canlab s.r.o.*/

#include "TaskCommon.h"
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include "esp_log.h"
#include "tinyusb.h"
#include "class/msc/msc_device.h"
#include "driver/gpio.h"
#include "MSCStorage.h"

static const char *TAG = "TaskCommon";

static bool is_mount = false;
#define PROMPT_STR CONFIG_IDF_TARGET
/************************************************************** TinyUSB ********/

/********* TinyUSB MSC callbacks ***************/

/** SCSI ASC/ASCQ codes. **/
/** User can add and use more codes as per the need of the application **/
#define SCSI_CODE_ASC_MEDIUM_NOT_PRESENT 0x3A             /** SCSI ASC code for 'MEDIUM NOT PRESENT' **/
#define SCSI_CODE_ASC_INVALID_COMMAND_OPERATION_CODE 0x20 /** SCSI ASC code for 'INVALID COMMAND OPERATION CODE' **/
#define SCSI_CODE_ASCQ 0x00

static void _mount(void);
static void _unmount(void);
bool is_eject = false;

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void)lun;
    ESP_LOGD(TAG, "tud_msc_inquiry_cb() invoked");

    const char vid[] = "TinyUSB";
    const char pid[] = "Flash Storage";
    const char rev[] = "0.1";

    memcpy(vendor_id, vid, strlen(vid));
    memcpy(product_id, pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void)lun;
    ESP_LOGD(TAG, "tud_msc_test_unit_ready_cb() invoked");

    if (is_eject)
    {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, SCSI_CODE_ASC_MEDIUM_NOT_PRESENT, SCSI_CODE_ASCQ);
        return false;
    }
    else
    {
        ESP_LOGD(TAG, "tud_msc_test_unit_ready_cb: MSC START: Expose Over USB");
        _unmount();
        return true;
    }
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    (void)lun;

    size_t size = storage_get_size();
    size_t sec_size = storage_get_sector_size();
    ESP_LOGI(TAG, "tud_msc_capacity_cb() size(%d), sec_size(%d)", size, sec_size);
    *block_count = size / sec_size;
    *block_size = sec_size;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    (void)lun;
    (void)power_condition;
    ESP_LOGI(TAG, "tud_msc_start_stop_cb() invoked, power_condition=%d, start=%d, load_eject=%d", power_condition, start, load_eject);

    if (load_eject && !start)
    {
        is_eject = true;
        ESP_LOGI(TAG, "tud_msc_start_stop_cb: MSC EJECT: Mount on Example");
        _mount();
    }
    return true;
}

// Invoked when received SCSI READ10 command
// - Address = lba * BLOCK_SIZE + offset
// - Application fill the buffer (up to bufsize) with address contents and return number of read byte.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    ESP_LOGD(TAG, "tud_msc_read10_cb() invoked, lun=%d, lba=%lu, offset=%lu, bufsize=%lu", lun, lba, offset, bufsize);

    size_t addr = lba * storage_get_sector_size() + offset;
    esp_err_t err = storage_read_sector(addr, bufsize, buffer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "storage_read_sector failed: 0x%x", err);
        return 0;
    }
    return bufsize;
}

// Invoked when received SCSI WRITE10 command
// - Address = lba * BLOCK_SIZE + offset
// - Application write data from buffer to address contents (up to bufsize) and return number of written byte.
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    ESP_LOGD(TAG, "tud_msc_write10_cb() invoked, lun=%d, lba=%lu, offset=%lu", lun, lba, offset);

    size_t addr = lba * storage_get_sector_size() + offset;
    esp_err_t err = storage_write_sector(addr, bufsize, buffer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "storage_write_sector failed: 0x%x", err);
        return 0;
    }
    return bufsize;
}

/**
 * Invoked when received an SCSI command not in built-in list below.
 * - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, TEST_UNIT_READY, START_STOP_UNIT, MODE_SENSE6, REQUEST_SENSE
 * - READ10 and WRITE10 has their own callbacks
 *
 * \param[in]   lun         Logical unit number
 * \param[in]   scsi_cmd    SCSI command contents which application must examine to response accordingly
 * \param[out]  buffer      Buffer for SCSI Data Stage.
 *                            - For INPUT: application must fill this with response.
 *                            - For OUTPUT it holds the Data from host
 * \param[in]   bufsize     Buffer's length.
 *
 * \return      Actual bytes processed, can be zero for no-data command.
 * \retval      negative    Indicate error e.g unsupported command, tinyusb will \b STALL the corresponding
 *                          endpoint and return failed status in command status wrapper phase.
 */
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    int32_t ret;

    ESP_LOGD(TAG, "tud_msc_scsi_cb() invoked. bufsize=%d", bufsize);

    switch (scsi_cmd[0])
    {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
        /* SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL is the Prevent/Allow Medium Removal
        command (1Eh) that requests the library to enable or disable user access to
        the storage media/partition. */
        ESP_LOGI(TAG, "tud_msc_scsi_cb() invoked: SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL");
        ret = 0;
        break;
    default:
        ESP_LOGW(TAG, "tud_msc_scsi_cb() invoked: %d", scsi_cmd[0]);
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_CODE_ASC_INVALID_COMMAND_OPERATION_CODE, SCSI_CODE_ASCQ);
        ret = -1;
        break;
    }
    return ret;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    is_eject = true;
    ESP_LOGI(TAG, "tud_umount_cb: Mount on Example");
    _mount();
}

// Invoked when device is mounted (configured)
void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "tud_mount_cb MSC START: Expose Over USB");
    _unmount();
}

// mount the partition and show all the files in BASE_PATH
static void _mount(void)
{
    ESP_LOGI(TAG, "Mount storage...");
    if (!is_mount)
    {
        ESP_ERROR_CHECK(storage_mount(BASE_PATH));
        is_mount = true;
    }
}
static void _unmount(void)
{
    if (!is_mount)
    {
        ESP_LOGD(TAG, "storage exposed over USB...");
        return;
    }
    ESP_LOGI(TAG, "Unmount storage...");
    ESP_ERROR_CHECK(storage_unmount());
    is_mount = false;
    is_eject = false;
    ESP_LOGI(TAG, "Unmount storage done...");
}
static uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01};

static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "Canlab",                   // 1: Manufacturer
    "TachoEasyDownload",        // 2: Product
    "123456",                   // 3: Serials
    "Data",                     // 4. MSC
};
/************************************************************** TinyUSB ********/

void start_deep_sleep(void)
{
    // esp_sleep_enable_ext0_wakeup(GPIO_NUM_8, 1);
    const int ext_wakeup_pin_1 = 1;
    const int ext_wakeup_pin_8 = 8;
    const uint64_t ext_tacho_mask = 1ULL << ext_wakeup_pin_8;
    const uint64_t ext_usb_mask = 1ULL << ext_wakeup_pin_1;
    esp_sleep_enable_ext1_wakeup(ext_tacho_mask | ext_usb_mask, ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_deep_sleep_start();
}

void initPowerSourcePin()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    //io_conf.pin_bit_mask = (1ULL << USB_POWER_PIN);
    io_conf.pull_down_en = 1;
    io_conf.pull_up_en = 0;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

void initFATFS(bool mount2app)
{
    ESP_LOGI(TAG, "Initializing storage...");
    ESP_ERROR_CHECK(storage_init());

    ESP_LOGI(TAG, "USB MSC initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &descriptor_config,
        .string_descriptor = string_desc_arr,
        .external_phy = false,
        .configuration_descriptor = desc_configuration,
        .self_powered = true,
        //.vbus_monitor_io = USB_POWER_PIN,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB MSC initialization DONE");

    if (mount2app)
    {
        ESP_LOGI(TAG, "Mount storage...");
        if (!is_mount)
        {
            ESP_ERROR_CHECK(storage_mount(BASE_PATH));
            is_mount = true;
        }
    }
}


void writeFlags(uint8_t flag_usr)
{
    xSemaphoreTake(xMutex, portMAX_DELAY);
    flags |= flag_usr;
    xSemaphoreGive(xMutex);
}

int parse_ini_file(dictionary *ini, settings_st *config)
{
    config->wifi_mode = iniparser_getint(ini, "wifi:mode", 0);
    config->wifi_ssid = iniparser_getstring(ini, "wifi:ssid", "TachoEasyDownload");
    config->wifi_key = iniparser_getstring(ini, "wifi:key", "Canlab4521.");

    config->web_run = iniparser_getboolean(ini, "web:web_run", true);
    config->lang = iniparser_getint(ini, "web:web_language", 2);
    config->web_timeout = iniparser_getint(ini, "web:web_timeout", 10);

    config->timestamp = iniparser_getint(ini, "time:timestamp", 1677530873);
    config->debug = iniparser_getboolean(ini, "web:debug", false);
    // iniparser_dump(ini, stdout);

    return 0;
}

int create_ini_file()
{
    FILE *ini;

    if ((ini = fopen("/data/config.ini", "w")) == NULL)
    {
        fprintf(stderr, "iniparser: cannot create config.ini\n");
        return -1;
    }

    fprintf(ini,
            "\n"
            "\n"
            "[wifi]\n"
            "\n"
            "ssid           = TachoEasyDownload \n"
            "key            = Canlab4521. \n"
            "mode           = 0 \n"
            "\n"
            "[time]\n"
            "\n"
            "timestamp      = 1676586541 \n"
            "\n"
            "[web]\n"
            "\n"
            "web_language   = 2 \n"
            "web_run        = true \n"
            "web_timeout    = 10 \n"
            "\n");
    fclose(ini);
    return 0;
}

int save_ini_file(dictionary *setup)
{
    FILE *ini;

    if ((ini = fopen("/data/config.ini", "w")) == NULL)
    {
        ESP_LOGE("TAG", "iniparser: cannot create config.ini");
        return -1;
    }

    iniparser_dump_ini(setup, ini);
    fclose(ini);

    return 0;
}

esp_err_t setRTCTime(time_t epoch_ts)
{
    struct timeval tv_ts = {.tv_sec = epoch_ts};
    settimeofday(&tv_ts, NULL);
    return ESP_OK;
}

int getRTCTime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec;
}

void getSDInfo()
{
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    esp_vfs_fat_info("/sdcard", &total_bytes, &free_bytes);
}
