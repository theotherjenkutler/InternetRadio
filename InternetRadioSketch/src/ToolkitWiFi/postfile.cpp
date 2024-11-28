//
// putfile.cpp

#include <Arduino.h>

#define MAX_FILENAME_LENGTH 32
#define MAX_BOUNDARY_LENGTH 72

inline const char *findTwoEndlines(const char *buffer)
{
    uint32_t n = 0;
    while (*buffer) {
        switch (*buffer) {
            case '\n' :
                n++;
            case '\r' :
                break;
            default :
                n = 0;
                break;
        }
        buffer++;
        if (2==n) {
            return buffer;
        }
    }
    return NULL;
}

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

const char *postfile_extractContent(const char *buffer,
    const char **content, size_t *content_size)
{
    static char filename[MAX_FILENAME_LENGTH+2] = "/";
    static char boundary[MAX_BOUNDARY_LENGTH+2] = "";

    *content = NULL;
    *content_size = 0;

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

    // (7) FIND the beginning of the boundary string
    // NOW WE have the end of the content
    end = strstr(buffer, boundary);
    if (NULL == end) {
        return NULL;
    }

    *content_size = end - buffer;

    return (const char *) filename;
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
