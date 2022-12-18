#pragma once
//#include <LittleFS.h>
#include "TickerDebug.hpp"

extern bool lfs_OK;

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(webServer.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

/*
   Read the given file from the filesystem and stream it back to the client
*/
bool handleFileRead(String path) {
  if (!lfs_OK) {
    Sprintln("handleFileRead: Filesystem error.");
    return true;
  }

  if (path.endsWith("/")) {
    path += "index.htm";
  }

  String contentType;
  if (webServer.hasArg("download")) {
    contentType = F("application/octet-stream");
  } else {
    contentType = getContentType(path);
  }

  if (!fileSystem->exists(path)) {
    // File not found, try gzip version
    path = path + ".gz";
  }
  if (fileSystem->exists(path)) {
    File file = fileSystem->open(path, "r");
    if (webServer.streamFile(file, contentType) != file.size()) {
      //DBG_OUTPUT_PORT.println("Sent less data than expected!");
    }
    file.close();
    return true;
  }

  return false;

}
  
/*
   Return the list of files in the directory specified by the "dir" query string parameter.
   Also demonstrates the use of chuncked responses.
*/
void printDirectory(String path) {
  if (!lfs_OK) {
     Sprintln(F("printDirectory: Filesystem error."));
     return;
  }

  if (path != "/" && !fileSystem->exists(path)) {

    Sprintln(F("BAD PATH"));
    return;
  }

  Dir dir = fileSystem->openDir(path);
  path.clear();


  // use the same string for every line
  while (dir.next()) {
    Sprint("{\"type\":\"");
    if (dir.isDirectory()) {
      Sprint("dir");
    } else {
      Sprint(F("file\",\"size\":\""));
      SprintDEC(dir.fileSize(), DEC);
    }

    Sprint(F("\",\"name\":\""));

    // Always return names without leading "/"
    if (dir.fileName()[0] == '/') {
      Sprint(&(dir.fileName()[1]));
    } else {
      Sprint(dir.fileName());
    }
    Sprintln("\"}");
  }
  
} // printDirectory