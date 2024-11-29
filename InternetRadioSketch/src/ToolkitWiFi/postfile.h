//
// postfile.h

#ifndef _POSTFILE_H_
#define _POSTFILE_H_

#include <FS.h>

const char *postfile_findContent(const char *buffer,
    const char **content, size_t *remaining);
    // returns the filename of the original uploaded file
    // NULL if there an error in the headers
    // *content and remaining will be set if there is uploaded content

boolean postfile_addContent(File *f, const char *buffer, size_t size);

#endif

//
// END OF postfile.h
