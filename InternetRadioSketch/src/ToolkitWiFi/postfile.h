//
// postfile.h

#ifndef _POSTFILE_H_
#define _POSTFILE_H_

const char *postfile_extractContent(const char *buffer,
    const char **content, size_t *content_size);
    // returns the filename of the original uploaded file

#endif

//
// END OF postfile.h
