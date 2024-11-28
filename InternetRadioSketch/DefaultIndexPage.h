//
// DefaultIndexPage.h

#ifndef _DEFAULT_INDEX_PAGE_H_
#define _DEFAULT_INDEX_PAGE_H_

const char *default_index_page =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta name='viewport' content='initial-scale=1'>"
    "<title>Wave Farm - Toolkit</title>"
    "</head>"
    "<body>"
    "<h1>Wave Farm Toolkit Default Index Page</h1>"
    "<form method='post' enctype='multipart/form-data'>"
    "<label for='file'>File</label>"
    "<input id='file' name='file' type='file' />"
    "<button>Upload</button>"
    "</form>"
    "</body>"
    "</html>";

#define DEFAULT_INDEX_PAGE_SIZE strlen(default_index_page)

#endif

/*
<form method="post" enctype="multipart/form-data">
  <label for="file">File</label>
  <input id="file" name="file" type="file" />
  <button>Upload</button>
</form>
*/

//
// END OF DefaultSettings.h
