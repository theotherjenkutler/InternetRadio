//
// putfile.cpp

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

#include <Arduino.h>
#include <FS.h>
#include "../parsingTools.h"

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

const char *postfile_findContent(const char *buffer,
    const char **content, size_t *remaining)
{

    *content = NULL;
    *remaining = 0;
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
    end = buffer;
    while (*end) { end++; }
    *remaining = end - *content;

    return (const char *) filename;

    // (7) FIND the beginning of the boundary string
    // NOW WE have the end of the content
/*    end = strstr(buffer, boundary);
    if (NULL == end) {
        return NULL;
    }

    *content_size = end - buffer;

    return (const char *) filename;
*/
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
boolean postfile_addContent(File *f, const char *buffer, size_t size)
{
    const char *from = buffer;
    size_t used = 0;
    while (size) {
        if (doesItMatch(*from)) {
            // *from is now in the match buffer
            // clear the used buffer
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

//
// END OF putfile.cpp
