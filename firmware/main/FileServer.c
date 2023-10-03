
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"
#include <sys/stat.h>
// #include <mbedtls/base64.h>
#include "esp_netif.h"
#include "TaskCommon.h"

#define STORAGE_NAMESPACE "storage"
httpd_handle_t server = NULL;
const char *config_paths = "/data/config.ini";
settings_st ini_config;
dictionary *ini;

typedef struct
{
    char url[32];
    char parameter[128];
} URL_t;
/* Max length a file path can have on storage */
#//define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 256)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE (500 * 1024) // 300 KB
#define MAX_FILE_SIZE_STR "500KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE 8192

struct file_server_data
{
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

static const char *TAG = "FileServer";

esp_err_t save_key_value(char *key, char *value)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    // Write
    err = nvs_set_str(my_handle, key, value);
    if (err != ESP_OK)
        return err;

    // Commit written value.
    // After setting any values, nvs_commit() must be called to ensure changes are written
    // to flash storage. Implementations may write to storage at other times,
    // but this is not guaranteed.
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        return err;

    // Close
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t load_key_value(char *key, char *value, size_t size)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    // Read
    size_t _size = size;
    err = nvs_get_str(my_handle, key, value, &_size);
    ESP_LOGI(TAG, "nvs_get_str err=%d", err);
    // if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    if (err != ESP_OK)
        return err;
    ESP_LOGI(TAG, "err=%d key=[%s] value=[%s] _size=%d", err, key, value, _size);

    // Close
    nvs_close(my_handle);
    // return ESP_OK;
    return err;
}

int find_value(char *key, char *parameter, char *value)
{
    // char * addr1;
    char *addr1 = strstr(parameter, key);
    if (addr1 == NULL)
        return 0;
    ESP_LOGD(TAG, "addr1=%s", addr1);

    char *addr2 = addr1 + strlen(key);
    ESP_LOGD(TAG, "addr2=[%s]", addr2);

    char *addr3 = strstr(addr2, "&");
    ESP_LOGD(TAG, "addr3=%p", addr3);
    if (addr3 == NULL)
    {
        strcpy(value, addr2);
    }
    else
    {
        int length = addr3 - addr2;
        ESP_LOGD(TAG, "addr2=%p addr3=%p length=%d", addr2, addr3, length);
        strncpy(value, addr2, length);
        value[length] = 0;
    }
    ESP_LOGI(TAG, "key=[%s] value=[%s]", key, value);
    return strlen(value);
}

/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0); // Response body can be empty
    return ESP_OK;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[] asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

static esp_err_t css_get_handler(httpd_req_t *req)
{
    extern const unsigned char admin_css_start[] asm("_binary_admin_css_start");
    extern const unsigned char admin_css_end[] asm("_binary_admin_css_end");
    const size_t admin_css_size = (admin_css_end - admin_css_start);
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)admin_css_start, admin_css_size);
    return ESP_OK;
}

static esp_err_t logo_get_handler(httpd_req_t *req)
{
    extern const unsigned char logo_start[] asm("_binary_canlab_png_start");
    extern const unsigned char logo_end[] asm("_binary_canlab_png_end");
    const size_t logo_size = (logo_end - logo_start);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, (const char *)logo_start, logo_size);
    return ESP_OK;
}
/*
 * Serve OTA update portal (index.html)
 */

static esp_err_t ota_get_handler(httpd_req_t *req)
{

    /* Add file upload form and script which on execution sends a POST request to /upload */
    extern const uint8_t update_html_start[] asm("_binary_update_html_start");
    extern const uint8_t update_html_end[] asm("_binary_update_html_end");
    if (ini == NULL)
        ESP_LOGE("FileServer", "setup config fail");
    else
    {
        if (ini_config.lang == LANG_CS)
            httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html lang=\"cs\">");
        else if (ini_config.lang == LANG_EN)
        {
            httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html lang=\"en\">");
        }
    }
    httpd_resp_send_chunk(req, (const char *)update_html_start, update_html_end - update_html_start);

    if (ini_config.lang == LANG_CS)
        httpd_resp_sendstr_chunk(req, "<h2>Aktuální verze: ");
    else if (ini_config.lang == LANG_EN)
        httpd_resp_sendstr_chunk(req, "<h2>Current version: ");

    httpd_resp_sendstr_chunk(req, "PROJECT_VER");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</h2></main><script>");
        httpd_resp_sendstr_chunk(req, "var statusData=[");
    httpd_resp_sendstr_chunk(req, "{name:\"version\",val:\"Version:");
    //httpd_resp_sendstr_chunk(req, ini_config.version ? "true}," : "false},");
    httpd_resp_sendstr_chunk(req, "PROJECT_VER");
    httpd_resp_sendstr_chunk(req, "\"},");
    httpd_resp_sendstr_chunk(req, "{name:\"sd_storage\",val:\"");
    uint64_t sd_total_bytes = 0;
    uint64_t sd_free_bytes = 0;
    esp_vfs_fat_info("/sdcard", &sd_total_bytes, &sd_free_bytes);
        char chsd_bytes[64];
    sprintf(chsd_bytes, "SD : %llu/%llu GB\"},", sd_free_bytes/1000000000,  sd_total_bytes/1000000000);
    httpd_resp_sendstr_chunk(req, chsd_bytes);
    httpd_resp_sendstr_chunk(req, "{name:\"flash_storage\",val:\"");
    uint64_t flash_total_bytes = 0;
    uint64_t flash_free_bytes = 0;
    esp_vfs_fat_info("/data", &flash_total_bytes, &flash_free_bytes);
    char chflash_bytes[64];
    sprintf(chflash_bytes, "Flash: %llu/%llu MB\"}", flash_free_bytes/1000000,  flash_total_bytes/1000000);
    httpd_resp_sendstr_chunk(req, chflash_bytes);
    httpd_resp_sendstr_chunk(req, "];");
    httpd_resp_sendstr_chunk(req, "function findStatusItem(item) {for (let data of statusData) {if (data.name == item) {return data;}}}");
    httpd_resp_sendstr_chunk(req, "function populateStorageStatus(){var stat_version = document.getElementById(\"stat_version\");var stat_sd = document.getElementById(\"stat_sd\");var stat_flash = document.getElementById(\"stat_flash\");  stat_version.innerHTML = findStatusItem('version').val;stat_sd.innerHTML = findStatusItem('sd_storage').val;stat_flash.innerHTML = findStatusItem('flash_storage').val;}populateStorageStatus();");
    httpd_resp_sendstr_chunk(req, "</script></body></html>");
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}


static esp_err_t http_resp_dir_html_tacho(httpd_req_t *req, const char *dirpath)
{
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    char datestring[16];
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = opendir(dirpath);
    const size_t dirpath_len = strlen(dirpath);

    /* Retrieve the base path of file storage to construct the full path */
    strlcpy(entrypath, dirpath, sizeof(entrypath));

    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }

    /* Get handle to embedded file upload script */
    extern const unsigned char upload_script_start[] asm("_binary_admin_html_start");
    extern const unsigned char upload_script_end[] asm("_binary_admin_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start); /* Send HTML file header */
    if (ini == NULL)
        ESP_LOGE("FileServer", "setup config fail");
    else
    {
        if (ini_config.lang == LANG_CS)
            httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html lang=\"cs\">");
        else if (ini_config.lang == LANG_EN)
        {
            httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html lang=\"en\">");
        }

        /* Add file upload form and script which on execution sends a POST request to /upload */
        httpd_resp_send_chunk(req, (const char *)upload_script_start, upload_script_size);

        if (ini_config.lang == LANG_CS)
        {
            /* Send file-list table definition and column labels */
            httpd_resp_sendstr_chunk(req,
                                     "<div class=\"btn-group\">"
                                     "<button onclick=\"download_files(tableData)\">Stáhnout vše</button>"
                                     "<button onclick=\"downloadChecked()\">Stáhnout označené</button>"
                                     " <input style=\"margin-left: 12px; \" type=\"text\" id=\"myinput\" placeholder=\"Search...\" title=\"Type in something\">"
                                     "</div>"
                                     "<table class=\"styled-table\">"
                                     "<thead><tr>"
                                     "<th><span id=\"index\" class=\"table-column\">#</span></th>"
                                     "<th><span id=\"name\" class=\"w3-button table-column\">Název <i class=\"caret\"></i></span></th>"
                                     "<th><span id=\"size\" class=\"w3-button table-column\">Velikost (Bytes) <i class=\"caret\"></i></span></th>"
                                     "<th><span id=\"date\" class=\"w3-button table-column\">Datum <i class=\"caret\"></i></span></th>"
                                     "<th><span id=\"actions\" class=\"table-column\">Možnosti</span></th>"
                                     "</tr></thead>"
                                     "<tbody id=\"mytable\"></tbody></table>");
        }
        else if (ini_config.lang == LANG_EN)
        {
            /* Send file-list table definition and column labels */
            httpd_resp_sendstr_chunk(req,
                                     "<div class=\"btn-group\">"
                                     "<button onclick=\"download_files(tableData)\">Download all</button>"
                                     "<button onclick=\"downloadChecked()\">Download selected</button>"
                                     " <input style=\"margin-left: 12px; \" type=\"text\" id=\"myinput\" placeholder=\"Search...\" title=\"Type in something\">"
                                     "</div>"
                                     "<table class=\"styled-table\">"
                                     "<thead><tr>"
                                     "<th><span id=\"index\" class=\"table-column\">#</span></th>"
                                     "<th><span id=\"name\" class=\"w3-button table-column\">Name <i class=\"caret\"></i></span></th>"
                                     "<th><span id=\"size\" class=\"w3-button table-column\">Size <i class=\"caret\"></i></span></th>"
                                     "<th><span id=\"date\" class=\"w3-button table-column\">Date <i class=\"caret\"></i></span></th>"
                                     "<th><span id=\"actions\" class=\"table-column\">Actions</span></th>"
                                     "</tr></thead>"
                                     "<tbody id=\"mytable\"></tbody></table>");
        }
        /* Iterate over all files / folders and fetch their names and sizes */
    }

    /* Finish the file list table */
    bool first = true;
     httpd_resp_sendstr_chunk(req, "<script>");
    ////////////////////////////////////////////////////////////////////
        
    httpd_resp_sendstr_chunk(req, "var statusData=[");
    httpd_resp_sendstr_chunk(req, "{name:\"version\",val:\"Version:");
    //httpd_resp_sendstr_chunk(req, ini_config.version ? "true}," : "false},");
    httpd_resp_sendstr_chunk(req, "PROJECT_VER");
    httpd_resp_sendstr_chunk(req, "\"},");
    httpd_resp_sendstr_chunk(req, "{name:\"sd_storage\",val:\"");
    uint64_t sd_total_bytes = 0;
    uint64_t sd_free_bytes = 0;
    esp_vfs_fat_info("/sdcard", &sd_total_bytes, &sd_free_bytes);
        char chsd_bytes[64];
    sprintf(chsd_bytes, "SD : %llu/%llu GB\"},", sd_free_bytes/1000000000,  sd_total_bytes/1000000000);
    httpd_resp_sendstr_chunk(req, chsd_bytes);
    httpd_resp_sendstr_chunk(req, "{name:\"flash_storage\",val:\"");
    uint64_t flash_total_bytes = 0;
    uint64_t flash_free_bytes = 0;
    esp_vfs_fat_info("/data", &flash_total_bytes, &flash_free_bytes);
    char chflash_bytes[64];
    sprintf(chflash_bytes, "Flash: %llu/%llu MB\"}", flash_free_bytes/1000000,  flash_total_bytes/1000000);
    httpd_resp_sendstr_chunk(req, chflash_bytes);
    httpd_resp_sendstr_chunk(req, "];");

    ////////////////////////////////////////////////////////////////////
    httpd_resp_sendstr_chunk(req, "var tableData = [");

    while ((entry = readdir(dir)) != NULL)
    {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1)
        {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);
        ESP_LOGI(TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

        if (entry->d_type == DT_DIR || (strcmp(entry->d_name, "config.ini") == 0))
            continue;

        if (first)
        {
            httpd_resp_sendstr_chunk(req, "{");
            first = false;
        }
        else
        {
            httpd_resp_sendstr_chunk(req, ",{");
        }
        httpd_resp_sendstr_chunk(req, "link: '");
        httpd_resp_sendstr_chunk(req, req->uri);
        httpd_resp_sendstr_chunk(req, entry->d_name);
        if (entry->d_type == DT_DIR)
        {
            httpd_resp_sendstr_chunk(req, "/");
        }
        httpd_resp_sendstr_chunk(req, "', name: '");
        httpd_resp_sendstr_chunk(req, entry->d_name);
        httpd_resp_sendstr_chunk(req, "', size: ");
        httpd_resp_sendstr_chunk(req, entrysize);
        time_t t = entry_stat.st_mtime;
        struct tm lt;
        localtime_r(&t, &lt);
        char timbuf[80];
        strftime(timbuf, sizeof(timbuf), "%c", &lt);
        httpd_resp_sendstr_chunk(req, ", mdate: '");
        httpd_resp_sendstr_chunk(req, timbuf);
        httpd_resp_sendstr_chunk(req, "'}");
    }
    httpd_resp_sendstr_chunk(req, "];");
    closedir(dir);

    /* Get handle to embedded file upload script */
    extern const unsigned char table_script_start[] asm("_binary_table_js_start");
    extern const unsigned char table_script_end[] asm("_binary_table_js_end");
    const size_t table_script_size = (table_script_end - table_script_start);

    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)table_script_start, table_script_size);
    httpd_resp_sendstr_chunk(req, "</script>");
    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</main></body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t http_resp_dir_html_settings(httpd_req_t *req)
{
    /* Get handle to embedded file upload script */
    extern const unsigned char setting_start[] asm("_binary_setting_html_start");
    extern const unsigned char setting_end[] asm("_binary_setting_html_end");
    const size_t setting_size = (setting_end - setting_start);

    ini = iniparser_load(config_paths);
    if (ini == NULL)
    {
        ESP_LOGE("FileServer", "setup config fail");
    }
    else
    {
        parse_ini_file(ini, &ini_config);

        if (ini_config.lang == LANG_CS)
            httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html lang=\"cs\">");
        else if (ini_config.lang == LANG_EN)
        {
            httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html lang=\"en\">");
        }
        /* Add file upload form and script which on execution sends a POST request to /upload */
        httpd_resp_send_chunk(req, (const char *)setting_start, setting_size);

        httpd_resp_sendstr_chunk(req, "<script>");


    httpd_resp_sendstr_chunk(req, "var statusData=[");
    httpd_resp_sendstr_chunk(req, "{name:\"version\",val:\"Version:");
    //httpd_resp_sendstr_chunk(req, ini_config.version ? "true}," : "false},");
    httpd_resp_sendstr_chunk(req, "PROJECT_VER");
    httpd_resp_sendstr_chunk(req, "\"},");
    httpd_resp_sendstr_chunk(req, "{name:\"sd_storage\",val:\"");
    uint64_t sd_total_bytes = 0;
    uint64_t sd_free_bytes = 0;
    esp_vfs_fat_info("/sdcard", &sd_total_bytes, &sd_free_bytes);
        char chsd_bytes[64];
    sprintf(chsd_bytes, "SD : %llu/%llu GB\"},", sd_free_bytes/1000000000,  sd_total_bytes/1000000000);
    httpd_resp_sendstr_chunk(req, chsd_bytes);
    httpd_resp_sendstr_chunk(req, "{name:\"flash_storage\",val:\"");
    uint64_t flash_total_bytes = 0;
    uint64_t flash_free_bytes = 0;
    esp_vfs_fat_info("/data", &flash_total_bytes, &flash_free_bytes);
    char chflash_bytes[64];
    sprintf(chflash_bytes, "Flash: %llu/%llu MB\"}", flash_free_bytes/1000000,  flash_total_bytes/1000000);
    httpd_resp_sendstr_chunk(req, chflash_bytes);
    httpd_resp_sendstr_chunk(req, "];");

        httpd_resp_sendstr_chunk(req, "var settingData=[");

        httpd_resp_sendstr_chunk(req, "{name:\"timestamp\"");
        httpd_resp_sendstr_chunk(req, ",val:");
        char timestamp[16];
        sprintf(timestamp, "%d},", ini_config.timestamp);
        httpd_resp_sendstr_chunk(req, timestamp);

        httpd_resp_sendstr_chunk(req, "{name:\"ssid\"");
        httpd_resp_sendstr_chunk(req, ",val:\"");
        httpd_resp_sendstr_chunk(req, ini_config.wifi_ssid);

        httpd_resp_sendstr_chunk(req, "\"},");

        httpd_resp_sendstr_chunk(req, "{name:\"passwd\"");
        httpd_resp_sendstr_chunk(req, ",val:\"");
        httpd_resp_sendstr_chunk(req, ini_config.wifi_key);
        httpd_resp_sendstr_chunk(req, "\"},");

        httpd_resp_sendstr_chunk(req, "{name:\"web_app_run\",val:");
        httpd_resp_sendstr_chunk(req, ini_config.web_run ? "true}," : "false},");

        httpd_resp_sendstr_chunk(req, "{name:\"web_app_timeout\"");
        httpd_resp_sendstr_chunk(req, ",val:");
        char web_timeout[16];
        sprintf(web_timeout, "%d},", ini_config.web_timeout);
        httpd_resp_sendstr_chunk(req, web_timeout);

        httpd_resp_sendstr_chunk(req, "{name:\"web_lang\"");
        httpd_resp_sendstr_chunk(req, ",val:");
        char web_lang[16];
        sprintf(web_lang, "%d},", ini_config.lang);
        httpd_resp_sendstr_chunk(req, web_lang);
        httpd_resp_sendstr_chunk(req, "];");
    }
    /* Get handle to embedded file upload script */
    extern const unsigned char setting_script_start[] asm("_binary_setting_js_start");
    extern const unsigned char setting_script_end[] asm("_binary_setting_js_end");
    const size_t setting_script_size = (setting_script_end - setting_script_start);

    /* Add file upload form and script which on execution sends a POST request to /upload */
    httpd_resp_send_chunk(req, (const char *)setting_script_start, setting_script_size);
    httpd_resp_sendstr_chunk(req, "</script></main></body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf"))
    {
        return httpd_resp_set_type(req, "application/pdf");
    }
    else if (IS_FILE_EXT(filename, ".html"))
    {
        return httpd_resp_set_type(req, "text/html");
    }
    else if (IS_FILE_EXT(filename, ".css"))
    {
        return httpd_resp_set_type(req, "text/css");
    }
    else if (IS_FILE_EXT(filename, ".jpeg"))
    {
        return httpd_resp_set_type(req, "image/jpeg");
    }
    else if (IS_FILE_EXT(filename, ".ico"))
    {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest)
    {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize)
    {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* Handler to download a file kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
    if (!filename)
    {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* If name has trailing '/', respond with directory contents */
    if (filename[strlen(filename) - 1] == '/')
    {
        return http_resp_dir_html_tacho(req, filepath);
    }

    if (stat(filepath, &file_stat) == -1)
    {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        if (strcmp(filename, "/index.html") == 0)
        {
            return index_html_get_handler(req);
        }
        else if (strcmp(filename, "/favicon.ico") == 0)
        {
            return favicon_get_handler(req);
        }
        else if (strcmp(filename, "/settings") == 0)
        {
            return http_resp_dir_html_settings(req);
        }
        else if (strcmp(filename, "/ota") == 0)
        {
            return ota_get_handler(req);
        }
        else if (strcmp(filename, "/admin.css") == 0)
        {
            return css_get_handler(req);
        }
        else if (strcmp(filename, "/canlab.png") == 0)
        {
            return logo_get_handler(req);
        }
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do
    {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0)
        {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
            {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler to upload a file onto the server */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    /* Skip leading "/upload" from URI to get filename something to do about */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/')
    {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0)
    {
        ESP_LOGE(TAG, "File already exists : %s", filepath);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        return ESP_FAIL;
    }

    /* File cannot be larger than a limit */
    if (req->content_len > MAX_FILE_SIZE)
    {
        ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
                            "File size must be less than " MAX_FILE_SIZE_STR "!");
        /* Return failure to close underlying connection else the
         * incoming file content will keep the socket busy */
        return ESP_FAIL;
    }

    fd = fopen(filepath, "w");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to create file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    int remaining = req->content_len;

    while (remaining > 0)
    {

        ESP_LOGI(TAG, "Remaining size : %d", remaining);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error,
             * close and delete the unfinished file*/
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received && (received != fwrite(buf, 1, received, fd)))
        {
            /* Couldn't write everything to file!
             * Storage may be full? */
            fclose(fd);
            unlink(filepath);

            ESP_LOGE(TAG, "File write failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    /* Close file upon upload completion */
    fclose(fd);
    ESP_LOGI(TAG, "File reception complete");

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File uploaded successfully");
    return ESP_OK;
}

void removeSubstr(char *string, char *sub)
{
    char *match;
    int len = strlen(sub);
    while ((match = strstr(string, sub)))
    {
        *match = '\0';
        strcat(string, match + len);
    }
}
esp_err_t parse_settings(char *buf, dictionary *setup)
{

    char overview[8] = {0};
    char events[8] = {0};
    char lang[8] = {0};
    // char activity_check[8] = {0};
    char detspeed[8] = {0};
    char web_run[8] = {0};
    char web_timeout[64] = {0};
    char technical[8] = {0};
    char activity_mode[8] = {0};
    char activity_begin[64] = {0};
    char activity_end[64] = {0};
    char driver_card[8] = {0};
    char wifi_ssid[64] = {0};
    char wifi_key[64] = {0};
    char wifi_mode[8] = {0};
    char timestamp_field[32] = {0};
    ///////////////////////////////////////////////////////////////////////// Tachograph

    if (find_value("overview=", buf, overview) > 0)
    {
        if (strcmp(overview, "on") == 0)
            iniparser_set(setup, "tacho:overview", "true");
    }
    else
        iniparser_set(setup, "tacho:overview", "false");

    if (find_value("events=", buf, events) > 0)
    {
        if (strcmp(events, "on") == 0)
            iniparser_set(setup, "tacho:events", "true");
    }
    else
        iniparser_set(setup, "tacho:events", "false");

    if (find_value("technicaldata=", buf, technical) > 0)
    {
        if (strcmp(technical, "on") == 0)
            iniparser_set(setup, "tacho:technicaldata", "true");
    }
    else
        iniparser_set(setup, "tacho:technicaldata", "false");

    if (find_value("detspeed=", buf, detspeed) > 0)
    {
        if (strcmp(detspeed, "on") == 0)
            iniparser_set(setup, "tacho:detailedspeed", "true");
    }
    else
        iniparser_set(setup, "tacho:detailedspeed", "false");

    if ((find_value("activity_check=", buf, activity_mode) > 0))
    {
        iniparser_set(setup, "tacho:activity_mode", activity_mode);
        if (strcmp(activity_mode, "1") == 0)
        {
            if (find_value("activity_from=", buf, activity_begin) > 0)
            {
                struct tm tm;
                time_t epoch;

                if (strptime(activity_begin, "%Y-%m-%d", &tm) != NULL)
                {
                    tm.tm_sec = 1;
                    tm.tm_min = 1;
                    tm.tm_hour = 12;
                    epoch = mktime(&tm);
                    char time_string[16];
                    // convert 123 to string [buf]
                    itoa(epoch, time_string, 10);
                    iniparser_set(setup, "tacho:activity_begin", time_string);
                }
            }
            else
                return ESP_FAIL;

            if (find_value("activity_to=", buf, activity_end) > 0)
            {
                struct tm tm;
                time_t epoch;

                if (strptime(activity_end, "%Y-%m-%d", &tm) != NULL)
                {
                    tm.tm_sec = 1;
                    tm.tm_min = 1;
                    tm.tm_hour = 12;
                    epoch = mktime(&tm);
                    char time_string[16];
                    // convert 123 to string [buf]
                    itoa(epoch, time_string, 10);
                    iniparser_set(setup, "tacho:activity_end", time_string);
                }
            }
            else
                return ESP_FAIL;
        }
    }
    if ((find_value("drivercard=", buf, driver_card) > 0))
    {
        if (strcmp(driver_card, "on") == 0)
            iniparser_set(setup, "tacho:drivercard", "true");
    }
    else
        iniparser_set(setup, "tacho:drivercard", "false");

    ///////////////////////////////////////////////////////////////////////// WIFI
    if ((find_value("wifi_mode=", buf, wifi_mode) > 0))
    {
        iniparser_set(setup, "wifi:wifi_mode", wifi_mode);
        if (strcmp(wifi_mode, "ac") == 0)
        {
        }
        else if (strcmp(wifi_mode, "client") == 0)
        {
        }
    }
    if (find_value("ssid=", buf, wifi_ssid) > 0)
    {

        if (find_value("passwd=", buf, wifi_key) > 0)
        {
            iniparser_set(setup, "wifi:ssid", wifi_ssid);
            iniparser_set(setup, "wifi:key", wifi_key);
        }
    }

    ///////////////////////////////////////////////////////////////////////// WEB INTERFACE

    if (find_value("run_web_app=", buf, web_run) > 0)
    {
        if (strcmp(web_run, "on") == 0)
            iniparser_set(setup, "web:web_run", "true");
    }
    else
        iniparser_set(setup, "web:web_run", "false");

    if (find_value("web_app_timeout=", buf, web_timeout) > 0)
    {
        iniparser_set(setup, "web:web_timeout", web_timeout);
    }
    ///////////////////////////////////////////////////////////////////////// TIME
    if (find_value("timestamp=", buf, timestamp_field) > 0)
    {
        struct tm tm;
        time_t epoch;

        removeSubstr(timestamp_field, "%3");
        ESP_LOGE("times", "removed %s", timestamp_field);

        if (strptime(timestamp_field, "%Y-%m-%dT%HA%M", &tm) != NULL)
        {
            tm.tm_sec = 1;
            epoch = mktime(&tm);
            char time_string[16];

            // convert 123 to string [buf]
            itoa(epoch, time_string, 10);
            ESP_LOGE("times", "loaded time = %s", time_string);
            iniparser_set(setup, "time:timestamp", time_string);
            setRTCTime(epoch);
        }
    }
    if (find_value("langs=", buf, lang) > 0)
    {
        if (strcmp(lang, "cs") == 0)
        {
            iniparser_set(setup, "web:web_language", "2");
        }
        else if (strcmp(lang, "en") == 0)
        {
            iniparser_set(setup, "web:web_language", "1");
        }
    }

    return ESP_OK;
}

static esp_err_t settings_post_handler(httpd_req_t *req)
{

    dictionary *setup;

    ESP_LOGI(TAG, "root_post_handler req->uri=[%s]", req->uri);
    ESP_LOGI(TAG, "root_post_handler content length %d", req->content_len);
    char *buf = malloc(req->content_len + 1);
    size_t off = 0;
    while (off < req->content_len)
    {
        /* Read data received in the request */
        int ret = httpd_req_recv(req, buf + off, req->content_len - off);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                httpd_resp_send_408(req);
            }
            free(buf);
            return ESP_FAIL;
        }
        off += ret;
    }
    buf[off] = '\0';
    ESP_LOGI(TAG, "root_post_handler buf=%s", buf);

    setup = iniparser_load(config_paths);
    if (setup == NULL)
    {
        ESP_LOGE("FileServer", "setup config fail");
    }
    else
    {
        ESP_LOGE("FileServer", "setup config success");

        parse_settings(buf, setup);
        ESP_LOGE("FileServer", "parse settings");

        save_ini_file(setup);
        ESP_LOGE("FileServer", "settings saved");
    }

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/settings");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);

    iniparser_freedict(setup);
    return ESP_OK;
}
/* Handler to delete a file from the server */
static esp_err_t delete_post_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    /* Skip leading "/delete" from URI to get filename */
    /* Note sizeof() counts NULL termination hence the -1 */
    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/delete") - 1, sizeof(filepath));
    if (!filename)
    {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    /* Filename cannot have a trailing '/' */
    if (filename[strlen(filename) - 1] == '/')
    {
        ESP_LOGE(TAG, "Invalid filename : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1)
    {
        ESP_LOGE(TAG, "File does not exist : %s", filename);
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleting file : %s", filename);
    /* Delete file */
    unlink(filepath);

    /* Redirect onto root to see the updated file list */
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_sendstr(req, "File deleted successfully");
    return ESP_OK;
}

/*
 * Handle OTA file upload
 */
esp_err_t update_post_handler(httpd_req_t *req)
{
    char buf[1000];
    esp_ota_handle_t ota_handle;
    int remaining = req->content_len;

    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

    while (remaining > 0)
    {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));

        // Timeout Error: Just retry
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
        {
            continue;

            // Serious Error: Abort OTA
        }
        else if (recv_len <= 0)
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
            return ESP_FAIL;
        }

        // Successful Upload: Flash firmware chunk
        if (esp_ota_write(ota_handle, (const void *)buf, recv_len) != ESP_OK)
        {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash Error");
            return ESP_FAIL;
        }

        remaining -= recv_len;
    }

    // Validate and switch to new OTA image and reboot
    if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
        return ESP_FAIL;
    }

    httpd_resp_sendstr(req, "Firmware update complete, rebooting now!\n");

    vTaskDelay(500 / portTICK_PERIOD_MS);
    esp_restart();

    return ESP_OK;
}
static esp_err_t download_status_get_handler(httpd_req_t *req)
{
    extern const uint8_t download_html_start[] asm("_binary_download_html_start");
    extern const uint8_t download_html_end[] asm("_binary_download_html_end");
    /* Send HTML file header */
    if (true)
        httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html lang=\"cs\">");
    else
        httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html lang=\"en\">");
    // httpd_resp_send(req, (const char *)download_html_start, download_html_end - download_html_start);
    httpd_resp_send_chunk(req, (const char *)download_html_start, download_html_end - download_html_start);
    httpd_resp_sendstr_chunk(req, "<progress id=\"file\" value=\"100\" max=\"100\" style=\"height:40px;width:50%;position: fixed;top: 50%;left: 25%;\"></progress>");

    /* Send remaining chunk of HTML file to complete it */
    httpd_resp_sendstr_chunk(req, "</body></html>");

    /* Send empty chunk to signal HTTP response completion */
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}
/* Function to start the file server */

/* URI handler for getting uploaded files */
httpd_uri_t status_download = {
    .uri = "/*", // Match all URIs of type /path/to/file
    .method = HTTP_GET,
    .handler = download_status_get_handler,
    .user_ctx = NULL // Pass server data as context
};

esp_err_t start_server()
{
    ini = iniparser_load(config_paths);
    if (ini == NULL)
    {
        ESP_LOGE("FileServer", "setup config fail");
    }
    else
    {
        parse_ini_file(ini, &ini_config);
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    }
    // httpd_register_uri_handler(server, &status_download);

    return ESP_OK;
}

esp_err_t start_file_server(const char *base_path)
{
    static struct file_server_data *server_data = NULL;

    if (server_data)
    {
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));

    httpd_unregister_uri_handler(server, "/*", HTTP_GET);

    /* URI handler for getting uploaded files */
    httpd_uri_t file_download = {
        .uri = "/*", // Match all URIs of type /path/to/file
        .method = HTTP_GET,
        .handler = download_get_handler,
        .user_ctx = server_data // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_download);

    /* URI handler for uploading files to server */
    httpd_uri_t file_upload = {
        .uri = "/upload/*", // Match all URIs of type /upload/path/to/file
        .method = HTTP_POST,
        .handler = upload_post_handler,
        .user_ctx = server_data // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_upload);

    /* URI handler for deleting files from server */
    httpd_uri_t file_delete = {
        .uri = "/delete/*", // Match all URIs of type /delete/path/to/file
        .method = HTTP_POST,
        .handler = delete_post_handler,
        .user_ctx = server_data // Pass server data as context
    };
    httpd_register_uri_handler(server, &file_delete);

    /* URI handler for deleting files from server */
    httpd_uri_t settings_tacho = {
        .uri = "/settings/*",
        .method = HTTP_POST,
        .handler = settings_post_handler,
        .user_ctx = server_data // Pass server data as context
    };
    httpd_register_uri_handler(server, &settings_tacho);

    httpd_uri_t update_post = {
        .uri = "/update",
        .method = HTTP_POST,
        .handler = update_post_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &update_post);
    return ESP_OK;
}
