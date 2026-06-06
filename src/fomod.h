#pragma once

#include <unordered_map>
#include <vector>
#include <variant>
#include <string>
#include <filesystem>
#include <optional>
#include <unordered_map>

namespace fomod
{

    struct Flags
    {
        std::unordered_map<std::string, std::string> flags;
        std::string GetFlag(std::string const & f) const
        {
            auto it = flags.find(f);
            if (it == flags.end())
            {
                return "";
            }
            return it->second;
        }

        void SetFlag(std::string const & f, std::string const & v)
        {
            flags.insert_or_assign(f, v);
        }

        void Apply(Flags const & f)
        {
            for (auto&& sf : f.flags)
            {
                flags.insert_or_assign(sf.first, sf.second);
            }
        }
    };

    // ? idk if these are suggestions or 
    // requirements. ie if a thing is set
    // to required, should it fail if it's not selected?
    enum class FileUsability
    {
        Required,       // cannot deselect
        Optional,   
        Recommended,    // starts selected
        NotUsable,      // cannot select
        CouldBeUsable,
    };

    enum class ConditionType
    {
        Flag,           // flagDependency
        File,           // fileDependency
        GameVersion,    // gameDependency : versionDependency
        FomVersion,     // fommDependency : versionDependency
    };

    struct FlagCondition
    {
        std::string key;
        std::string value;
    };

    // schema is not specific on what to do with these.
    // should be more neutral. Missing implies a problem.
    enum class FileStatus
    {
        Missing,
        Inactive,
        Active
    };

    struct FileDependency
    {
        std::string file;
        FileStatus status;
    };

    // there is no specification for 
    // versioning, so version dependencies
    // are unusable and will be ignored.

    struct GameVerDependency
    {
        std::string version;
    };

    struct FomVerDependency
    {
        std::string version;
    };

    struct ConditionTreeNode;
    using ConditionVar = std::variant<FlagCondition, FileDependency, GameVerDependency, FomVerDependency, ConditionTreeNode>;

    enum class ConditionNodeType
    {
        Leaf,
        And,
        Or,
        True,
    };

    struct ConditionTreeNode
    {
        ConditionNodeType type = ConditionNodeType::True;
        std::vector<ConditionVar> children;
    };

    struct FileUseCondition
    {
        FileUsability use = FileUsability::Optional;
        ConditionTreeNode condition;
    };

    struct FileUse
    {
        FileUsability defaultUse;
        std::vector<FileUseCondition> conditionalUsability;
    };

    enum class FileAction
    {
        // move contents of src to dst
        DirToDir,
        // move src to dst
        FileToFile,
    };

    // "file" and "folder" install items are the same...
    struct FileInstall
    {
        FileAction action = FileAction::DirToDir;
        std::string source;
        std::optional<std::string> destination;
        bool alwaysInstall = false;
        bool installIfUsable = false;
        int priority = 0;
    };

    struct ConditionalInstall
    {
        ConditionTreeNode condition;
        std::vector<FileInstall> files;
    };

    struct FileStates
    {
        std::vector<std::string> filePaths;
        bool HasFile(std::string const & path)
        {
            return false;
        }
    };

    enum class Order
    {
        Explicit,
        Ascending,
        Descending,
    };

    // breaking from the schema: nobody is ever gonna use anything except explicit.
    // It's probably an error if they do.
    constexpr Order DEFAULT_ORDER = Order::Explicit;

    struct FileSubGroup // aka "plugin"
    {
        std::string name;
        std::string description;
        std::string imagePath;
        std::vector<FileInstall> files;
        std::vector<FlagCondition> setFlags;
        FileUse useCondition;
    };


    enum class SelectionType
    {
        AtLeastOne,
        AtMostOne,
        ExactlyOne,
        All,
        Any,
    };

    struct InstallSubStep // aka "group"
    {
        std::string name;
        Order order = DEFAULT_ORDER;
        SelectionType selectType;
        std::vector<FileSubGroup> selections;
    };

    struct InstallStep
    {
        std::string name;
        ConditionTreeNode visible;
        std::vector<InstallSubStep> subSteps;
        Order order;
    };

    struct InstallerTitleInfo
    {
        std::string name;
        std::string position;
    };

    struct Fomod
    {
        InstallerTitleInfo titleLayout;
        std::string name;
        std::string logoPath;
        ConditionTreeNode globalDependencies;
        std::vector<FileInstall> alwaysInstall;
        Order stepOrder = DEFAULT_ORDER;
        std::vector<InstallStep> installSteps;
        std::vector<ConditionalInstall> conditionalInstall;
    };

    template <typename T>
    struct UniqueVec
    {
        std::vector<T> values;
        void add(T const & t)
        {
            for (int i = 0; i < values.size(); ++i)
            {
                if (values[i] == t)
                {
                    return;
                }
            }
            values.emplace_back(t);
        }

        void remove(T const & t)
        {
            for (auto it = values.begin(); it != values.end(); ++it)
            {
                if (*it == t)
                {
                    values.erase(it);
                    return;
                }
            }
        }

        bool contains(T const & t)
        {
            for (auto&& v : values)
            {
                if (v == t)
                {
                    return true;
                }
            }
            return false;
        }
    };

    struct SubstepState
    {
        UniqueVec<int> choices;
        Flags setFlags;
    };

    struct StepState
    {
        std::vector<SubstepState> substeps;
    };

    struct Eval
    {
        std::vector<StepState> stepCache;
        int currentStep = 0;
        int currentSubStep = 0;
    };

    //struct FileCheck
    //{
    //    std::string path;
    //    mutable FileStatus status;// = FileStatus::Missing;
    //};

    struct Option
    {
        bool selected = false;
        std::string optionText;
        std::string imagePath;
        std::string name;
        FileUsability usability = FileUsability::Optional;
    };

    struct SubstepInfo
    {
        // files to check for existence, generated from PrepareSubstep
        std::unordered_map<std::string, FileStatus> fileChecks;

        // selections for current step, gnerated from EvalSubstep
        SelectionType optionType;
        std::vector<Option> options;
        std::string name;
        std::string stepName;
    };

    struct FileInstallAction
    {
        FileAction action;
        std::string from;
        std::string to;
        int priority = 0;
    };

    struct InstallActions
    {
        std::vector<FileInstallAction> actions;
    };


    void convert_path(std::string & path);

    std::optional<Fomod> Load(std::filesystem::path const & path);

    // retrives files that need to be queried by the mod manager
    SubstepInfo PrepareSubstep(Fomod const & fm, Eval const & eval);

    // evaluates conditionals for available steps and options,
    // and outputs the current options.
    // returns true if the current step/substep is visible.
    // returns false when it's not visible, and advances the stage to the next step.
    bool EvalSubstep(Fomod const & fm, Eval & eval, SubstepInfo & ss);

    // applies selected options and advances to the next step.
    // returns false if selected options are inconsistent with selection type,
    // and does not advance the step.
    bool ApplySubstep(Fomod const & fm, Eval & eval, SubstepInfo & ss);

    // checks if all user interaction has been completed (completed the last step).
    bool Configured(Fomod const & fm, Eval const & eval);

    // same as PrepareSubstep, but for the conditional file installs.
    SubstepInfo PrepareInstallActions(Fomod const & m, Eval const & eval);
    
    // determines file install actions, with paths being relative to the fomod directory.
    InstallActions GetInstallActions(Fomod const & m, Eval const & eval, SubstepInfo const & ss);



};




