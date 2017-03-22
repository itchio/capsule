
#include "env.h"

#include <stdlib.h>

#include "platform.h"
#include "strings.h"

#if defined(LAB_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif // LAB_WINDOWS

namespace lab {
namespace env {

const int kMaxCharacters = 16 * 1024;

char **MergeBlocks (char **a, char **b) {
    size_t total_size = 0;
    char **p = a;
    while (*p) {
        total_size++;
        p++;
    }

    p = b;
    while (*p) {
        total_size++;
        p++;
    }

    char **res = (char **) calloc(total_size + 1, sizeof(char *));
    size_t i = 0;

    p = a;
    while (*p) {
        res[i++] = *p;
        p++;
    }

    p = b;
    while (*p) {
        res[i++] = *p;
        p++;
    }

    res[i] = NULL;

    return res;
}

std::string Get(std::string name) {
#if defined(LAB_WINDOWS)
    wchar_t *name_w;
    strings::ToWideChar(name.c_str(), &name_w);
    const size_t value_w_characters = kMaxCharacters;
    wchar_t *value_w = reinterpret_cast<wchar_t *>(calloc(value_w_characters, sizeof(wchar_t)));
    int ret = GetEnvironmentVariableW(name_w, value_w, value_w_characters);
    free(name_w);

    if (ret == 0) {
        free(value_w);
        return "";
    } else {
        char *value;
        strings::FromWideChar(value_w, &value);
        free(value_w);
        std::string result = std::string(value);
        free(value);
        return result;
    }
#else // LAB_WINDOWS
    // doesn't need to be freed, points to environment block
    const char *value = getenv(name.c_str());
    if (!value) {
        return "";
    } else {
        return std::string(value):
    }
#endif // !LAB_WINDOWS
}

bool Set(std::string name, std::string value) {
#if defined(LAB_WINDOWS)
    wchar_t *name_w;
    strings::ToWideChar(name.c_str(), &name_w);
    wchar_t *value_w;
    strings::ToWideChar(value.c_str(), &value_w);

    BOOL result = SetEnvironmentVariableW(name_w, value_w);
    free(name_w);
    free(value_w);
    return result == TRUE;
#else  // LAB_WINDOWS
    int result = setenv(name.c_str(), value.c_str(), 1 /* overwrite */);
    return result == 0;
#endif // !LAB_WINDOWS
}

#if defined(LAB_WINDOWS)
std::string Expand(std::string input) {
    wchar_t *input_w;
    strings::ToWideChar(input.c_str(), &input_w);
    const size_t output_w_characters = kMaxCharacters;
    wchar_t *output_w = reinterpret_cast<wchar_t *>(calloc(output_w_characters, sizeof(wchar_t)));

    ExpandEnvironmentStringsW(input_w, output_w, output_w_characters);
    free(input_w);

    char* output;
    strings::FromWideChar(output_w, &output);
    std::string result = std::string(output);
    free(output_w);
    free(output);
    return result;
}
#endif // LAB_WINDOWS

} // namespace env
} // namespace lab
