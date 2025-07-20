// NAME : ILIYAN MOMIN ................... STUDENT ID : 1302338

#ifndef VCHELPER_H
#define VCHELPER_H

#include "VCParser.h"
#include "LinkedListAPI.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

DateTime* TimeTextDate(char* dateTimeStr);

void freeTimeTextDate(DateTime* dt);

void deleteList(List* list);

int hasNext(ListIterator* it);

bool isValidPropertyName(const char* name);

bool isValidDateTime(const DateTime* dt);

#endif