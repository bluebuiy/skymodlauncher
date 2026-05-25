
#pragma once

#include <curl/curl.h>

namespace curl
{
    #if 1
    template <typename T, void (*DELFUNC)(T *)>
    struct wrapper
    {
        T* ptr = nullptr;

        wrapper(T * p)
            : ptr(p)
        {}

        ~wrapper()
        {
            DELFUNC(ptr);
        }

        operator T*() const
        {
            return ptr;
        }

        wrapper(wrapper && o)
        {
            ptr = o;
        }

        wrapper & operator=(wrapper && o)
        {
            DELFUNC(ptr);
            ptr = o;
            o.ptr = nullptr;
        }

        T* release()
        {
            T* ret = ptr;
            ptr = nullptr;
            return ret;
        }

    };

    using url = wrapper<CURLU, curl_url_cleanup>;
    using curl = wrapper<CURL, curl_easy_cleanup>;

    #else
    
    struct url
    {
        CURLU* curlu = nullptr;

        url(CURLU* u)
            : curlu(u)
        {}

        ~url()
        {
            curl_url_cleanup(curlu);
        }

        operator CURLU*() const
        {
            return curlu;
        }

    };
    
#endif


}


