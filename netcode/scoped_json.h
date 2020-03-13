#pragma once

#include <jansson.h>

// TODO: move this file to the thcrap directory
class ScopedJson
{
private:
    json_t *obj;

public:
    ScopedJson(json_t *obj)
        : obj(obj)
    {}
    ScopedJson(const ScopedJson& src) = delete;
    ScopedJson(ScopedJson&& src) = delete;
    ScopedJson& operator=(const ScopedJson& src) = delete;
    ScopedJson& operator=(ScopedJson&& src) = delete;
    ~ScopedJson()
    {
        json_decref(obj);
    }
    void operator()(json_t *obj)
    {
        json_decref(obj);
    }
    json_t *operator*()
    {
        return this->obj;
    }
};
