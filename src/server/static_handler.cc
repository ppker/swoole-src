/*
 +----------------------------------------------------------------------+
 | Swoole                                                               |
 +----------------------------------------------------------------------+
 | This source file is subject to version 2.0 of the Apache license,    |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.apache.org/licenses/LICENSE-2.0.html                      |
 | If you did not receive a copy of the Apache2.0 license and are unable|
 | to obtain it through the world-wide-web, please send a note to       |
 | license@swoole.com so we can mail you a copy immediately.            |
 +----------------------------------------------------------------------+
 | Author: Tianfeng Han  <rango@swoole.com>                             |
 +----------------------------------------------------------------------+
 */

#include "swoole_static_handler.h"
#include "swoole_util.h"

#include <string>
#include <dirent.h>
#include <algorithm>

namespace swoole {

namespace http_server {
bool StaticHandler::is_modified(const std::string &date_if_modified_since) {
    char date_tmp[64];
    if (date_if_modified_since.empty() || date_if_modified_since.length() > sizeof(date_tmp) - 1) {
        return false;
    }

    struct tm tm3 {};
    memcpy(date_tmp, date_if_modified_since.c_str(), date_if_modified_since.length());
    date_tmp[date_if_modified_since.length()] = 0;

    const char *date_format = nullptr;

    if (strptime(date_tmp, SW_HTTP_RFC1123_DATE_GMT, &tm3) != nullptr) {
        date_format = SW_HTTP_RFC1123_DATE_GMT;
    } else if (strptime(date_tmp, SW_HTTP_RFC1123_DATE_UTC, &tm3) != nullptr) {
        date_format = SW_HTTP_RFC1123_DATE_UTC;
    } else if (strptime(date_tmp, SW_HTTP_RFC850_DATE, &tm3) != nullptr) {
        date_format = SW_HTTP_RFC850_DATE;
    } else if (strptime(date_tmp, SW_HTTP_ASCTIME_DATE, &tm3) != nullptr) {
        date_format = SW_HTTP_ASCTIME_DATE;
    }
    return date_format && mktime(&tm3) - (int) serv->timezone_ >= get_file_mtime();
}

std::string StaticHandler::get_date() {
    char date_[64];
    time_t now = ::time(nullptr);
    struct tm *tm1 = gmtime(&now);
    strftime(date_, sizeof(date_), "%a, %d %b %Y %H:%M:%S %Z", tm1);
    return std::string(date_);
}

std::string StaticHandler::get_date_last_modified() {
    char date_last_modified[64];
    time_t file_mtime = get_file_mtime();
    struct tm *tm2 = gmtime(&file_mtime);
    strftime(date_last_modified, sizeof(date_last_modified), "%a, %d %b %Y %H:%M:%S %Z", tm2);
    return std::string(date_last_modified);
}

bool StaticHandler::hit() {
    char *p = task.filename;
    const char *url = request_url.c_str();
    size_t url_length = request_url.length();
    /**
     * discard the url parameter
     * [/test.jpg?version=1#position] -> [/test.jpg]
     */
    char *params = (char *) memchr(url, '?', url_length);
    if (params == nullptr) {
        params = (char *) memchr(url, '#', url_length);
    }
    size_t n = params ? params - url : url_length;

    const std::string &document_root = serv->get_document_root();

    memcpy(p, document_root.c_str(), document_root.length());
    p += document_root.length();

    if (serv->locations->size() > 0) {
        for (auto i = serv->locations->begin(); i != serv->locations->end(); i++) {
            if (swoole_strcasect(url, url_length, i->c_str(), i->size())) {
                last = true;
            }
        }
        if (!last) {
            return false;
        }
    }

    if (document_root.length() + n >= PATH_MAX) {
        return false;
    }

    memcpy(p, url, n);
    p += n;
    *p = '\0';
    if (dir_path != "") {
        dir_path.clear();
    }
    dir_path = std::string(url, n);

    l_filename = http_server::url_decode(task.filename, p - task.filename);
    task.filename[l_filename] = '\0';

    if (swoole_strnpos(url, n, SW_STRL("..")) == -1) {
        goto _detect_mime_type;
    }

    char real_path[PATH_MAX];
    if (!realpath(task.filename, real_path)) {
        if (last) {
            status_code = SW_HTTP_NOT_FOUND;
            return true;
        } else {
            return false;
        }
    }

    if (real_path[document_root.length()] != '/') {
        return false;
    }

    if (swoole_streq(real_path, strlen(real_path), document_root.c_str(), document_root.length()) != 0) {
        return false;
    }

// non-static file
_detect_mime_type:
// file does not exist
check_stat:
    if (lstat(task.filename, &file_stat) < 0) {
        if (last) {
            status_code = SW_HTTP_NOT_FOUND;
            return true;
        } else {
            return false;
        }
    }

    if (S_ISLNK(file_stat.st_mode)) {
        char buf[PATH_MAX];
        ssize_t byte = ::readlink(task.filename, buf, sizeof(buf) - 1);
        if (byte <= 0) {
            return false;
        }
        buf[byte] = 0;
        swoole_strlcpy(task.filename, buf, sizeof(task.filename));
        goto check_stat;
    }

    if (serv->http_index_files && !serv->http_index_files->empty() && is_dir()) {
        return true;
    }

    if (serv->http_autoindex && is_dir()) {
        return true;
    }

    if (!swoole::mime_type::exists(task.filename) && !last) {
        return false;
    }

    if (!S_ISREG(file_stat.st_mode)) {
        return false;
    }
    task.length = get_filesize();

    return true;
}

bool StaticHandler::hit_index_file() {
    if (serv->http_index_files && !serv->http_index_files->empty() && is_dir()) {
        if (!get_dir_files()) {
            return false;
        }
        index_file = intersection(*serv->http_index_files, dir_files);

        if (has_index_file() && !set_filename(index_file)) {
            return false;
        }
        if (!has_index_file() && !is_enabled_auto_index()) {
            return false;
        }
    }
    return true;
}

size_t StaticHandler::make_index_page(String *buffer) {
    get_dir_files();

    if (dir_path.back() != '/') {
        dir_path.append("/");
    }

    buffer->format_impl(String::FORMAT_APPEND | String::FORMAT_GROW,
                        "<html>\n"
                        "<head>\n"
                        "\t<meta charset='UTF-8'>\n<title>Index of %s</title>"
                        "</head>\n"
                        "<body>\n" SW_HTTP_PAGE_CSS "<h1>Index of %s</h1>"
                        "\t<ul>\n",
                        dir_path.c_str(),
                        dir_path.c_str());

    for (auto iter = dir_files.begin(); iter != dir_files.end(); iter++) {
        if (*iter == "." || (dir_path == "/" && *iter == "..")) {
            continue;
        }
        buffer->format_impl(String::FORMAT_APPEND | String::FORMAT_GROW,
                            "\t\t<li><a href=%s%s>%s</a></li>\n",
                            dir_path.c_str(),
                            (*iter).c_str(),
                            (*iter).c_str());
    }

    buffer->format_impl(String::FORMAT_APPEND | String::FORMAT_GROW, "\t</ul>\n" SW_HTTP_POWER_BY "</body>\n</html>\n");

    return buffer->length;
}

bool StaticHandler::get_dir_files() {
    if (!dir_files.empty()) {
        return true;
    }

    if (!is_dir()) {
        return false;
    }

    DIR *dir = opendir(task.filename);
    if (dir == nullptr) {
        return false;
    }

    struct dirent *ptr;
    while ((ptr = readdir(dir)) != nullptr) {
        dir_files.insert(ptr->d_name);
    }

    closedir(dir);

    return true;
}

bool StaticHandler::set_filename(std::string &filename) {
    char *p = task.filename + l_filename;

    if (*p != '/') {
        *p = '/';
        p += 1;
    }

    memcpy(p, filename.c_str(), filename.length());
    p += filename.length();
    *p = 0;

    if (lstat(task.filename, &file_stat) < 0) {
        return false;
    }

    if (!S_ISREG(file_stat.st_mode)) {
        return false;
    }

    task.length = get_filesize();

    return true;
}
}  // namespace http_server
void Server::add_static_handler_location(const std::string &location) {
    if (locations == nullptr) {
        locations = new std::unordered_set<std::string>;
    }
    locations->insert(location);
}

void Server::add_static_handler_index_files(const std::string &file) {
    if (http_index_files == nullptr) {
        http_index_files = new std::vector<std::string>;
    }

    auto iter = std::find(http_index_files->begin(), http_index_files->end(), file);
    if (iter == http_index_files->end()) {
        http_index_files->push_back(file);
    }
}
}  // namespace swoole
