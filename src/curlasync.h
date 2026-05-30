
#pragma once


#include "asyncengine.h"
#include <curl/curl.h>
#include <unordered_map>
#include <mutex>
#include <string>

class CurlAsyncEngine;
struct CurlEasyTaskResult;

typedef void CURL;
typedef void CURLM;
struct curl_slist;


struct CurlFileWrapper
{
    CurlFileWrapper() 
        : f(nullptr)
    {}
    CurlFileWrapper(FILE* f)
        : f(f)
    {}
    CurlFileWrapper(CurlFileWrapper&& o)
    {
        f = o.f;
        o.f = nullptr;
    }
    CurlFileWrapper& operator=(CurlFileWrapper&& o)
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

    ~CurlFileWrapper()
    {
        destroy();
    }

    operator FILE*() const
    {
        return f;
    }
};

struct CurlEasyTaskResult
{
    std::string data;
    CurlFileWrapper file;
    int httpCode = 0;
    CURLMcode mError = CURLMcode::CURLM_OK;
    CURLcode cError = CURLcode::CURLE_OK;
};

enum class HttpType
{
    Get,
    Post
};

struct CurlEasyTask : public AsyncTask<CurlEasyTaskResult, CurlAsyncEngine>
{
    CURL* ez = nullptr;
    curl_slist* headers = nullptr;
    curl_mime* postData = nullptr;
    curl_mimepart* part = nullptr;
    CurlFileWrapper file;
    std::string outStr;
    HttpType type;
    std::string postDataStr;

    CurlEasyTask();
    ~CurlEasyTask();
    void Start(CurlAsyncEngine &);
    void Stop(CurlAsyncEngine &);
    void OnFinish(CurlAsyncEngine & , CurlEasyTaskResult& outResult);
};


struct CurlTaskData
{
    CURLcode code;
    std::shared_ptr<AsyncTaskBase<CurlAsyncEngine>> task;
};

class CurlAsyncEngine
{
public:
    CURLM* multi = nullptr;
    std::mutex mt;

    std::unordered_map<CURL*, CurlTaskData> curlMap;

    CurlAsyncEngine();
    ~CurlAsyncEngine();

    void update(ProcessorUpdate<CurlAsyncEngine> & result);

};

