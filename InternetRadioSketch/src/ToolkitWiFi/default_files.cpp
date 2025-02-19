//
// default_files.cpp

#include <Arduino.h>

typedef struct {
    const char name[32];
    const char *data;      // data is NULL terminated
} default_file_data;

//---------------------------------------------------------------
//
// FILE DATA GOES IN HERE
//

//
// index.html

// static const char data_index_html[] = {
//     0x00
// };

#include "default_files/index_html.h"

static default_file_data default_index_html = {
    "index.html", data_index_html
};

//
// toolkit.js

#include "default_files/toolkit_js.h"

static default_file_data default_toolkit_js = {
    "toolkit.js", data_toolkit_js
};

//
// toolkit.css

#include "default_files/toolkit_css.h"

static default_file_data default_toolkit_css = {
    "toolkit.css", data_toolkit_css
};

//
// upload.html

#include "default_files/upload_html.h"

static default_file_data default_upload_html = {
    "upload.html", data_upload_html
};

//
// kiosk.html

#include "default_files/kiosk_html.h"

static default_file_data default_kiosk_html = {
    "kiosk.html", data_kiosk_html
};

//---------------------------------------------------------------
//
// Table of file data
//

static default_file_data *files[] = {
    &default_index_html,
    &default_toolkit_js,
    &default_toolkit_css,
    &default_upload_html,
    &default_kiosk_html,
    0
};

const char *default_files_find(const char *path, size_t *length)
{
    default_file_data **list = files;
    while (*list) {
        if (0 == strcmp((*list)->name, path)) {
            *length = strlen((*list)->data);
            return (*list)->data;
        }
        list++;
    }
    *length = 0;
    return 0;
}

//
// END OF default_files.cpp
