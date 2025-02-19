//
// http_file.cpp

#include <Arduino.h>
#include "../parsingTools.h"
#include "ToolkitWiFi_Client.h"
#include "../ToolkitFiles/ToolkitFiles.h"

//------------------------------------------------------------------
//
// Kiosk mode forces all .html extensions to become kiosk.html
//

static boolean isKioskOn = false;

void http_turnOnKioskMode(int onNotOff)
{
    isKioskOn = onNotOff;
}

//------------------------------------------------------------------
//
// HTTP RESPONSE .. send header and content
//

enum {
    RESPONSE_OKAY       = 200,
    RESPONSE_CREATED    = 201,
    RESPONSE_NOT_FOUND  = 404
};

static void http_send_header(ToolkitWiFi_Client *twfc,
    size_t content_length, uint32_t response, const char *mime)
{
    const char nocache[] =
        "Cache-Control: no-cache, no-store, must-revalidate\n"
        "Pragma: no-cache\nExpires: 0\n";
	twfc->client->printf(
        "HTTP/1.1 %u OK\nContent-Length: %u\nContent-Type: %s; charset=UTF-8\n%s\n",
        response, content_length, mime, nocache);
}

static void http_send_data_chunk(ToolkitWiFi_Client *twfc,
    const char *data, size_t size)
{
    twfc->client->write(data,size);
}

static void http_send(ToolkitWiFi_Client *twfc, const char *data, size_t size,
    uint32_t response, const char *type)
{
    const char nocache[] =
        "Cache-Control: no-cache, no-store, must-revalidate\n"
        "Pragma: no-cache\nExpires: 0\n";
	twfc->client->printf(
        "HTTP/1.1 %u OK\nContent-Length: %u\nContent-Type: %s; charset=UTF-8\n%s\n",
        response, size, type, nocache);
	twfc->client->write(data,size);
}

static void http_send_201(ToolkitWiFi_Client *twfc)
{
    const char okay[] = "201 File was uploaded!\n";
    http_send(twfc, okay, strlen(okay), RESPONSE_CREATED, "text/html");
}

static void http_send_404(ToolkitWiFi_Client *twfc)
{
    const char unknown[] = "404 Can't find the file!\n";
    http_send(twfc, unknown, strlen(unknown), RESPONSE_NOT_FOUND, "text/html");
}

//------------------------------------------------------------------
//
// HANDLE POST REQUEST
//

/*
POST /upload HTTP/1.1
Host: 192.168.4.1
Connection: keep-alive
Content-Length: 188
Cache-Control: max-age=0
Upgrade-Insecure-Requests: 1
Origin: http://192.168.4.1
Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryoKkPKmBNiJBy6KPu
*/

#define MAX_FILENAME_LENGTH 32
#define MAX_BOUNDARY_LENGTH 72

inline const char *copyBoundary(char *dst, const char *buffer)
{
    uint32_t boundary = MAX_BOUNDARY_LENGTH;
    while ((*buffer) && boundary) {
        switch (*buffer) {
            case '\n' :
            case '\r' :
                *dst = 0;
                buffer++;
                return buffer;
                break;
            default :
                *dst++ = *buffer++;
                break;
        }
        boundary--;
    }
    return NULL;
}

static char filename[MAX_FILENAME_LENGTH+2] = "/";
static char boundary[MAX_BOUNDARY_LENGTH+2] = "";
static char match[MAX_BOUNDARY_LENGTH+2] = "";
static uint32_t matchpoint = 0;

static const char *postfile_findContent(const char *buffer,
    const char **content)
{

    *content = NULL;
    filename[0] = '/';
    filename[1] = 0;
    boundary[0] = 0;
    match[0] = 0;
    matchpoint = 0;

    // (0) make sure it is one off
    // POST /
    // POST /upload
    // POST /upload.html
    buffer = strstr(buffer, "/");
    if (NULL == buffer) {
        return NULL;
    }
    if (' ' != buffer[1]) {
        buffer = strstr(buffer, "upload");
        if (NULL == buffer) {
            return NULL;
        }
    }

    // (1) We already know it is a POST request ..
    // We also know that buffer is NULL terminated by the ToolkitWiFi code
    // find two endlines in a row
    buffer = findTwoEndlines(buffer);
    if (NULL == buffer) {
        return NULL;
    }

    // (2) The boundary string is the next line
    buffer = copyBoundary(boundary, buffer);
    if (NULL == buffer) {
        return NULL;
    }

    // (3) Find the "Content-Disposition:" header
    buffer = strstr(buffer, "Content-Disposition");
    if (NULL == buffer) {
        return NULL;
    }
    buffer += 19;   // length of Content-Disposition

    // (4) find the "filename"
    buffer = strstr(buffer, "filename");
    if (NULL == buffer) {
        return NULL;
    }
    buffer += 8;    // length of filename

    buffer = strstr(buffer, "\"");
    if (NULL == buffer) {
        return NULL;
    }
    buffer++;   // buffer now points at the filename string

    const char *end = strstr(buffer, "\"");
    if (NULL == end) {
        return NULL;
    }
    size_t length = end - buffer;
    if (length > MAX_FILENAME_LENGTH) {
        return NULL;
    }
    strncpy(&filename[1], buffer, length);
    filename[length+1] = 0;

    buffer = end;

    // (5) find two endlines in a row
    buffer = findTwoEndlines(buffer);
    if (NULL == buffer) {
        return NULL;
    }

    // (6) NOW we have the start of the content
    *content = buffer;
    return (const char *) filename;
}

inline boolean doesItMatch(const char c)
{
    if (c==boundary[matchpoint]) {
        match[matchpoint] = c;
        matchpoint++;
        return true;
    }
    return false;
}

inline boolean matchIsComplete() {
    return (0==boundary[matchpoint]);
}

inline boolean areThereMatchChars() {
    return (0!=matchpoint);
}

// if we are false and match is not complete
// then we want to move all the buffered match characters
// into our output buffer

// this returns false (have not finished) or true (have finished)
// to and from should be the same size
static boolean postfile_addContent(File *f, const char *buffer, size_t size)
{
    const char *from = buffer;
    size_t used = 0;
    while (size) {
        if (doesItMatch(*from)) {
            // *from is now in the match buffer
            // write the used buffer to file and clear it
            if (used) {
                f->write((uint8_t *) buffer, used);
                used = 0;
            }
            if (matchIsComplete()) { //  then we are done
                // TODO: write out used characters
                f->write((uint8_t *) buffer, used);
                return false; // nohing else to do
            }
            from++; // from is in the match buffer
            size--; // size decreases
            // buffer points to the head of the data we have been parsing
            // used stays the same, since we moved the char into the match buffer
        } else { // the character is not a match and is not in the match buffer
            if (areThereMatchChars()) {
                // if there is anything in the match buffer
                // then it needs to go into the file
                f->write((uint8_t *) match, matchpoint);
                matchpoint = 0; // reset the match buffer
                buffer = from;  // buffer points to 1st char after the match
            }
            from++; // from is in our 'unmatched' buffer
            used++; // buffer has 1 more used char
            size--; // size decreases
        }
    }
    if (used) {
        f->write((uint8_t *) buffer, used);
    }
    return true;
}

/*

REQUEST HEADERS

POST / HTTP/1.1
Host: 192.168.1.76
Connection: keep-alive
Content-Length: 1081
Cache-Control: max-age=0
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_6) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/103.0.0.0 Safari/537.36
Origin: http://192.168.1.76
Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryDM6Hu5pIA1Abecvp
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng;q=0.8,application/signed-exchange;v=b3;q=0.9
Referer: http://192.168.1.76/
Accept-Encoding: gzip, deflate
Accept-Language: en-US,en;q=0.9
\n\n

BOUNDARY

------WebKitFormBoundaryDM6Hu5pIA1Abecvp
Content-Disposition: form-data; name="file"; filename="_git_manual.txt"
Content-Type: text/plain
\n\n

CONTENT

BOUNDARY

------WebKitFormBoundaryDM6Hu5pIA1Abecvp--

*/

void http_handlePostRequest(ToolkitWiFi_Client *twfc,
    char *buffer, size_t actual, size_t max_size)
{
    const char *content = NULL;
    size_t remaining = 0;
    const char *filename = NULL;

    filename = postfile_findContent(buffer, &content);

    if ((NULL==filename) || (0==filename[1])) {
        twfc->closeClient();
        return;
    }

    File f = ToolkitFiles::fileOpen(filename, FILE_WRITE);
    if (!f) {
        Serial.printf("Error creating file %s\n", filename);
        twfc->closeClient();
        return;
    }

    boolean keepgoing = true;
    remaining = actual - (content - buffer);

    if (remaining) {
        keepgoing = postfile_addContent(&f, content, remaining);
    }

    int32_t tryagain = 4;

    // read additional chuncks of data and save them to file
    while (keepgoing && (0!=tryagain)) {
        // delay and try to load more bytes
        vTaskDelay(portTICK_PERIOD_MS * 20);
        size_t avail;
        while ((avail = twfc->client->available()) > 0) {
            if (avail > max_size) {
                avail = max_size;
            }
            twfc->client->readBytes(buffer, avail);
            // we still want to read any bytes after the file data
            // so we keep readBytes(..) from the client, but
            // only write to the file if keepgoing==true
            if (keepgoing) {
                keepgoing = postfile_addContent(&f, buffer, avail);
                vTaskDelay(portTICK_PERIOD_MS * 1);
            }
        }

        tryagain--;
        if (tryagain <= 0) {
            Serial.println("INCOMING POST DATA TIMEDOUT WAITING FOR PACKET!");
        }
    }

    f.close();

    http_send_201(twfc);
    twfc->setClientTimedClose();
}

//------------------------------------------------------------------
//
// HANDLE GET REQUEST
//

//
// (1) MIME TYPES .. based on file extension

typedef struct {
    char ext[8];
    char type[16];
} mime_item;

static mime_item mime_list[] = {
    { "txt",  "text/plain" },
    { "htm",  "text/html" },
    { "html", "text/html" },
    { "js",   "text/javascript" },
    { "css",  "text/css" },
    { "png",  "image/png" },
    { "jpg",  "image/jpeg" },
    { "jpeg", "image/jpeg "},
    { "", ""}
};

static char *mime_type(const char *name)
{
    char *dot = strrchr(name, '.');
    if ((NULL != dot) && (0 != dot[1])) {
        dot++;  // point at first char after the dot
        mime_item *mi = mime_list;
        while (mi->ext[0]) {
            if (0 == strcmp(dot, mi->ext)) {
                return mi->type;
            }
            mi++;
        }
    }
    return mime_list[0].type;
}

//
// (2) File types based on filename

enum {
    FILE_IS_ANY     = 0,
    FILE_IS_ROOT,
    FILE_IS_INDEX,
    FILE_IS_FAVICON,
    FILE_IS_UPLOAD
};

static uint32_t match_filename(const char *path)
{
    if (0==strcmp("/favicon.ico", path)) {
        return FILE_IS_FAVICON;
    } else if (0==strcmp("/upload", path)) {
        return FILE_IS_UPLOAD;
    } else if (0==strcmp("/upload.html", path)) {
        return FILE_IS_UPLOAD;
    } else if (0==strcmp("/index.html", path)) {
        return FILE_IS_INDEX;
    } else if (0==strcmp("/", path)) {
        return FILE_IS_ROOT;
    }
    return FILE_IS_ANY;
}

static boolean is_file_html(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (dot) {
        if (0==strcmp(".js",dot)) {
            return false;
        } else if (0==strcmp(".css",dot)) {
            return false;
        }
    }
    return true; // everything forwards to html except js and css
}

void http_handleGetRequest(ToolkitWiFi_Client *twfc, const char *path,
    const char *default_index, size_t default_index_size,
    char *buffer, size_t max_size)
{
    // fetch the file from *path, set the mime-type and content headers
        // the spiffs read will pull the file into an internal 4kB buffer
        // then send those bytes directly to the client
    size_t size = 0;
    char *data = NULL;
    uint32_t type = match_filename(path);

    if (isKioskOn) {
        // over ride html files when we are in kiosk mode
        if (is_file_html(path)) {
            path = "/kiosk.html";
            type = FILE_IS_ANY;
        }
    }

    // upload needs to set data and size directly
    // index if it doesn't exist needs to set data and size directly
    // favicon should keep it's path
    // any, should change to index if it is not found

    if (FILE_IS_ROOT==type) {
        path = "/index.html";
        type = FILE_IS_INDEX;
    }

    if (FILE_IS_ANY==type) {
        if (!ToolkitFiles::fileExists(path)) {
            path = "/index.html";
            type = FILE_IS_INDEX;
        }
    }

    if (FILE_IS_INDEX==type) {
        if(!ToolkitFiles::fileExists(path)) {
            type = FILE_IS_UPLOAD;
        }
    }

    // I think at this point it is one of
    //  FILE_IS_ANY .. exists on the flash drive
    //  FILE_IS_INDEX .. exists on the flash drive
    //  FILE_IS_FAVICON .. may or may not exist on the flash drive
    //  FILE_IS_UPLOAD .. use the _default if it exists
    if (FILE_IS_UPLOAD==type) {
        if (default_index && default_index_size) {
            http_send(twfc, default_index, default_index_size,
                RESPONSE_OKAY, "text/html");
        } else {
            http_send_404(twfc);
        }
    } else {
        File f = ToolkitFiles::fileOpen(path, FILE_READ);
        if (f) {
            uint32_t content_length = f.size();
            const char *mime = mime_type(path);
            http_send_header(twfc, content_length, RESPONSE_OKAY, mime);
            // now read the file in chunks until it all sent
            int avail;
            size_t actual;
            while ((avail=f.available()) > 0) {
                if (avail > max_size) {
                    avail = max_size;
                }
                actual = f.readBytes(buffer, avail);
                http_send_data_chunk(twfc, buffer, actual);
            }
            f.close();
        } else {
            http_send_404(twfc);
        }
    }

    twfc->setClientTimedClose();
}

//
// END OF http_file.cpp
