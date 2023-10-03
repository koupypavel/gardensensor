/*Copyright 2022 Canlab s.r.o.*/

#pragma once
#include "TaskCommon.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include <sys/param.h>
#include <dirent.h>

int openFile(char *filename, FILE **fp);
void closeFile(FILE *fp);
int appendBlock(uint8_t *data, size_t size, FILE *fp);
char *randomFilename(size_t length);
void renameFile(const char *filename, const char *newFilename);
void removeZeroedFiles(void);