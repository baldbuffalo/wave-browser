#include "logger.h"
#include "macro_magic.h"
#include <cerrno>
#include <coreinit/debug.h>
#include <coreinit/dynload.h>
#include <cstdarg>
#include <forward_list>
#include <string_view>
#include <sys/socket.h>

static void *(*s_setsockopt)() = nullptr;

static bool sInitDone                 = false;
static OSDynLoad_Module sModuleHandle = nullptr;
static uint8_t *s_cacert_pem          = nullptr;
static uint32_t *s_cacert_pem_size    = nullptr;

std::forward_list<uint32_t *> functionHandles;

extern "C" int curl_global_init() {
    if (sInitDone) {
        return 0;
    }

    if (OSDynLoad_Acquire("homebrew_curlwrapper", &sModuleHandle) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_ERR("OSDynLoad_Acquire failed.");
        return -1;
    }

    if (OSDynLoad_FindExport(sModuleHandle, OS_DYNLOAD_EXPORT_FUNC, "setsockopt", (void **) &s_setsockopt) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_ERR("FindExport setsockopt failed.");
        return -2;
    }

    if (OSDynLoad_FindExport(sModuleHandle, OS_DYNLOAD_EXPORT_DATA, "cacert_pem", (void **) &s_cacert_pem) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_WARN("FindExport cacert_pem failed.");
    }

    if (OSDynLoad_FindExport(sModuleHandle, OS_DYNLOAD_EXPORT_DATA, "cacert_pem_size", (void **) &s_cacert_pem_size) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_WARN("FindExport cacert_pem_size failed.");
    }

    char *(*s_curl_version_tmp)() = nullptr;
    if (OSDynLoad_FindExport(sModuleHandle, OS_DYNLOAD_EXPORT_FUNC, "curl_version", (void **) &s_curl_version_tmp) == OS_DYNLOAD_OK) {
        const char *expectedCURLVersion1 = "libcurl/7.84.0";
        const char *expectedCURLVersion2 = "libcurl/8.0.1";
        const char *expectedCURLVersion3 = "libcurl/8.7.1";
        if (!std::string_view(s_curl_version_tmp()).starts_with(expectedCURLVersion1) && !std::string_view(s_curl_version_tmp()).starts_with(expectedCURLVersion2) && !std::string_view(s_curl_version_tmp()).starts_with(expectedCURLVersion3)) {
            DEBUG_FUNCTION_LINE_WARN("Unexpected libcurl version: %s (expected %s)", s_curl_version_tmp(), expectedCURLVersion3);
        }
    } else {
        DEBUG_FUNCTION_LINE_WARN("Failed to check curl_version");
    }

    sInitDone = true;
    return 0;
}

extern "C" void curl_global_cleanup() {
    if (!sInitDone) { return; }
    OSDynLoad_Release(sModuleHandle);
    sModuleHandle     = nullptr;
    sInitDone         = false;
    s_setsockopt      = nullptr;
    s_cacert_pem      = nullptr;
    s_cacert_pem_size = nullptr;
    for (auto &handle : functionHandles) {
        *handle = 0;
    }
    functionHandles.clear();
}

extern "C" int curlwrapper_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    if (s_setsockopt == nullptr) {
        if (curl_global_init() != 0 || s_setsockopt == nullptr) {
            return ENOTSUP;
        }
    }
    return reinterpret_cast<decltype(&setsockopt)>(s_setsockopt)(sockfd, level, optname, optval, optlen);
}

// Variadic function in curl header, but always has 3 args
static void *(*s_curl_easy_setopt)() = nullptr;
extern "C" int curl_easy_setopt(void *param1, void *param2, void *param3) {
    if (s_curl_easy_setopt == nullptr) {
        if (sModuleHandle == nullptr || OSDynLoad_FindExport(sModuleHandle, OS_DYNLOAD_EXPORT_FUNC, "curl_easy_setopt", (void **) &s_curl_easy_setopt) != OS_DYNLOAD_OK) {
            DEBUG_FUNCTION_LINE_ERR("Failed to find export curl_easy_setopt");
            return CURLE_FAILED_INIT;
        } else {
            functionHandles.push_front((uint32_t *) &s_curl_easy_setopt);
        }
    }
    if ((uint32_t) param2 == 10000 + 149) { // CURLOPT_SOCKOPTDATA
        if (s_setsockopt != nullptr && (uint32_t) param3 == 0x13371337) {
            param3 = (void *) s_setsockopt;
        }
    }

    return reinterpret_cast<decltype(&curl_easy_setopt)>(s_curl_easy_setopt)(param1, param2, param3);
}

// Variadic function in curl header, but always has 3 args
static void *(*s_curl_easy_init)() = nullptr;
extern "C" void *curl_easy_init() {
    if (s_curl_easy_init == nullptr) {
        if (sModuleHandle == nullptr || OSDynLoad_FindExport(sModuleHandle, OS_DYNLOAD_EXPORT_FUNC, "curl_easy_init", (void **) &s_curl_easy_init) != OS_DYNLOAD_OK) {
            DEBUG_FUNCTION_LINE_ERR("Failed to find export curl_easy_init");
            return nullptr;
        } else {
            functionHandles.push_front((uint32_t *) &s_curl_easy_init);
        }
    }

    auto result = reinterpret_cast<decltype(&curl_easy_init)>(s_curl_easy_init)();
    if (result != nullptr && s_cacert_pem != nullptr && s_cacert_pem_size != nullptr) {
        struct curl_blob blob {};
        blob.data  = (void *) s_cacert_pem;
        blob.len   = *s_cacert_pem_size;
        blob.flags = 1; /*CURL_BLOB_COPY*/

        // Use the certificate bundle in the data
        if (curl_easy_setopt(result, reinterpret_cast<void *>(40000 + 309) /*CURLOPT_CAINFO_BLOB*/, &blob) != 0) {
            DEBUG_FUNCTION_LINE_WARN("Failed to set ca_certs");
        }
    }
    return result;
}

MAGIC_FUNCTION(int, curl_easy_perform, CURLE_FAILED_INIT, arg1);
MAGIC_FUNCTION(void, curl_easy_cleanup, RETURN_VOID, arg1);
MAGIC_FUNCTION(int, curl_easy_getinfo, CURLE_FAILED_INIT, arg1, arg2, arg3);
MAGIC_FUNCTION(void *, curl_easy_duphandle, nullptr, arg1);
MAGIC_FUNCTION(void, curl_easy_reset, RETURN_VOID, arg1);

MAGIC_FUNCTION(int, curl_easy_recv, CURLE_FAILED_INIT, arg1, arg2, arg3, arg4);
MAGIC_FUNCTION(int, curl_easy_send, CURLE_FAILED_INIT, arg1, arg2, arg3, arg4);
MAGIC_FUNCTION(int, curl_easy_upkeep, CURLE_FAILED_INIT, arg1);

MAGIC_FUNCTION(int, curl_easy_header, CURLHE_NOT_BUILT_IN, arg1, arg2, arg3, arg4, arg5, arg6);
MAGIC_FUNCTION(void *, curl_easy_nextheader, nullptr, arg1, arg2, arg3, arg4);

MAGIC_FUNCTION(void *, curl_easy_option_by_name, nullptr, arg1);
MAGIC_FUNCTION(void *, curl_easy_option_by_id, nullptr, arg1);
MAGIC_FUNCTION(void *, curl_easy_option_next, nullptr, arg1);

MAGIC_FUNCTION_ARG0(void *, curl_url, nullptr);
MAGIC_FUNCTION(void, curl_url_cleanup, RETURN_VOID, arg1);
MAGIC_FUNCTION(void *, curl_url_dup, nullptr, arg1);

MAGIC_FUNCTION(int, curl_url_get, CURLUE_BAD_HANDLE, arg1, arg2, arg3, arg4);
MAGIC_FUNCTION(int, curl_url_set, CURLUE_BAD_HANDLE, arg1, arg2, arg3, arg4);
MAGIC_FUNCTION(const char *, curl_url_strerror, "[ERROR]", arg1);

MAGIC_FUNCTION(int, curl_strequal, 0, arg1, arg2);
MAGIC_FUNCTION(int, curl_strnequal, 0, arg1, arg2, arg3);

MAGIC_FUNCTION(void *, curl_mime_init, nullptr, arg1);
MAGIC_FUNCTION(void, curl_mime_free, RETURN_VOID, arg1);
MAGIC_FUNCTION(void *, curl_mime_addpart, nullptr, arg1);
MAGIC_FUNCTION(int, curl_mime_name, CURLE_FAILED_INIT, arg1, arg2);
MAGIC_FUNCTION(int, curl_mime_filename, CURLE_FAILED_INIT, arg1, arg2);
MAGIC_FUNCTION(int, curl_mime_type, CURLE_FAILED_INIT, arg1, arg2);
MAGIC_FUNCTION(int, curl_mime_encoder, CURLE_FAILED_INIT, arg1, arg2);
MAGIC_FUNCTION(int, curl_mime_data, CURLE_FAILED_INIT, arg1, arg2, arg3);
MAGIC_FUNCTION(int, curl_mime_filedata, CURLE_FAILED_INIT, arg1, arg2);
MAGIC_FUNCTION(int, curl_mime_data_cb, CURLE_FAILED_INIT, arg1, arg2, arg3, arg4, arg5, arg6);
MAGIC_FUNCTION(int, curl_mime_subparts, CURLE_FAILED_INIT, arg1, arg2);
MAGIC_FUNCTION(int, curl_mime_headers, CURLE_FAILED_INIT, arg1, arg2, arg3);

MAGIC_FUNCTION(int, curl_formadd, CURLE_FAILED_INIT, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
MAGIC_FUNCTION(int, curl_formget, CURLE_FAILED_INIT, arg1, arg2, arg3);
MAGIC_FUNCTION(void, curl_formfree, RETURN_VOID, arg1);
MAGIC_FUNCTION(char *, curl_getenv, nullptr, arg1);
MAGIC_FUNCTION_ARG0(const char *, curl_version, "[ERROR]");
MAGIC_FUNCTION(char *, curl_easy_escape, nullptr, arg1, arg2, arg3);
MAGIC_FUNCTION(char *, curl_escape, nullptr, arg1, arg2);
MAGIC_FUNCTION(char *, curl_easy_unescape, nullptr, arg1, arg2, arg3, arg4);
MAGIC_FUNCTION(char *, curl_unescape, nullptr, arg1, arg2);
MAGIC_FUNCTION(void, curl_free, RETURN_VOID, arg1);
MAGIC_FUNCTION(int, curl_global_init_mem, CURLE_FAILED_INIT, arg1, arg2, arg3, arg4, arg5, arg6);

MAGIC_FUNCTION(int, curl_global_sslset, CURLSSLSET_NO_BACKENDS, arg1, arg2, arg3);
MAGIC_FUNCTION(int, curl_global_trace, CURLM_INTERNAL_ERROR, const char *config);
MAGIC_FUNCTION(void *, curl_slist_append, nullptr, arg1, arg2);
MAGIC_FUNCTION(void, curl_slist_free_all, RETURN_VOID, arg1);
MAGIC_FUNCTION(time_t, curl_getdate, 0, arg1, arg2);

MAGIC_FUNCTION_ARG0(void *, curl_share_init, nullptr);
// Variadic function in curl header, but always has 3 args
MAGIC_FUNCTION(int, curl_share_setopt, CURLSHE_NOT_BUILT_IN, arg1, arg2, arg3);
MAGIC_FUNCTION(int, curl_share_cleanup, CURLSHE_NOT_BUILT_IN, arg1);

MAGIC_FUNCTION(void *, curl_version_info, nullptr, arg1);

MAGIC_FUNCTION(const char *, curl_easy_strerror, "[ERROR]", arg1);
MAGIC_FUNCTION(const char *, curl_share_strerror, "[ERROR]", arg1);
MAGIC_FUNCTION(int, curl_easy_pause, CURLE_FAILED_INIT, arg1, arg2);

MAGIC_FUNCTION_ARG0(void *, curl_multi_init, nullptr);
MAGIC_FUNCTION(int, curl_multi_add_handle, CURLM_INTERNAL_ERROR, arg1, arg2);
MAGIC_FUNCTION(int, curl_multi_remove_handle, CURLM_INTERNAL_ERROR, arg1, arg2);
MAGIC_FUNCTION(int, curl_multi_fdset, CURLM_INTERNAL_ERROR, arg1, arg2, arg3, arg4, arg5);
MAGIC_FUNCTION(void **, curl_multi_get_handles, nullptr, CURLM *multi_handle);
MAGIC_FUNCTION(int, curl_multi_wait, CURLM_INTERNAL_ERROR, arg1, arg2, arg3, arg4, arg5);
MAGIC_FUNCTION(int, curl_multi_poll, CURLM_INTERNAL_ERROR, arg1, arg2, arg3, arg4, arg5);
MAGIC_FUNCTION(int, curl_multi_wakeup, CURLM_INTERNAL_ERROR, arg1);
MAGIC_FUNCTION(int, curl_multi_perform, CURLM_INTERNAL_ERROR, arg1, arg2);
MAGIC_FUNCTION(int, curl_multi_cleanup, CURLM_INTERNAL_ERROR, arg1);
MAGIC_FUNCTION(void *, curl_multi_info_read, nullptr, arg1, arg2);
MAGIC_FUNCTION(const char *, curl_multi_strerror, "[ERROR]", arg1);

MAGIC_FUNCTION(int, curl_multi_socket, CURLM_INTERNAL_ERROR, arg1, arg2, arg3);
MAGIC_FUNCTION(int, curl_multi_socket_action, CURLM_INTERNAL_ERROR, arg1, arg2, arg3, arg4);
MAGIC_FUNCTION(int, curl_multi_socket_all, CURLM_INTERNAL_ERROR, arg1, arg2);
MAGIC_FUNCTION(int, curl_multi_timeout, CURLM_INTERNAL_ERROR, arg1, arg2);
// Variadic function in curl header, but always has 3 args
MAGIC_FUNCTION(int, curl_multi_setopt, CURLM_INTERNAL_ERROR, arg1, arg2, arg3);
MAGIC_FUNCTION(int, curl_multi_assign, CURLM_INTERNAL_ERROR, arg1, arg2, arg3);
MAGIC_FUNCTION(char *, curl_pushheader_bynum, nullptr, arg1, arg2);
MAGIC_FUNCTION(char *, curl_pushheader_byname, nullptr, arg1, arg2);
MAGIC_FUNCTION(int, curl_mvprintf, 0, arg1, arg2);
MAGIC_FUNCTION(int, curl_mvfprintf, 0, arg1, arg2, arg3);
MAGIC_FUNCTION(int, curl_mvsprintf, 0, arg1, arg2, arg3);
MAGIC_FUNCTION(int, curl_mvsnprintf, 0, arg1, arg2, arg3, arg4);
MAGIC_FUNCTION(char *, curl_mvaprintf, nullptr, arg1, arg2);

MAGIC_FUNCTION(int, curl_ws_recv, CURLM_INTERNAL_ERROR, arg1, arg2, arg3, arg4, arg5);
MAGIC_FUNCTION(int, curl_ws_send, CURLM_INTERNAL_ERROR, arg1, arg2, arg3, arg4, arg5);

extern "C" int curl_mprintf(const char *format, ...) {
    va_list va;

    va_start(va, format);
    auto res = curl_mvprintf((void *) format, (void *) va);
    va_end(va);

    return res;
}
extern "C" int curl_mfprintf(FILE *fd, const char *format, ...) {
    va_list va;

    va_start(va, format);
    auto res = curl_mvfprintf((void *) fd, (void *) format, (void *) va);
    va_end(va);

    return res;
}
extern "C" int curl_msprintf(char *buffer, const char *format, ...) {
    va_list va;

    va_start(va, format);
    auto res = curl_mvsprintf((void *) buffer, (void *) format, (void *) va);
    va_end(va);

    return res;
}

extern "C" int curl_msnprintf(char *buffer, size_t maxlength, const char *format, ...) {
    va_list va;

    va_start(va, format);
    auto res = curl_mvsnprintf((void *) buffer, (void *) maxlength, (void *) format, (void *) va);
    va_end(va);

    return res;
}

extern "C" char *curl_maprintf(const char *format, ...) {
    va_list va;

    va_start(va, format);
    auto res = curl_mvaprintf((void *) format, (void *) va);
    va_end(va);

    return res;
}
