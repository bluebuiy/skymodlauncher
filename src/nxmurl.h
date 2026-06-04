
#pragma once

#include <string>

struct NxmModFileUrl
{
    std::string game;
    int modId = 0;
    int fileId = 0;
    std::string key;
    std::string expires;
};


struct NxmCollectionUrl
{
    std::string game;
    std::string slug;
    int rev = -1;
};

inline bool operator==(NxmCollectionUrl const & a, NxmCollectionUrl const & b)
{
    return a.game == b.game && a.slug == b.slug && a.rev == b.rev;
}

