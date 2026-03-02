#pragma once

struct curl_blob {
    void *data;
    size_t len;
    unsigned int flags; /* bit 0 is defined, the rest are reserved and should be
                         left zeroes */
};

#define MAGIC_CHECK_FUNCTION_PTR(function_name, error_return)                                                                                                                     \
    if (WUT_PP_CAT(s_, function_name) == nullptr) {                                                                                                                               \
        if (sModuleHandle == nullptr || OSDynLoad_FindExport(sModuleHandle, OS_DYNLOAD_EXPORT_FUNC, #function_name, (void **) &WUT_PP_CAT(s_, function_name)) != OS_DYNLOAD_OK) { \
            DEBUG_FUNCTION_LINE_ERR("FindExport " #function_name " failed.");                                                                                                     \
            return error_return;                                                                                                                                                  \
        } else {                                                                                                                                                                  \
            functionHandles.push_front((uint32_t *) &WUT_PP_CAT(s_, function_name));                                                                                              \
        }                                                                                                                                                                         \
    }

#define MAGIC_FUNCTION_ARG8(res, function_name, error_return, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)                                             \
    static void *(*WUT_PP_CAT(s_, function_name))(void) = nullptr;                                                                                        \
    extern "C" res function_name(void *param1, void *param2, void *param3, void *param4, void *param5, void *param6, void *param7, void *param8) {        \
        MAGIC_CHECK_FUNCTION_PTR(function_name, error_return)                                                                                             \
        return reinterpret_cast<decltype(&function_name)>(WUT_PP_CAT(s_, function_name))(param1, param2, param3, param4, param5, param6, param7, param8); \
    }

#define MAGIC_FUNCTION_ARG7(res, function_name, error_return, arg1, arg2, arg3, arg4, arg5, arg6, arg7)                                           \
    static void *(*WUT_PP_CAT(s_, function_name))(void) = nullptr;                                                                                \
    extern "C" res function_name(void *param1, void *param2, void *param3, void *param4, void *param5, void *param6, void *param7) {              \
        MAGIC_CHECK_FUNCTION_PTR(function_name, error_return)                                                                                     \
        return reinterpret_cast<decltype(&function_name)>(WUT_PP_CAT(s_, function_name))(param1, param2, param3, param4, param5, param6, param7); \
    }

#define MAGIC_FUNCTION_ARG6(res, function_name, error_return, arg1, arg2, arg3, arg4, arg5, arg6)                                         \
    static void *(*WUT_PP_CAT(s_, function_name))(void) = nullptr;                                                                        \
    extern "C" res function_name(void *param1, void *param2, void *param3, void *param4, void *param5, void *param6) {                    \
        MAGIC_CHECK_FUNCTION_PTR(function_name, error_return)                                                                             \
        return reinterpret_cast<decltype(&function_name)>(WUT_PP_CAT(s_, function_name))(param1, param2, param3, param4, param5, param6); \
    }

#define MAGIC_FUNCTION_ARG5(res, function_name, error_return, arg1, arg2, arg3, arg4, arg5)                                       \
    static void *(*WUT_PP_CAT(s_, function_name))(void) = nullptr;                                                                \
    extern "C" res function_name(void *param1, void *param2, void *param3, void *param4, void *param5) {                          \
        MAGIC_CHECK_FUNCTION_PTR(function_name, error_return)                                                                     \
        return reinterpret_cast<decltype(&function_name)>(WUT_PP_CAT(s_, function_name))(param1, param2, param3, param4, param5); \
    }

#define MAGIC_FUNCTION_ARG4(res, function_name, error_return, arg1, arg2, arg3, arg4)                                     \
    static void *(*WUT_PP_CAT(s_, function_name))(void) = nullptr;                                                        \
    extern "C" res function_name(void *param1, void *param2, void *param3, void *param4) {                                \
        MAGIC_CHECK_FUNCTION_PTR(function_name, error_return)                                                             \
        return reinterpret_cast<decltype(&function_name)>(WUT_PP_CAT(s_, function_name))(param1, param2, param3, param4); \
    }

#define MAGIC_FUNCTION_ARG3(res, function_name, error_return, arg1, arg2, arg3)                                   \
    static void *(*WUT_PP_CAT(s_, function_name))(void) = nullptr;                                                \
    extern "C" res function_name(void *param1, void *param2, void *param3) {                                      \
        MAGIC_CHECK_FUNCTION_PTR(function_name, error_return)                                                     \
        return reinterpret_cast<decltype(&function_name)>(WUT_PP_CAT(s_, function_name))(param1, param2, param3); \
    }

#define MAGIC_FUNCTION_ARG2(res, function_name, error_return, arg1, arg2)                                 \
    static void *(*WUT_PP_CAT(s_, function_name))(void) = nullptr;                                        \
    extern "C" res function_name(void *param1, void *param2) {                                            \
        MAGIC_CHECK_FUNCTION_PTR(function_name, error_return)                                             \
        return reinterpret_cast<decltype(&function_name)>(WUT_PP_CAT(s_, function_name))(param1, param2); \
    }

#define MAGIC_FUNCTION_ARG1(res, function_name, error_return, arg1)                               \
    static void *(*WUT_PP_CAT(s_, function_name))(void) = nullptr;                                \
    extern "C" res function_name(void *param1) {                                                  \
        MAGIC_CHECK_FUNCTION_PTR(function_name, error_return)                                     \
        return reinterpret_cast<decltype(&function_name)>(WUT_PP_CAT(s_, function_name))(param1); \
    }

#define MAGIC_FUNCTION_ARG0(res, function_name, error_return)                               \
    static void *(*WUT_PP_CAT(s_, function_name))(void) = nullptr;                          \
    extern "C" res function_name() {                                                        \
        MAGIC_CHECK_FUNCTION_PTR(function_name, error_return)                               \
        return reinterpret_cast<decltype(&function_name)>(WUT_PP_CAT(s_, function_name))(); \
    }

#define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, MACRO_NAME, ...) MACRO_NAME
#define MAGIC_FUNCTION(res, function_name, error_return, ...) \
    GET_MACRO(__VA_ARGS__,                                    \
              MAGIC_FUNCTION_ARG8,                            \
              MAGIC_FUNCTION_ARG7,                            \
              MAGIC_FUNCTION_ARG6,                            \
              MAGIC_FUNCTION_ARG5,                            \
              MAGIC_FUNCTION_ARG4,                            \
              MAGIC_FUNCTION_ARG3,                            \
              MAGIC_FUNCTION_ARG2,                            \
              MAGIC_FUNCTION_ARG1)                            \
    (res, function_name, error_return, __VA_ARGS__)

#define RETURN_VOID

#define CURLE_FAILED_INIT      2
#define CURLHE_NOT_BUILT_IN    7
#define CURLUE_BAD_HANDLE      1
#define CURLSSLSET_NO_BACKENDS 3
#define CURLSHE_NOT_BUILT_IN   5
#define CURLM_INTERNAL_ERROR   4
