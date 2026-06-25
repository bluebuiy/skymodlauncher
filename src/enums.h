
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
    // added through manual submission
    Manual
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
