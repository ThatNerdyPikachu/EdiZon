#include "upload_manager.hpp"

#include "zipper.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <dirent.h>
#include <vector>
#include <string>
#include <cstdio>
#include <filesystem>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "guis/gui.hpp"

UploadManager::UploadManager() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  m_curl = curl_easy_init();

  if (!m_curl)
    printf("Curl initialization failed!\n");
}

UploadManager::~UploadManager() {
  curl_easy_cleanup(m_curl);
  curl_global_cleanup();
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, std::string* s) {
  *s += reinterpret_cast<char*>(ptr);
  return size*nmemb;
}

static s32 seek_cb(void *userp, curl_off_t offset, s32 origin) {
  return CURL_SEEKFUNC_OK;
}

std::vector<std::string> listFiles(const std::string &path) {
  std::vector<std::string> paths;
    if (auto dir = opendir(path.c_str())) {
        while (auto f = readdir(dir)) {
            if (!f->d_name || f->d_name[0] == '.') continue;
            if (f->d_type == DT_DIR) 
                listFiles(path + f->d_name + "/");

            if (f->d_type == DT_REG)
              paths.push_back(path + "/" + f->d_name);
        }
        closedir(dir);
    }

    return paths;
}
 
static int xferinfo(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
  if (Gui::g_currMessageBox != nullptr)
    Gui::g_currMessageBox->setProgress(50.0F + ((ulnow / static_cast<float>(ultotal)) * 100) / 2.0F);

  hidScanInput();

  return hidKeysDown(CONTROLLER_P1_AUTO) & KEY_B;
}

std::string UploadManager::upload(std::string path, std::string fileName, Title *title, std::string hash) {
  if (gethostid() == INADDR_LOOPBACK) return "";

  std::vector<u8> zipData;
  std::string returnAddress;

  std::vector<std::string> filePaths;
  std::filesystem::recursive_directory_iterator end;
  for (std::filesystem::recursive_directory_iterator it(path); it != end; ++it)
    if (!it->is_directory())
      filePaths.push_back(it->path().c_str());

  if (!this->zip(filePaths, &zipData)) return "";

  curl_mime *mime;
  curl_mimepart *part;

  mime = curl_mime_init(m_curl);
  part = curl_mime_addpart(mime);

  curl_mime_data(part, reinterpret_cast<const char*>(&zipData[0]), zipData.size());
  curl_mime_filename(part, std::string(fileName + ".zip").c_str());
  curl_mime_name(part, "file");

  curl_easy_setopt(m_curl, CURLOPT_MIMEPOST, mime);
  curl_easy_setopt(m_curl, CURLOPT_URL, "http://werwolv.teamatlasnx.com/v1/upload");
  curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writefunc);
  curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &returnAddress);
  curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, &xferinfo);
  curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);

  if (curl_easy_perform(m_curl) != CURLE_OK) returnAddress = "";

  if (returnAddress.find("https://transfer.sh/") != std::string::npos) {
    std::stringstream ss;
    ss << "http://werwolv.net/edizon_upload/sharedlink.php";
    ss << "?link=" << curl_easy_escape(m_curl, returnAddress.c_str(), returnAddress.length());
    ss << "&hash=" << hash;
    ss << "&backupname=" << curl_easy_escape(m_curl, fileName.c_str(), fileName.length());
    ss << "&gamename=" << curl_easy_escape(m_curl, title->getTitleName().c_str(), title->getTitleName().length());
    ss << "&titleid=" << std::uppercase << std::setfill('0') << std::setw(16) << std::hex << title->getTitleID();

    curl_easy_setopt(m_curl, CURLOPT_URL, ss.str().c_str());
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &m_returnCode);

    CURLcode res = curl_easy_perform(m_curl);
    if (res != CURLE_OK)
      printf("Error %d\n", res);
  }

  curl_mime_free(mime);

  return m_returnCode;
}

bool UploadManager::zip(std::vector<std::string> paths, std::vector<u8> *zipData) {
  zipper::Zipper archive(*zipData);

  u8 pathOffset = 25;
  u32 currFile = 0;

  if (paths[0].find("/EdiZon/batch/") != std::string::npos)
    pathOffset = 14;
  else if (paths[0].find("/EdiZon/restore/") != std::string::npos)
    pathOffset = 16;

  for(auto path : paths) { 
    std::ifstream file = std::ifstream(path);

    archive.add(file, &path.c_str()[pathOffset], zipper::Zipper::Better);

    if (Gui::g_currMessageBox != nullptr)
      Gui::g_currMessageBox->setProgress(((++currFile / static_cast<float>(paths.size())) * 100) / 2.0F);

    hidScanInput();
    if (hidKeysDown(CONTROLLER_P1_AUTO) & KEY_B)
      return false;

    file.close();
  }

  archive.close();

  return true;
}