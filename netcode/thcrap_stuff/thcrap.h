#pragma once

#include <jansson.h>

#include "jansson_ex.h"
#include "repo.h"
#include "patchfile.h"
#include "stack.h"

#define log_printf printf
#define InternetOpenU InternetOpenA

#define PROJECT_NAME_SHORT() "thcrap netcode"
#define PROJECT_VERSION_STRING() "0.0.2"
#define windows_version() "Windows 10 (hardcoded string)"

// Automatically decref a json when leaving the current scope
class ScopedJson
{
private:
    json_t *obj;

public:
    ScopedJson(json_t *obj)
        : obj(obj)
    {}

    ScopedJson(const ScopedJson& src)
        : obj(json_incref(src.obj))
    {}
    ScopedJson& operator=(const ScopedJson& src)
    {
        this->obj = json_incref(src.obj);
        return *this;
    }
    ScopedJson(ScopedJson&& src)
        : obj(src.obj)
    {
        src.obj = nullptr;
    }
    ScopedJson& operator=(ScopedJson&& src)
    {
        this->obj = src.obj;
        src.obj = nullptr;
        return *this;
    }

    ~ScopedJson()
    {
        if (obj) {
            json_decref(obj);
        }
    }

    json_t *operator*() const
    {
        return this->obj;
    }
    operator bool()
    {
        return this->obj != nullptr;
    }
};
