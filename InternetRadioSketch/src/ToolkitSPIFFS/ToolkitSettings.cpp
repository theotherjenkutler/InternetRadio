//
// ToolkitSettings.cpp

#include "ToolkitSettings.h"
#include "../parsingTools.h"

//
// The public stuff we want to use in our Sketch

// (1) load settings from a buffer and create the object list
void SettingItem::loadSettingsFromBuffer(const char *buffer, size_t buffer_length)
{
    if (buffer_length > 0) {
        //buffer[buffer_length-1] = 0; // make it safe for c-str functions
        const char *text = buffer;
        const char *end = buffer + buffer_length - 1;
        while (text) {
            text = makeSettingFromParseableText(text, end);
        }
    }
}

// (2) save everything in the object list to a text buffer
size_t SettingItem::saveAll(char *buffer, size_t buffer_length)
{
    SettingItem *current = first;
    size_t total = 0;
    size_t actual;
    while (current) {
        actual = current->save(buffer, buffer_length);
        if (0==actual) {
            return total; // we have run out of space!
        }
        buffer += actual;
        total += actual;
        buffer_length -= actual;
        current = current->next;
    }
    return total;
}

// (3) destroy the entire object list, free all the memory
void SettingItem::destroyAll()
{
    while (first) {
        delete first;
    }
}

// (4) fetch your settings by key name
char* SettingItem::findString(const char *name)
{
    SettingItem *result;
    if (result = find(name)) {
        return result->value;
    }
    return NULL;
}

uint16_t SettingItem::findUInt(const char *name, uint16_t default_value)
{
    SettingItem *result;
    if (result = find(name)) {
        return (uint16_t) atoi(result->value);
    }
    return default_value;
}

float SettingItem::findFloat(const char *name, float default_value)
{
    SettingItem *result;
    if (result = find(name)) {
        return (float) atof(result->value);
    }
    return default_value;
}

const char* SettingItem::parseSetting(const char *text,
    const char **outName, const char **outValue)
{
    static char name[ITEM_MAX_NAME_STRING];
    static char value[ITEM_MAX_VALUE_STRING];
    *outName = NULL;
    *outValue = NULL;

    text = clearWhitespace(text);   // will point to name or '\n' or '\0'
    if (isTerminator(text)) {
        return text;   // there was no settings name in the text
    }
    const char *end = findEndOfString(text);
    // name starts at text and ends and end
    uint16_t length = end - text;
    if (length > (ITEM_MAX_NAME_STRING-1)) {
        length = ITEM_MAX_NAME_STRING-1;
    }
    for (uint16_t i = 0; i < length; i++) {
        name[i] = text[i];
    } name[length] = 0;
    *outName = name;

    text = clearWhitespace(end); // may return end if there is no value
    if (isTerminator(text)) {
        return text;
    }

    end = findEndOfString(text);
    length = end - text;
    if (length > (ITEM_MAX_VALUE_STRING-1)) {
        length = ITEM_MAX_VALUE_STRING-1;
    }
    for (uint16_t i = 0; i < length; i++) {
        value[i] = text[i];
    } value[length] = 0;
    *outValue = value;
    return end;
}

void SettingItem::updateOrAdd(const char *name, const char *inValue)
{
    // find the setting if it exists
    SettingItem *si = find(name);
    if (si) { // copy new value
        si->value[0] = 0;
        if (inValue) {
            strncpy(si->value,inValue,ITEM_MAX_VALUE_STRING-1);
            si->value[ITEM_MAX_VALUE_STRING-1] = 0;
        }
    } else {
        si = new SettingItem(name, inValue);
    }
}

// (5) log setting to the console
void SettingItem::printSettingsToSerial()
{
    SettingItem *current = first;
    while (current) {
        Serial.printf("%s = %s\n", current->name, current->value);
        current = current->next;
    }
}

//
// NON-STATIC PUBLIC METHODS .. so we can create/delete on the fly

SettingItem::SettingItem(const char *name, const char *inValue)
{
    add(name);
    if (inValue) {
        strncpy(value,inValue,ITEM_MAX_VALUE_STRING-1);
        value[ITEM_MAX_VALUE_STRING-1] = 0;
    }
}

SettingItem::~SettingItem()
{
    remove();
}

//
// NON-STATIC HIDDEN METHODS

void SettingItem::add(const char *inName)
{
	if (NULL == first) {
		first = this;
        last = this;
		next = previous = NULL;
		}
	else {
		next = NULL;
		previous = last;
		last->next = this;
		last = this;
	}
    strncpy(name,inName,ITEM_MAX_NAME_STRING-1);
    name[ITEM_MAX_NAME_STRING-1] = 0;
    value[0] = 0;
}

void SettingItem::remove()
{
	if (NULL != previous) { previous->next = next; }
	if (NULL != next) { next->previous = previous; }
	if (this == last) { last = this->previous; }
	if (this == first) { first = this->next; }
	previous = next = NULL;
}

size_t SettingItem::save(char *buffer, size_t buffer_length)
{ // name = value\n
    size_t result = 0;
    size_t required = strlen(name) + 4 + strlen(value);
    if (required < buffer_length) {
        result = sprintf(buffer, "%s = %s\n", name, value);
    }
    return result;
}

//
// STATIC HIDDEN

SettingItem* SettingItem::first = NULL;
SettingItem* SettingItem::last = NULL;

SettingItem* SettingItem::find(const char *name)
{
    SettingItem *current = first;
    while (current) {
        // compare, if the name is the same then return current
        if (0 == strcmp(current->name, name)) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// *end points to the last valid character in *text
const char* SettingItem::makeSettingFromParseableText(
    const char *text, const char *end)
{   // return pointer to end of text
    if ('#'==*text) { // treat this line as a comment
        text = skipToTerminatar(text); // \n\r\0
        if (*text) { // if a \n or \r is found, clear all newlines
            text = clearNewlines(text);
        }
        return checkForEnd(text,end);
    }

    const char *name;
    const char *value;
    text = parseSetting(text, &name, &value);
    // text now points at a terminator \n\r\0
    text = clearNewlines(text);

    if (NULL==name) { // blank line or end of buffer
        return checkForEnd(text,end);
    }
    SettingItem *si = new SettingItem(name, value);
    return checkForEnd(text,end);
}

//
// END OF ToolkitSettings.cpp
