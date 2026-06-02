

#include "curlasync.h"

#include <format>


CurlEasyTask::CurlEasyTask()
{
    ez = curl_easy_init();
}

CurlEasyTask::~CurlEasyTask()
{
    curl_easy_cleanup(ez);
    curl_slist_free_all(headers);
}

size_t CurlEasyTask_write_cb(char* data, size_t n, size_t l, CurlEasyTask* userp)
{
    userp->outStr.insert(userp->outStr.end(), data, data + n * l);
    return n * l;
}

void CurlEasyTask::Start(CurlAsyncEngine& env)
{
    if (file)
    {
        curl_easy_setopt(ez, CURLOPT_WRITEFUNCTION, fwrite);
        curl_easy_setopt(ez, CURLOPT_WRITEDATA, static_cast<FILE*>(file));
    }
    else
    {
        curl_easy_setopt(ez, CURLOPT_WRITEFUNCTION, CurlEasyTask_write_cb);
        curl_easy_setopt(ez, CURLOPT_WRITEDATA, this);
    }
    if (type == HttpType::Post && !contentType.empty())
    {
        std::string ctHeader = std::format("Content-Type: {}", contentType);
        headers = curl_slist_append(headers, ctHeader.c_str());
    }
    curl_easy_setopt(ez, CURLOPT_HTTPHEADER, headers);
    if (type == HttpType::Get)
    {
        curl_easy_setopt(ez, CURLOPT_HTTPGET, 1);
    }
    else if (type == HttpType::Post)
    {
        //postData = curl_mime_init(ez);
        //part = curl_mime_addpart(postData);
        //curl_mime_data(part, postDataStr.data(), postDataStr.size());
        //curl_easy_setopt(ez, CURLOPT_MIMEPOST, postData);
        if (postDataStr.size() < std::numeric_limits<int32_t>::max())
        {
            curl_easy_setopt(ez, CURLOPT_POSTFIELDSIZE, postDataStr.size());
            curl_easy_setopt(ez, CURLOPT_POSTFIELDS, postDataStr.data());
        }
    }
    env.mt.lock();
    if (env.curlMap.find(ez) == env.curlMap.end())
    {
        CURLMcode merr = curl_multi_add_handle(env.multi, ez);
        if (merr == CURLM_OK)
        {
            CurlTaskData taskData;
            taskData.code = CURLE_OK;
            taskData.task = this->shared_from_this();
            env.curlMap.emplace(ez, std::move(taskData));
        }
        else
        {
            CurlTaskData taskData;
            taskData.mcode = merr;
            taskData.task = this->shared_from_this();
            env.failed.push_back(taskData);
        }
    }
    env.mt.unlock();
}

void CurlEasyTask::Stop(CurlAsyncEngine& env)
{
    auto _ = do_lock(env.mt);
    {
        auto it = env.curlMap.find(ez);
        if (it != env.curlMap.end())
        {
            CURLMcode merr = curl_multi_remove_handle(env.multi, ez);
            env.curlMap.erase(it);
            this->canceled = true;
        }
    }
}

void CurlEasyTask::OnFinish(CurlAsyncEngine & env, CurlEasyTaskResult& result)
{
    CURLMcode merr = CURLM_OK;
    auto _ = do_lock(env.mt);
    {
        decltype(env.curlMap.find(ez)) it;
        it = env.curlMap.find(ez);
        if (it != env.curlMap.end())
        {
            // returns ok if it was already removed... sigh
            CURLMcode merr = curl_multi_remove_handle(env.multi, ez);
            result.cError = it->second.code;
            env.curlMap.erase(it);
        }
    }
    result.canceled = canceled;
    result.mError = merr;
    result.data = std::move(outStr);
    result.file = std::move(file);

    curl_easy_getinfo(ez, CURLINFO_RESPONSE_CODE, &result.httpCode);
}

void CurlEasyTask::SetUrl(char const * url)
{
    curl_easy_setopt(ez, CURLOPT_URL, url);
}

void CurlEasyTask::SetHeader(std::string const & name, std::string const & value)
{
    std::string full = std::format("{}: {}", name, value);
    headers = curl_slist_append(headers, full.c_str());
}

void CurlEasyTask::ClearHeaders()
{
    curl_easy_setopt(ez, CURLOPT_HTTPHEADER, nullptr);
    curl_slist_free_all(headers);
    headers = nullptr;
}

CurlAsyncEngine::CurlAsyncEngine()
{
    multi = curl_multi_init();
}

CurlAsyncEngine::~CurlAsyncEngine()
{
    CURLMcode merr = curl_multi_cleanup(multi);
    
}

void CurlAsyncEngine::update(ProcessorUpdate<CurlAsyncEngine>& result)
{
    int rh = 0;
    CURLMcode merr = curl_multi_perform(multi, &rh);

    {
        auto _ = do_lock(mt);
        for (auto&& f : failed)
        {
            result.completedTaskList.emplace_back(std::move(f.task));
        }
        failed.clear();

        int msgRem = 0;
        while (CURLMsg* msg = curl_multi_info_read(multi, &msgRem))
        {
            if (msg->msg == CURLMSG_DONE)
            {
                decltype(curlMap.find(msg->easy_handle)) it;
                std::shared_ptr<AsyncTaskBase<CurlAsyncEngine>> task;
                it = curlMap.find(msg->easy_handle);
                if (it != curlMap.end())
                {
                    it->second.code = msg->data.result;
                    task = it->second.task;
                }
                if (task)
                {
                    result.completedTaskList.emplace_back(task);
                }
            }
        }
    }

}
