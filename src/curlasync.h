
#pragma once


#include "asyncengine.h"
#include "filewrapper.h"
#include <curl/curl.h>
#include <unordered_map>
#include <mutex>
#include <string>

class CurlAsyncEngine;
struct CurlEasyTaskResult;

typedef void CURL;
typedef void CURLM;
struct curl_slist;


struct CurlEasyTaskResult
{
    std::string data;
    FileWrapper file;
    int httpCode = 0;
    CURLMcode mError = CURLMcode::CURLM_OK;
    CURLcode cError = CURLcode::CURLE_OK;
    bool canceled = false;
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
    //curl_mime* postData = nullptr;
    //curl_mimepart* part = nullptr;
    FileWrapper file;
    std::string outStr;
    HttpType type;
    std::string postDataStr;
    std::string contentType;
    bool canceled = false;
    
    // internal functions
    CurlEasyTask();
    ~CurlEasyTask();
    void Start(CurlAsyncEngine &);
    void Stop(CurlAsyncEngine &);
    void OnFinish(CurlAsyncEngine & , CurlEasyTaskResult& outResult);

    void SetUrl(char const * url);
    void SetHeader(std::string const & name, std::string const & value);
    void ClearHeaders();

    // progress queries
};


struct CurlTaskData
{
    CURLcode code;
    CURLMcode mcode;
    std::shared_ptr<AsyncTaskBase<CurlAsyncEngine>> task;
};

class CurlAsyncEngine
{
public:
    CURLM* multi = nullptr;
    std::mutex mt;

    std::unordered_map<CURL*, CurlTaskData> curlMap;
    std::vector<CurlTaskData> failed;

    CurlAsyncEngine();
    ~CurlAsyncEngine();

    void update(ProcessorUpdate<CurlAsyncEngine> & result);

};

using CurlTask = AsyncTaskRef<CurlAsyncEngine, CurlEasyTask>;

