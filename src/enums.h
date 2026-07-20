
#pragma once


enum class ModInstallType
{
    Data,
    Root,
    Undetermined,
    Conflicting,
};

enum class FileSource
{
    Unknown,
    Nexus,
    CollectionBundle,
    // downloaded from non-nexus source
    Independent,
    // Local archive sourced from a file path
    Local,
    // No archive source, files added manualy
    Empty,
};

enum class ModDlState
{
    None = 0,
    UrlQuery,
    ModDownload,
    ModPaused,
    Error,
    Complete,
    Canceled,
};


char const * EnumStr(FileSource fs);


