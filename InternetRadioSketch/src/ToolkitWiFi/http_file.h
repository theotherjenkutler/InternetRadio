//
// http_file.h

#ifndef _HTTP_FILE_H_
#define _HTTP_FILE_H_

#include "ToolkitWiFi_Client.h"

void http_handlePostRequest(ToolkitWiFi_Client *twfc,
    char *buffer, size_t actual, size_t max_size);

void http_handleGetRequest(ToolkitWiFi_Client *twfc, const char *path,
    const char *default_index, size_t default_index_size,
    char *buffer, size_t max_size);

#endif

//
// END OF http_file.h
