// NAME : ILIYAN MOMIN ................... STUDENT ID : 1302338

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "VCParser.h"
#include "LinkedListAPI.h"
#include "VCHelper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

VCardErrorCode createCard(char* fileName, Card** obj) {
    if (!fileName || !obj || strlen(fileName) == 0) {
        return INV_FILE;
    }

    const char* ext = strrchr(fileName, '.');
    if (!ext || (strcmp(ext, ".vcf") != 0 && strcmp(ext, ".vcard") != 0)) {
        *obj = NULL;
        return INV_FILE;
    }

    FILE* file = fopen(fileName, "r");
    if (!file) {
        *obj = NULL;
        return INV_FILE;
    }

    Card* newCard = malloc(sizeof(Card));
    if (!newCard) {
        fclose(file);
        return OTHER_ERROR;
    }

    newCard->fn = NULL;
    newCard->optionalProperties = initializeList(propertyToString, deleteProperty, compareProperties);
    newCard->birthday = NULL;
    newCard->anniversary = NULL;

    char line[1024];
    char previousLine[1024] = "";
    int versionFound = 0, fnFound = 0, beginFound = 0, endFound = 0;

    while (fgets(line, sizeof(line), file)) {
        size_t len = strlen(line);

        while ((len > 0) && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[len - 1] = '\0';
            len--;
        }

        if (len == 0) {
            continue;
        }

        if (line[0] == ' ' || line[0] == '\t') {
            if (len > 0) {
                strcat(previousLine, line);
            }
            continue;
        }

        if (previousLine[0] != '\0') {
            if (strcmp(previousLine, "BEGIN:VCARD") == 0) {
                beginFound = 1;
            } else if (strncmp(previousLine, "VERSION:4.0", 11) == 0) {
                versionFound = 1;
            } else if (strcmp(previousLine, "END:VCARD") == 0) {
                endFound = 1;
                break;
            } else {
                char* colon = strchr(previousLine, ':');
                if (!colon) {
                    fclose(file);
                    deleteCard(newCard);
                    return INV_PROP;
                } else if (colon == previousLine) {
                    fclose(file);
                    deleteCard(newCard);
                    return INV_PROP;
                } else if (*(colon + 1) == '\0') {
                    fclose(file);
                    deleteCard(newCard);
                    return INV_PROP;
                }

                size_t propNameLen = strcspn(previousLine, ";:");
                char* propName = strndup(previousLine, propNameLen);

                char* paramStr = strchr(previousLine, ';');
                char* propValue = colon + 1;

                Property* newProperty = malloc(sizeof(Property));
                if (!newProperty) {
                    free(propName);
                    fclose(file);
                    deleteCard(newCard);
                    return OTHER_ERROR;
                }

                newProperty->name = propName;

                char* dot = strchr(propName, '.');
                if (dot) {
                    size_t groupLen = dot - propName;
                    newProperty->group = strndup(propName, groupLen);
                    newProperty->name = strdup(dot + 1);
                    free(propName);
                } else {
                    newProperty->group = strdup("");
                    newProperty->name = propName;
                }

                newProperty->values = initializeList(valueToString, deleteValue, compareValues);

                char* value = strdup(propValue);
                if (!value) {
                    deleteProperty(newProperty);
                    fclose(file);
                    deleteCard(newCard);
                    return OTHER_ERROR;
                }
                insertBack(newProperty->values, value);

                newProperty->parameters = initializeList(parameterToString, deleteParameter, compareParameters);

                while (paramStr && paramStr < colon) {
                    paramStr++;
                    char* equals = strchr(paramStr, '=');
                    if (!equals || equals > colon) {
                        deleteProperty(newProperty);
                        fclose(file);
                        deleteCard(newCard);
                        return INV_PROP;
                    }

                    size_t paramNameLen = equals - paramStr;
                    char* paramName = strndup(paramStr, paramNameLen);
                    char* paramValue = strndup(equals + 1, strchr(equals, ';') ? strchr(equals, ';') - equals - 1 : colon - equals - 1);

                    if (paramValue == NULL || strlen(paramValue) == 0) {
                        free(paramName);
                        deleteProperty(newProperty);
                        fclose(file);
                        deleteCard(newCard);
                        return INV_PROP;
                    }

                    Parameter* newParam = malloc(sizeof(Parameter));
                    if (!newParam) {
                        free(paramName);
                        free(paramValue);
                        deleteProperty(newProperty);
                        fclose(file);
                        deleteCard(newCard);
                        return OTHER_ERROR;
                    }

                    newParam->name = paramName;
                    newParam->value = paramValue;
                    insertBack(newProperty->parameters, newParam);

                    paramStr = strchr(equals, ';');
                }

                if (strcmp(newProperty->name, "FN") == 0) {
                    if (!fnFound) {
                        newCard->fn = newProperty;
                        fnFound = 1;
                    } else {
                        insertBack(newCard->optionalProperties, newProperty);
                    }
                } else if (strcmp(newProperty->name, "BDAY") == 0) {
                    newCard->birthday = TimeTextDate(propValue);
                    if (!newCard->birthday || (strlen(newCard->birthday->date) == 0 && strlen(newCard->birthday->time) == 0 && !newCard->birthday->isText)) {
                        deleteProperty(newProperty);
                        fclose(file);
                        deleteCard(newCard);
                        return INV_PROP;
                    }
                } else if (strcmp(newProperty->name, "ANNIVERSARY") == 0) {
                    newCard->anniversary = TimeTextDate(propValue);
                } else {
                    insertBack(newCard->optionalProperties, newProperty);
                }
            }

            previousLine[0] = '\0';
        }

        strncpy(previousLine, line, sizeof(previousLine));
    }

    if (previousLine[0] != '\0') {
        if (strcmp(previousLine, "BEGIN:VCARD") == 0) {
            beginFound = 1;
        } else if (strncmp(previousLine, "VERSION:4.0", 11) == 0) {
            versionFound = 1;
        } else if (strcmp(previousLine, "END:VCARD") == 0) {
            endFound = 1;
        } else {
            char* colon = strchr(previousLine, ':');
            if (!colon) {
                fclose(file);
                deleteCard(newCard);
                return INV_PROP;
            } else if (colon == previousLine) {
                fclose(file);
                deleteCard(newCard);
                return INV_PROP;
            } else if (*(colon + 1) == '\0') {
                fclose(file);
                deleteCard(newCard);
                return INV_PROP;
            }

            size_t propNameLen = strcspn(previousLine, ";:");
            char* propName = strndup(previousLine, propNameLen);

            char* paramStr = strchr(previousLine, ';');
            char* propValue = colon + 1;

            Property* newProperty = malloc(sizeof(Property));
            if (!newProperty) {
                free(propName);
                fclose(file);
                deleteCard(newCard);
                return OTHER_ERROR;
            }

            newProperty->name = propName;

            char* dot = strchr(propName, '.');
            if (dot) {
                size_t groupLen = dot - propName;
                newProperty->group = strndup(propName, groupLen);
                newProperty->name = strdup(dot + 1);
                free(propName);
            } else {
                newProperty->group = strdup("");
                newProperty->name = propName;
            }

            newProperty->values = initializeList(valueToString, deleteValue, compareValues);

            char* value = strdup(propValue);
            if (!value) {
                deleteProperty(newProperty);
                fclose(file);
                deleteCard(newCard);
                return OTHER_ERROR;
            }
            insertBack(newProperty->values, value);

            newProperty->parameters = initializeList(parameterToString, deleteParameter, compareParameters);

            while (paramStr && paramStr < colon) {
                paramStr++;
                char* equals = strchr(paramStr, '=');
                if (!equals || equals > colon) {
                    deleteProperty(newProperty);
                    fclose(file);
                    deleteCard(newCard);
                    return INV_PROP;
                }

                size_t paramNameLen = equals - paramStr;
                char* paramName = strndup(paramStr, paramNameLen);
                char* paramValue = strndup(equals + 1, strchr(equals, ';') ? strchr(equals, ';') - equals - 1 : colon - equals - 1);

                if (paramValue == NULL || strlen(paramValue) == 0) {
                    free(paramName);
                    deleteProperty(newProperty);
                    fclose(file);
                    deleteCard(newCard);
                    return INV_PROP;
                }

                Parameter* newParam = malloc(sizeof(Parameter));
                if (!newParam) {
                    free(paramName);
                    free(paramValue);
                    deleteProperty(newProperty);
                    fclose(file);
                    deleteCard(newCard);
                    return OTHER_ERROR;
                }

                newParam->name = paramName;
                newParam->value = paramValue;
                insertBack(newProperty->parameters, newParam);

                paramStr = strchr(equals, ';');
            }

            if (strcmp(newProperty->name, "FN") == 0) {
                if (!fnFound) {
                    newCard->fn = newProperty;
                    fnFound = 1;
                } else {
                    insertBack(newCard->optionalProperties, newProperty);
                }
            } else if (strcmp(newProperty->name, "BDAY") == 0) {
                newCard->birthday = TimeTextDate(propValue);
                if (!newCard->birthday || (strlen(newCard->birthday->date) == 0 && strlen(newCard->birthday->time) == 0 && !newCard->birthday->isText)) {
                    deleteProperty(newProperty);
                    fclose(file);
                    deleteCard(newCard);
                    return INV_PROP;
                }
            } else if (strcmp(newProperty->name, "ANNIVERSARY") == 0) {
                newCard->anniversary = TimeTextDate(propValue);
            } else {
                insertBack(newCard->optionalProperties, newProperty);
            }
        }
    }

    fclose(file);

    if (!versionFound || !beginFound || !endFound || !fnFound) {
        deleteCard(newCard);
        return INV_CARD;
    }

    *obj = newCard;
    return OK;
}

char* cardToString(const Card* obj) {
    if (obj == NULL) {
        return NULL;
    }

    size_t len = 0;
    size_t capacity = 512;
    char* result = (char*)malloc(capacity);
    if (result == NULL) {
        return NULL;
    }

    if (obj->fn != NULL && obj->fn->name != NULL) {
        int written = snprintf(result + len, capacity - len, "FN: %s\n", obj->fn->name);
        if (written < 0) {
            free(result);
            return NULL;
        }
        len += written;
    } else {
        int written = snprintf(result + len, capacity - len, "FN: <none>\n");
        if (written < 0) {
            free(result);
            return NULL;
        }
        len += written;
    }

    // Handle properties
    if (obj->optionalProperties != NULL && getLength(obj->optionalProperties) > 0) {
        ListIterator it = createIterator(obj->optionalProperties);
        void* property;
        while ((property = nextElement(&it))) {
            char* propStr = propertyToString(property);
            if (propStr != NULL) {
                int written = snprintf(result + len, capacity - len, "Property: %s\n", propStr);
                free(propStr);
                if (written < 0) {
                    free(result);
                    return NULL;
                }
                len += written;

                if (len >= capacity - 100) {
                    capacity *= 2;
                    char* temp = realloc(result, capacity);
                    if (temp == NULL) {
                        free(result);
                        return NULL;
                    }
                    result = temp;
                }
            }
        }
    } else {
        int written = snprintf(result + len, capacity - len, "No additional properties.\n");
        if (written < 0) {
            free(result);
            return NULL;
        }
        len += written;
    }

    return result;
}



void deleteCard(Card* obj) {
    if (!obj) {
        return;
    }

    if (obj->anniversary) {
        freeTimeTextDate(obj->anniversary);
    }

    if (obj->birthday) {
        freeTimeTextDate(obj->birthday);
    }

    if (obj->fn) {
        if (obj->fn->name) {
            free(obj->fn->name);
        }

        if (obj->fn->group) {
            free(obj->fn->group);
        }

        freeList(obj->fn->values);

        freeList(obj->fn->parameters);

        free(obj->fn);
    }

    freeList(obj->optionalProperties);

    free(obj);
}



char* errorToString(VCardErrorCode err) {
    switch (err) {
        case OK: return "OK";
        case INV_FILE: return "Invalid File";
        case INV_CARD: return "Invalid Card";
        case INV_PROP: return "Invalid Property";
        case INV_DT: return "Invalid Date-Time";
        case WRITE_ERROR: return "Write Error";
        case OTHER_ERROR: return "Other Error";
        default: return "Unknown Error";
    }
}

void deleteProperty(void* toBeDeleted) {
    if (toBeDeleted) {
        Property* prop = (Property*)toBeDeleted;

        if (prop->name) {
            free(prop->name);
        }
        
        if (prop->group && strcmp(prop->group, "") != 0) {
            free(prop->group);
        }

        freeList(prop->parameters);
        freeList(prop->values);

        free(prop);
    }
}



int compareProperties(const void* first, const void* second) {
    Property* prop1 = (Property*)first;
    Property* prop2 = (Property*)second;
    return strcmp(prop1->name, prop2->name);
}

char* propertyToString(void* prop) {
    if (!prop) {
        return NULL;
    }

    Property* property = (Property*)prop;
    if (!property->name || !property->values) {
        return NULL;
    }

    size_t size = 512;
    size_t len = 0;
    char* result = malloc(size);
    if (!result) {
        return NULL;
    }

    ListIterator it = createIterator(property->values);
    void* elem;
    while ((elem = nextElement(&it))) {
        char* value = (char*)elem;

        size_t remaining = size - len;
        size_t valueLen = strlen(value) + 2;
        if (valueLen >= remaining) {
            size *= 2;
            result = realloc(result, size);
            if (!result) {
                return NULL;
            }
        }

        len += snprintf(result + len, size - len, " %s;", value);
    }

    return result;
}
void deleteParameter(void* toBeDeleted) {
    if (toBeDeleted) {
        Parameter* param = (Parameter*)toBeDeleted;
        free(param->name);
        free(param->value);
        free(param);
    }
}

int compareParameters(const void* first, const void* second) {
    Parameter* param1 = (Parameter*)first;
    Parameter* param2 = (Parameter*)second;
    return strcmp(param1->name, param2->name);
}

char* parameterToString(void* param) {
    if (!param){
        return NULL;
    }

    size_t size = 256;
    char* result = malloc(size);
    if (!result){
        return NULL;
    }
    return result;
}

void deleteValue(void* toBeDeleted) {
    if (toBeDeleted) {
        free((char*)toBeDeleted);
    }
}

int compareValues(const void* first, const void* second) {
    const char* val1 = (const char*)first;
    const char* val2 = (const char*)second;
    return strcmp(val1, val2);
}

char* valueToString(void* val) {
    if (!val){
        return NULL;
    }
    char* newStr = (char*)malloc(strlen((const char*)val) + 1);
    if (newStr) {
        strcpy(newStr, (const char*)val);
    }
    return newStr;
}


void deleteDate(void* toBeDeleted) {
    if (toBeDeleted) {
        DateTime* date = (DateTime*)toBeDeleted;
        free(date->date);
        free(date->time);
        free(date->text);
        free(date);
    }
}

int compareDates(const void* first, const void* second) {
    (void)first;
    (void)second;
    
    return 0; 
}

char* dateToString(void* date) {
    if (!date) {
        return NULL;
    }
    size_t size = 256;
    char* result = malloc(size);
    
    if (!result){
        return NULL;
    } 

    return result;
}

VCardErrorCode writeCard(const char* fileName, const Card* obj) {
    if (!fileName || !obj || strlen(fileName) == 0) {
        return WRITE_ERROR;
    }

    const char* ext = strrchr(fileName, '.');
    if (!ext || (strcmp(ext, ".vcf") != 0 && strcmp(ext, ".vcard") != 0)) {
        return WRITE_ERROR;
    }

    FILE* file = fopen(fileName, "w");
    if (!file) {
        return WRITE_ERROR;
    }

    fprintf(file, "BEGIN:VCARD\r\n");
    fprintf(file, "VERSION:4.0\r\n");

    // Write FN (Formatted Name) property
    if (obj->fn && obj->fn->values) {
        char* fnValue = (char*)getFromFront(obj->fn->values);
        if (fnValue) {
            fprintf(file, "FN:%s\r\n", fnValue);
        }
    }

    ListIterator it = createIterator(obj->optionalProperties);
    void* elem;
    while ((elem = nextElement(&it))) {
        Property* prop = (Property*)elem;
        if (prop->name && prop->values) {
            fprintf(file, "%s", prop->name);

            // Write parameters
            ListIterator paramIt = createIterator(prop->parameters);
            void* paramElem;
            while ((paramElem = nextElement(&paramIt))) {
                Parameter* param = (Parameter*)paramElem;
                fprintf(file, ";%s=%s", param->name, param->value);
            }

            // Write values
            fprintf(file, ":");
            ListIterator valIt = createIterator(prop->values);
            void* valElem;
            int first = 1;
            while ((valElem = nextElement(&valIt))) {
                if (!first) {
                    fprintf(file, ";");
                }
                fprintf(file, "%s", (char*)valElem);
                first = 0;
            }
            fprintf(file, "\r\n");
        }
    }

    // Write BDAY (Birthday) property
    if (obj->birthday) {    
        if (obj->birthday->isText) {
            fprintf(file, "BDAY;VALUE=text:%s\r\n", obj->birthday->text);
        } else {
            if (obj->birthday->date && strlen(obj->birthday->date) > 0) {
                if (obj->birthday->time && strlen(obj->birthday->time) > 0) {
                    fprintf(file, "BDAY:%sT%s\r\n", obj->birthday->date, obj->birthday->time);
                } else {
                    fprintf(file, "BDAY:%s\r\n", obj->birthday->date);
                }
            } else if (obj->birthday->time && strlen(obj->birthday->time) > 0) {
                fprintf(file, "BDAY:T%s\r\n", obj->birthday->time);
            } else {
                fprintf(file, "BDAY:\r\n");
            }
        }
    }

    // Write ANNIVERSARY property
    if (obj->anniversary) {
        if (obj->anniversary->isText) {
            fprintf(file, "ANNIVERSARY;VALUE=text:%s\r\n", obj->anniversary->text);
        } else {
            fprintf(file, "ANNIVERSARY:%s%s%s\r\n",
                    obj->anniversary->date,
                    obj->anniversary->time ? "T" : "",
                    obj->anniversary->time ? obj->anniversary->time : "");
        }
    }

    // Write END
    fprintf(file, "END:VCARD\r\n");

    if (fclose(file) != 0) {
        return WRITE_ERROR;
    }

    return OK;
}

VCardErrorCode validateCard(const Card* obj) {
    if (obj == NULL) {
        return INV_CARD;
    }
    if (obj->fn == NULL) {
        return INV_CARD;
    }

    if (obj->fn->name == NULL) {
        return INV_CARD;
    }
    if (strcmp(obj->fn->name, "FN") != 0) {
        return INV_CARD;
    }
    if (obj->fn->values == NULL) {
        return INV_CARD;
    }

    if (obj->optionalProperties == NULL) {
        return INV_CARD;
    }

    int kindCount = 0;
    int xmlCount = 0;
    int nCount = 0;
    int photoCount = 0;
    int genderCount = 0;
    int adrCount = 0;
    int telCount = 0;
    int emailCount = 0;
    int imppCount = 0;
    int langCount = 0;
    int tzCount = 0;
    int geoCount = 0;
    int titleCount = 0;
    int roleCount = 0;
    int logoCount = 0;
    int orgCount = 0;
    int memberCount = 0;
    int relatedCount = 0;
    int categoriesCount = 0;
    int noteCount = 0;
    int prodidCount = 0;
    int revCount = 0;
    int soundCount = 0;
    int uidCount = 0;
    int clientpidmapCount = 0;
    int urlCount = 0;
    int keyCount = 0;
    int fburlCount = 0;
    int caladruriCount = 0;
    int caluriCount = 0;

    ListIterator iter = createIterator(obj->optionalProperties);
    Property* prop;
    while ((prop = (Property*)nextElement(&iter)) != NULL) {
        if (!isValidPropertyName(prop->name)) {
            return INV_PROP;
        }

        if (strcmp(prop->name, "KIND") == 0) {
            kindCount++;
            if (kindCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "XML") == 0) {
            xmlCount++;
            if (xmlCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "N") == 0) {
            nCount++;
            if (nCount > 5) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "NICKNAME") == 0) {
            if (getLength(prop->values) < 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "PHOTO") == 0) {
            photoCount++;
            if (photoCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "GENDER") == 0) {
            genderCount++;
            if (genderCount > 2) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "ADR") == 0) {
            adrCount++;
            if (adrCount > 7) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "TEL") == 0) {
            telCount++;
            if (telCount > 2) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "EMAIL") == 0) {
            emailCount++;
            if (emailCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "IMPP") == 0) {
            imppCount++;
            if (imppCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "LANG") == 0) {
            langCount++;
            if (langCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "TZ") == 0) {
            tzCount++;
            if (tzCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "GEO") == 0) {
            geoCount++;
            if (geoCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "TITLE") == 0) {
            titleCount++;
            if (titleCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "ROLE") == 0) {
            roleCount++;
            if (roleCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "LOGO") == 0) {
            logoCount++;
            if (logoCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "ORG") == 0) {
            orgCount++;
            if (getLength(prop->values) < 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "MEMBER") == 0) {
            memberCount++;
            if (memberCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "RELATED") == 0) {
            relatedCount++;
            if (relatedCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "CATEGORIES") == 0) {
            categoriesCount++;
            if (categoriesCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "NOTE") == 0) {
            noteCount++;
            if (noteCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "PRODID") == 0) {
            prodidCount++;
            if (prodidCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "REV") == 0) {
            revCount++;
            if (revCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "SOUND") == 0) {
            soundCount++;
            if (soundCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "UID") == 0) {
            uidCount++;
            if (uidCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "CLIENTPIDMAP") == 0) {
            clientpidmapCount++;
            if (getLength(prop->values) < 2) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "URL") == 0) {
            urlCount++;
            if (urlCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "KEY") == 0) {
            keyCount++;
            if (keyCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "FBURL") == 0) {
            fburlCount++;
            if (fburlCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "CALADRURI") == 0) {
            caladruriCount++;
            if (caladruriCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "CALURI") == 0) {
            caluriCount++;
            if (caluriCount > 1) {
                return INV_PROP;
            }
        }

        if (strcmp(prop->name, "VERSION") == 0) {
            return INV_CARD;
        }

        if (prop->values == NULL || getLength(prop->values) == 0) {
            return INV_PROP;
        }
    }

    if (obj->birthday) {

        if (!isValidDateTime(obj->birthday)) {
            return INV_DT;
        }
    }

    if (obj->anniversary) {

        if (!isValidDateTime(obj->anniversary)) {
            return INV_DT;
        }
    }

    return OK;
}