/*Copyright 2022 Canlab s.r.o.*/

#include <stdio.h>
#include "FileHandler.h"

static const char *TAG = "FileHandler";

int openFile(char *filename, FILE **fp)
{
    int result = EXIT_SUCCESS;

    *fp = fopen(filename, "a");
    if (fp == NULL)
    {
        ESP_LOGE("TAG", "Failed to open file for writing");
        return EXIT_FAILURE;
    }
    return result;
}

void closeFile(FILE *fp)
{
    fclose(fp);
}

void removeZeroedFiles()
{
    char entrypath[128];
    char entrysize[32];
    const char *entrytype;

    struct dirent *entry;
    struct stat entry_stat;

    DIR *dir = opendir("/data");
    const size_t dirpath_len = strlen("/data/");

    /* Retrieve the base path of file storage to construct the full path */
    strlcpy(entrypath, "/data/", sizeof(entrypath));

    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to stat dir : %s", "/data");
        return;
    }
    /* Iterate over all files / folders and fetch their names and sizes */
    while ((entry = readdir(dir)) != NULL)
    {
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        if (stat(entrypath, &entry_stat) == -1)
        {
            ESP_LOGE(TAG, "Failed to stat %s : %s", entrytype, entrypath);
            continue;
        }
        sprintf(entrysize, "%ld", entry_stat.st_size);

        // ignoring config.ini
        if (strcmp(entry->d_name, "config.ini") == 0)
        {
            continue;
        }
            ESP_LOGI(TAG, "To unlink Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);
        
        /* Send chunk of HTML file containing table entries with file name and size */
        if (strcmp(entrysize, "0") ==  0)
        {
            ESP_LOGI(TAG, "To unlink Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

            unlink(entrypath);
        }
    }
    closedir(dir);
}

int appendBlock(uint8_t *data, size_t size, FILE *fp)
{
    int result = EXIT_SUCCESS;
    size_t elements_written = fwrite(data, sizeof(uint8_t), size, fp);
    if (elements_written != size)
    {
        result = EXIT_FAILURE;
    }
    return result;
}

void renameFile(const char *filename, const char *newFilename)
{
    // Check if destination file exists before renaming
    struct stat st;
    if (stat(newFilename, &st) == 0)
    {
        // Delete it if it exists
        unlink(newFilename);
    }
    // Rename original file
    ESP_LOGI("TAG", "Renaming file");
    if (rename(filename, newFilename) != 0)
    {
        ESP_LOGE("TAG", "Rename failed");
        return;
    }
    // Open renam
}

char *randomFilename(size_t length)
{
    length = length + 10;
    const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    char *randomString;
    randomString = malloc(length);

    if (length)
    {
        if (randomString)
        {
            int l = (int)(sizeof(charset) - 1);
            int key;
            for (int n = 0; n < length; n++)
            {
                key = rand() % l;
                randomString[n] = charset[key];
            }
            randomString[0] = '/';
            randomString[1] = 'd';
            randomString[2] = 'a';
            randomString[3] = 't';
            randomString[4] = 'a';
            randomString[5] = '/';
            randomString[length - 4] = '.';
            randomString[length - 3] = 'd';
            randomString[length - 2] = 'd';
            randomString[length - 1] = 'd';
        }
    }
    return randomString;
}