
#pragma once

#include <stdio.h>

struct FileWrapper
{
    FileWrapper() 
        : f(nullptr)
    {}
    FileWrapper(FILE* f)
        : f(f)
    {}
    FileWrapper(FileWrapper&& o)
    {
        f = o.f;
        o.f = nullptr;
    }
    FileWrapper& operator=(FileWrapper&& o)
    {
        destroy();
        f = o.f;
        o.f = nullptr;
        return *this;
    }
    FILE* f = nullptr;

    void destroy()
    {
        fclose(f);
        f = nullptr;
    }

    ~FileWrapper()
    {
        destroy();
    }

    operator FILE*() const
    {
        return f;
    }
};
