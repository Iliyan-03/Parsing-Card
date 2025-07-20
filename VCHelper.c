// NAME : ILIYAN MOMIN ................... STUDENT ID : 1302338

#define _POSIX_C_SOURCE 200809L

#include "VCParser.h"
#include "LinkedListAPI.h"
#include "VCHelper.h"
#include <stdlib.h>     
#include <string.h>     
#include <stdbool.h>   
#include <stddef.h>
#include <ctype.h> 

const char* validProperties[] = {
    "FN", "N", "NICKNAME", "PHOTO", "BDAY", "ANNIVERSARY", "GENDER",
    "ADR", "TEL", "EMAIL", "IMPP", "LANG", "TZ", "GEO", "TITLE", "ROLE",
    "LOGO", "ORG", "MEMBER", "RELATED", "CATEGORIES", "NOTE", "PRODID",
    "REV", "SOUND", "UID", "CLIENTPIDMAP", "URL", "KEY", "FBURL",
    "CALADRURI", "CALURI", "BEGIN","VERSION", "END", "SOURCE", "KIND", "XML"
};
const int numValidProperties = sizeof(validProperties) / sizeof(validProperties[0]);

bool isValidPropertyName(const char* name) {
    if (name == NULL) {
        return false;
    }
    for (int i = 0; i < numValidProperties; i++) {
        if (strcmp(name, validProperties[i]) == 0) {
            return true;
        }
    }
    return false;
}

DateTime* TimeTextDate(char* dateTimeStr) {
    if (!dateTimeStr || strlen(dateTimeStr) == 0) {
        return NULL;
    }

    DateTime* dt = malloc(sizeof(DateTime));
    if (!dt) {
        return NULL;
    }

    dt->UTC = false;
    dt->isText = false;
    dt->date = strdup("");
    dt->time = strdup("");  
    dt->text = strdup("");  

    if (!dt->date || !dt->time || !dt->text) {
        freeTimeTextDate(dt);  
        return NULL;
    }

    if (strchr(dateTimeStr, ' ') != NULL) {
        dt->isText = true;
        free(dt->text);  
        dt->text = strdup(dateTimeStr);
        if (!dt->text) {
            freeTimeTextDate(dt);  
            return NULL;
        }
        return dt;
    }

    if (dateTimeStr[0] == 'T') {
        free(dt->time);  
        dt->time = strdup(dateTimeStr + 1);
        if (!dt->time) {
            freeTimeTextDate(dt);  
            return NULL;
        }
        return dt;
    }

    char* timePtr = strchr(dateTimeStr, 'T');
    if (timePtr) {
        size_t dateLen = timePtr - dateTimeStr;
        free(dt->date);  
        dt->date = strndup(dateTimeStr, dateLen);
        free(dt->time);  
        dt->time = strdup(timePtr + 1);

        if (!dt->date || !dt->time) {
            freeTimeTextDate(dt);  
            return NULL;
        }

        if (dt->time[strlen(dt->time) - 1] == 'Z') {
            dt->UTC = true;
            dt->time[strlen(dt->time) - 1] = '\0';
        }
        return dt;
    }

    if (strncmp(dateTimeStr, "--", 2) == 0) {
        free(dt->date);  
        dt->date = strdup(dateTimeStr);
        if (!dt->date) {
            freeTimeTextDate(dt);  
            return NULL;
        }
        return dt;
    }

    free(dt->date);  
    dt->date = strdup(dateTimeStr);
    if (!dt->date) {
        freeTimeTextDate(dt);  
        return NULL;
    }

    return dt;
}

void freeTimeTextDate(DateTime* dt) {
    if (dt) {
        free(dt->date);   
        free(dt->time);   
        free(dt->text); 
        free(dt); 
    }
}

void deleteList(List* list) {
    if (list) {
        Node* current = list->head;
        while (current) {
            deleteProperty(current->data);  
            current = current->next;
        }
        free(list); 
    }
}

int hasNext(ListIterator* it) {
    return it->current != NULL;
}

bool isValidDateTime(const DateTime* dt) {
    if (dt == NULL) {
        return false;
    }

    if (dt->isText) {
        if (dt->text == NULL || strlen(dt->text) == 0) {
            return false; 
        }
        if (dt->UTC || (dt->date != NULL && strlen(dt->date) > 0) || (dt->time != NULL && strlen(dt->time) > 0)) {
            return false; 
        }
        return true;
    } else {
        if (dt->date == NULL && dt->time == NULL) {
            return false; 
        }

        if (dt->date != NULL && strlen(dt->date) > 0) {
            size_t dateLen = strlen(dt->date);
            if (dateLen == 8) {
                for (size_t i = 0; i < dateLen; i++) {
                    if (!isdigit(dt->date[i])) {
                        return false; 
                    }
                }
            } else if (dateLen == 5 && strncmp(dt->date, "--", 2) == 0) {
                for (size_t i = 2; i < dateLen; i++) {
                    if (!isdigit(dt->date[i])) {
                        return false;
                    }
                }
            } else {
                return false;
            }
        }

        if (dt->time != NULL && strlen(dt->time) > 0) {
            size_t timeLen = strlen(dt->time);
            if (timeLen == 6) {
                for (size_t i = 0; i < timeLen; i++) {
                    if (!isdigit(dt->time[i])) {
                        return false; 
                    }
                }
            } else if (timeLen == 7 && dt->time[timeLen - 1] == 'Z') {
                for (size_t i = 0; i < timeLen - 1; i++) {
                    if (!isdigit(dt->time[i])) {
                        return false; 
                    }
                }
                if (!dt->UTC) {
                    return false; 
                }
            } else if (timeLen == 7 && dt->time[0] == 'T') {
                for (size_t i = 1; i < timeLen; i++) {
                    if (!isdigit(dt->time[i])) {
                        return false;
                    }
                }
            } else {
                return false; 
            }
        }

        if (dt->text != NULL && strlen(dt->text) > 0) {
            return false;
        }

        return true;
    }
}

