#include <sys/stat.h>
#include "my_net_config.h"

// LittleFS mount point (default in Arduino-ESP32)
static constexpr const char *FS_BASE_PATH = "/littlefs";

// stat-based exists check to avoid VFS warning logs on missing files
static bool fsExistsNoLog(const String &path)
{
    struct stat st {};
    String full = String(FS_BASE_PATH) + path;
    return stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}
// ======================= MIME & 静态文件 =======================
static String contentType(const String &path)
{
    if (path.endsWith(".htm") || path.endsWith(".html"))
        return "text/html; charset=utf-8";
    if (path.endsWith(".css"))
        return "text/css; charset=utf-8";
    if (path.endsWith(".js"))
        return "application/javascript; charset=utf-8";
    if (path.endsWith(".json"))
        return "application/json; charset=utf-8";
    if (path.endsWith(".png"))
        return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg"))
        return "image/jpeg";
    if (path.endsWith(".gif"))
        return "image/gif";
    if (path.endsWith(".svg"))
        return "image/svg+xml";
    if (path.endsWith(".ico"))
        return "image/x-icon";
    if (path.endsWith(".txt"))
        return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

bool handleFileRead(AsyncWebServerRequest *req, String path)
{
    if (path.endsWith("/"))
        path += "home.html"; // 默认页
    const String gz = path + ".gz";
    const String type = contentType(path);

    const bool hasGz = fsExistsNoLog(gz);
    const bool hasRaw = fsExistsNoLog(path);

    if (hasGz)
    {
        auto *res = req->beginResponse(FSYS, gz, type);
        res->addHeader("Content-Encoding", "gzip");
        res->addHeader("Cache-Control", "public, max-age=604800");
        req->send(res);
        return true;
    }
    if (hasRaw)
    {
        auto *res = req->beginResponse(FSYS, path, type);
        res->addHeader("Cache-Control", "public, max-age=604800");
        req->send(res);
        return true;
    }
    return false;
}
