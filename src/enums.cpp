
#include "enums.h"

char const * EnumStr(FileSource fs)
{
    switch (fs)
    {
        case FileSource::CollectionBundle:
            return "CollBundle";
        case FileSource::Independent:
            return "DirectURL";
        case FileSource::Local:
            return "Local";
        case FileSource::Nexus:
            return "Nexus";
        case FileSource::Empty:
            return "Empty";
        case FileSource::Unknown:
            return "Unknown";
    }
    return "Unknown!";
}

