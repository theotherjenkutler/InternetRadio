//
// ToolkitSPIFFS.cpp

#include "ToolkitSPIFFS.h"
//#include "ToolkitSettings.h"

const char *SETTINGS_PATHNAME = "/settings.txt";

char ToolkitSPIFFS::big_buffer[MAX_FILE_SIZE];

boolean ToolkitSPIFFS::begin()
{
    return SPIFFS.begin(true); // formats the flash if it isn't formatted
//    bool formatted = SPIFFS.format();
} // returns false if the file system doesn't mount

//
// Some DOCS:
//
// size_t actual = file.readBytes(char *buffer, size_t length);
// int avail = file.available(); .. not sure if this is the same as filesize
// void file.close();
// boolean file.isDirectory();
// size_t file.size();
// size_t file.write(char *buffer, size_t length);
//
// boolean FS.exists(const char *path);
// File FS.open(const char *path, int mode=FILE_READ, const bool create=false);
// mode can also be FILE_WRITE or FILE_APPEND
// NOTE: the headers available online differ from the headers used by the IDE
// bool create is not part of the open() prototype!

boolean ToolkitSPIFFS::fileExists(const char *path)
{
    return SPIFFS.exists(path);
}

char* ToolkitSPIFFS::fileRead(const char *path, size_t *size)
{
    *size = 0;
    File f = SPIFFS.open(path);
    if(!f || f.isDirectory()){
        return NULL;
    }
    size_t length = f.size();
    if (length > (MAX_FILE_SIZE-1)) {
        Serial.print("Filesize is too big .. ");
        Serial.println(path);
        f.close();
        return NULL;
    }

    char *p = big_buffer;
    size_t remaining = length;
    size_t actual;
    while (remaining) {
        actual = f.readBytes(p, remaining);
        p = p + actual;
        remaining = remaining - actual;
    }
    f.close();
    big_buffer[length] = 0;
    *size = length;
    return big_buffer;
}

void ToolkitSPIFFS::fileReadToSerial(const char *path)
{
    Serial.printf("Reading file: %s\r\n", path);

    File f = SPIFFS.open(path);
    if(!f || f.isDirectory()){
        Serial.println("− failed to open file for reading");
        return;
    }

    Serial.println("− read from file:");
    while(f.available()){
        Serial.write(f.read());
    }
}

boolean ToolkitSPIFFS::fileWrite(const char *path, const char *buffer, size_t size)
{
    File f = SPIFFS.open(path, FILE_WRITE);
    if (!f) {
        return false;
    }
    const char *p = buffer;
    size_t remaining = size;
    size_t actual;
    while (remaining) {
        actual = f.write((uint8_t *) p, remaining);
        p = p + actual;
        remaining = remaining - actual;
    }
    f.close();
    return true;
}

boolean ToolkitSPIFFS::loadSettings()
{
    size_t length;
    char *buffer = fileRead(SETTINGS_PATHNAME, &length);
    if (buffer && length) {
        SettingItem::loadSettingsFromBuffer(buffer, length);
        return true;
    }
    return false;
}

void ToolkitSPIFFS::saveSettings()
{
    size_t actual = SettingItem::saveAll(big_buffer, MAX_FILE_SIZE);
    if (actual) {
        fileWrite(SETTINGS_PATHNAME, big_buffer, actual);
    }
}

/*
    REFERENCE CODE
*/

/*
//  listDir(SPIFFS, "/", 0);
//  readFile(SPIFFS, "/test.txt");

//--------------
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
   Serial.printf("Listing directory: %s\r\n", dirname);

   File root = fs.open(dirname);
   if(!root){
      Serial.println("− failed to open directory");
      return;
   }
   if(!root.isDirectory()){
      Serial.println(" − not a directory");
      return;
   }

   File file = root.openNextFile();
   while(file){
      if(file.isDirectory()){
         Serial.print("  DIR : ");
         Serial.println(file.name());
         if(levels){
            listDir(fs, file.name(), levels -1);
         }
      } else {
         Serial.print("  FILE: ");
         Serial.print(file.name());
         Serial.print("\tSIZE: ");
         Serial.println(file.size());
      }
      file = root.openNextFile();
   }
}

void readFile(fs::FS &fs, const char * path){
   Serial.printf("Reading file: %s\r\n", path);

   File file = fs.open(path);
   if(!file || file.isDirectory()){
       Serial.println("− failed to open file for reading");
       return;
   }

   Serial.println("− read from file:");
   while(file.available()){
      Serial.write(file.read());
   }
}
*/
//-----------

//
// END OF ToolkitSPIFFS.cpp
