#include "unzipper.h"

#include "memory/mem_buffer.h"
#include "roo_logging.h"

namespace tapuino {

namespace {

FS *zipFs;
File myfile;

//
// Callback functions needed by the unzipLIB to access a file system
// The library has built-in code for memory-to-memory transfers, but needs
// these callback functions to allow using other storage media
//
void *myOpen(const char *filename, int32_t *size) {
  myfile = zipFs->open(filename);
  *size = myfile.size();
  return (void *)&myfile;
}
void myClose(void *p) {
  ZIPFILE *pzf = (ZIPFILE *)p;
  File *f = (File *)pzf->fHandle;
  if (f) f->close();
}

int32_t myRead(void *p, uint8_t *buffer, int32_t length) {
  ZIPFILE *pzf = (ZIPFILE *)p;
  File *f = (File *)pzf->fHandle;
  return f->read(buffer, length);
}

int32_t mySeek(void *p, int32_t position, int iType) {
  ZIPFILE *pzf = (ZIPFILE *)p;
  File *f = (File *)pzf->fHandle;
  if (iType == SEEK_SET)
    return f->seek(position);
  else if (iType == SEEK_END) {
    return f->seek(position + pzf->iSize);
  } else {  // SEEK_CUR
    long l = f->position();
    return f->seek(l + position);
  }
}

}  // namespace

namespace unzipper {

int OpenZip(FS &fs, const char *filename) {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  zipFs = &fs;
  zip.zHandle =
      unzOpen(filename, nullptr, 0, &zip, &myOpen, &myRead, &mySeek, &myClose);
  if (zip.zHandle == nullptr) {
    LOG(ERROR) << "Error opening file: " << filename;
    return -1;
  }
  return 0;
}

int CloseZip() {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  zip.iLastError = unzClose((unzFile)zip.zHandle);
  return zip.iLastError;
}

int OpenCurrentFile() {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  zip.iLastError = unzOpenCurrentFile((unzFile)zip.zHandle);
  return zip.iLastError;
}

int CloseCurrentFile() {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  zip.iLastError = unzCloseCurrentFile((unzFile)zip.zHandle);
  return zip.iLastError;
}

int ReadCurrentFile(uint8_t *buffer, size_t len) {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  return unzReadCurrentFile((unzFile)zip.zHandle, buffer, len);
}

int GetCurrentFilePos() {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  return (int)unztell((unzFile)zip.zHandle);
}

int GotoFirstFile() {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  return unzGoToFirstFile((unzFile)zip.zHandle);
}

int GotoNextFile() {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  zip.iLastError = unzGoToNextFile((unzFile)zip.zHandle);
  return zip.iLastError;
}

int LocateFile(const char *filename) {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  zip.iLastError = unzLocateFile((unzFile)zip.zHandle, filename, 2);
  return zip.iLastError;
}

int GetCurrentFileInfo(FileInfo &info) {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  return unzGetCurrentFileInfo((unzFile)zip.zHandle, &info.info, info.filename,
                               sizeof(info.filename), info.extra_field,
                               sizeof(info.extra_field), info.comment,
                               sizeof(info.comment));
}

int GetLastError() {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  return zip.iLastError;
}

int GetGlobalComment(char *buf, size_t size) {
  ZIPFILE &zip = membuf::GetUnzipBuffer();
  zip.iLastError = unzGetGlobalComment((unzFile)zip.zHandle, buf, size);
  return zip.iLastError;
}

}  // namespace unzipper
}  // namespace tapuino