


#include "pugixml.hpp"
#include "fomod.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>

namespace fomod {

bool is(pugi::xml_node const & n, char const * name)
{
    return std::strcmp(n.name(), name) == 0;
}

void convert_path(std::string & path)
{
    for (auto&& c : path)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }
}

ConditionNodeType loadConditionType(pugi::xml_node const & node)
{
    std::string v = node.attribute("operator").as_string();
    if (v == "And" || v == "")
    {
        return ConditionNodeType::And;
    }
    else if (v == "Or")
    {
        return ConditionNodeType::Or;
    }
    std::cout << "Unknown condition type " << v << std::endl;
    return ConditionNodeType::And;
}

FileStatus loadFileStatus(pugi::xml_node const & value)
{
    std::string s = value.attribute("state").as_string();
    if (s == "Missing")
    {
        return FileStatus::Missing;
    }
    else if (s == "Inactive")
    {
        return FileStatus::Inactive;
    }
    else if (s == "Active")
    {
        return FileStatus::Active;
    }
    std::cout << "Unknown file status: " << s << std::endl;
    return FileStatus::Active;
}

FileDependency loadFileDependency(pugi::xml_node const & value)
{
    FileDependency ret;
    ret.file = value.attribute("file").as_string();
    ret.status = loadFileStatus(value);
    return ret;
}

FlagCondition loadFlagCondition(pugi::xml_node const & value)
{
    FlagCondition ret;
    ret.key = value.attribute("flag").as_string();
    ret.value = value.attribute("value").as_string();
    return ret;
}

GameVerDependency loadGameVerDependency(pugi::xml_node const & value)
{
    return {};
}

FomVerDependency loadFomVerDependency(pugi::xml_node const & value)
{
    return {};
}

ConditionTreeNode loadCompositeCondition(pugi::xml_node const & value)
{
    ConditionTreeNode n;
    n.type = loadConditionType(value);
    for (auto&& child : value.children("fileDependency"))
    {
        n.children.emplace_back(loadFileDependency(child));
    }
    for (auto&& child : value.children("flagDependency"))
    {
        n.children.emplace_back(loadFlagCondition(child));
    }
    for (auto&& child : value.children("gameDependency"))
    {
        n.children.emplace_back(loadGameVerDependency(child));
    }
    for (auto&& child : value.children("fommDependency"))
    {
        n.children.emplace_back(loadFomVerDependency(child));
    }
    for (auto&& child : value.children("dependencies"))
    {
        n.children.emplace_back(loadCompositeCondition(child));
    }
    return n;
}

FileUsability loadFileUsability(pugi::xml_node const & value)
{
    std::string v = value.attribute("name").as_string();
    if (v == "Optional")
    {
        return FileUsability::Optional;
    }
    else if (v == "Required")
    {
        return FileUsability::Required;
    }
    else if (v == "Recommended")
    {
        return FileUsability::Recommended;
    }
    else if (v == "NotUsable")
    {
        return FileUsability::NotUsable;
    }
    else if (v == "CouldBeUsable")
    {
        return FileUsability::CouldBeUsable;
    }
    std::cout << "Unknown file use: " << v << std::endl;
    return FileUsability::CouldBeUsable;
}

FileUseCondition loadFileUseCondition(pugi::xml_node const & value)
{
    FileUseCondition cond;
    cond.use = loadFileUsability(value.child("type"));
    cond.condition = loadCompositeCondition(value.child("dependencies"));
    return cond;
}

FileUse loadFileUse(pugi::xml_node const & value)
{
    FileUse ret;
    if (auto t = value.child("type"))
    {
        ret.defaultUse = loadFileUsability(t);
    }
    else if (auto t = value.child("dependencyType"))
    {
        ret.defaultUse = loadFileUsability(t.child("defaultType"));
        auto patterns = t.child("patterns");
        for (auto&& child : patterns)
        {
            ret.conditionalUsability.emplace_back(loadFileUseCondition(child));
        }
    }
    return ret;
}

FlagCondition loadSetFlag(pugi::xml_node const & value)
{
    FlagCondition ret;
    ret.key = value.attribute("name").as_string();
    ret.value = value.child_value();
    return ret;
}

FileInstall loadFileInstall(pugi::xml_node const & node, FileAction action)
{
    FileInstall result;

    result.source = node.attribute("source").as_string();
    convert_path(result.source);
    auto destAttr = node.attribute("destination");
    if (!destAttr.empty())
    {
        result.destination = destAttr.as_string();
        convert_path(*result.destination);
    }
    result.alwaysInstall = node.attribute("alwaysInstall").as_bool(false);
    result.installIfUsable = node.attribute("installIfUsable").as_bool();
    result.priority = node.attribute("priority").as_int();
    result.action = action;
    
    return result;
}

std::vector<FileInstall> loadFileList(pugi::xml_node const & value)
{
    std::vector<FileInstall> ret;
    for (auto&& child : value.children("file"))
    {
        ret.emplace_back(loadFileInstall(child, FileAction::FileToFile));
    }
    for (auto&& child : value.children("folder"))
    {
        ret.emplace_back(loadFileInstall(child, FileAction::DirToDir));
    }
    return ret;
}

FileSubGroup loadFileSubGroup(pugi::xml_node const & value)
{
    FileSubGroup ret;
    ret.name = value.attribute("name").as_string();
    ret.description = value.child("description").child_value();
    ret.imagePath = value.child("image").attribute("path").as_string();
    auto fileList = value.child("files");
    ret.files = loadFileList(fileList);
    auto flagList = value.child("conditionFlags");
    for (auto&& child : flagList.children("flag"))
    {
        ret.setFlags.emplace_back(loadSetFlag(child));
    }
    ret.useCondition = loadFileUse(value.child("typeDescriptor"));
    


    return ret;
}

Order loadOrder(pugi::xml_node const & value)
{
    std::string order = value.attribute("order").as_string();
    if (order == "Explicit")
    {
        return Order::Explicit;
    }
    else if (order == "Ascending")
    {
        return Order::Ascending;
    }
    else if (order == "Descending")
    {
        return Order::Descending;
    }
    else if (order != "")
    {
        std::cout << "Unknown order " << order << std::endl;
    }
    return Order::Ascending;
}

SelectionType loadSelectionType(pugi::xml_node const & value)
{
    std::string st = value.attribute("type").as_string();
    if (st == "SelectAtLeastOne")
    {
        return SelectionType::AtLeastOne;
    }
    else if (st == "SelectAtMostOne")
    {
        return SelectionType::AtMostOne;
    }
    else if (st == "SelectExactlyOne")
    {
        return SelectionType::ExactlyOne;
    }
    else if (st == "SelectAll")
    {
        return SelectionType::All;
    }
    else if(st == "SelectAny")
    {
        return SelectionType::Any;
    }
    std::cout << "Unknown selection type " << st << std::endl;
    return SelectionType::Any;
}


InstallSubStep loadSubStep(pugi::xml_node const & value)
{
    InstallSubStep ss;
    ss.order = loadOrder(value);
    //ss.selectType = loadSelectionType(value);
    for (auto child : value.children("plugin"))
    {
        FileSubGroup sg = loadFileSubGroup(child);
        ss.selections.emplace_back(std::move(sg));
    }
    return ss;
}

InstallStep loadInstallStep(pugi::xml_node const & value)
{
    InstallStep step;
    step.visible = loadCompositeCondition(value.child("visible"));
    step.name = value.attribute("name").as_string();
    auto optGroups = value.child("optionalFileGroups");
    for (auto&& child : optGroups.children("group"))
    {
        // bro this plugins node is useless
        auto plugins = child.child("plugins");
        InstallSubStep ss = loadSubStep(plugins);
        ss.selectType = loadSelectionType(child);
        ss.name = child.attribute("name").as_string();
        step.subSteps.emplace_back(std::move(ss));
    }
    step.order = loadOrder(optGroups);
    return step;
}

std::optional<Fomod> Load(std::filesystem::path const & path)
{
    std::ifstream infile(path);
    if (!infile)
    {
        return {};
    }
    pugi::xml_document doc;
    auto pres = doc.load(infile);
    infile.close();

    if (pres.status != pugi::xml_parse_status::status_ok)
    {
        return {};
    }

    auto root = doc.root().child("config");

    // if there's no schema then just keep going :)
    auto schemaAttrib = root.attribute("xsi:noNamespaceSchemaLocation");
    if (schemaAttrib)
    {
        std::string schema = schemaAttrib.as_string();
        if (schema.find("ModConfig5.0.xsd") == std::string::npos)
        {
            return {};
        }
    }

    Fomod result;

    result.name = root.child("moduleName").child_value();
    result.logoPath = root.child("moduleImage").child_value();

    auto always = root.child("requiredInstallFiles");
    result.alwaysInstall = loadFileList(always);

    auto installSteps = root.child("installSteps");
    result.stepOrder = loadOrder(installSteps);

    for (auto&& child : installSteps)
    {
        if (child.name() == std::string("installStep"))
        {
            result.installSteps.emplace_back(loadInstallStep(child));
        }
    }

    auto patterns = root.child("conditionalFileInstalls").child("patterns");
    for (auto&& child : patterns.children("pattern"))
    {
        ConditionalInstall ci;
        ci.condition = loadCompositeCondition(child.child("dependencies"));
        ci.files = loadFileList(child.child("files"));
        result.conditionalInstall.emplace_back(std::move(ci));
    }

    return result;
}

template <typename T>
void IterateCond(ConditionTreeNode const & node, T && func)
{
    for (auto&& n : node.children)
    {
        if (std::holds_alternative<FileDependency>(n))
        {
            FileDependency const & dep = std::get<FileDependency>(n);
            func(dep.file);
        }
        else if (std::holds_alternative<ConditionTreeNode>(n))
        {  
            IterateCond(std::get<ConditionTreeNode>(n), func);
        }
    }
}

bool EvaluateCond(ConditionTreeNode const & node, Eval const & eval, SubstepInfo const & ss, Flags const & flags)
{
    bool value = (node.type == ConditionNodeType::And ? true : false);
    for (auto&& n : node.children)
    {
        bool b = false;
        if (std::holds_alternative<FileDependency>(n))
        {
            FileDependency const & dep = std::get<FileDependency>(n);
            auto it = ss.fileChecks.find(dep.file);
            FileStatus status;
            if (it == ss.fileChecks.end())
            {
                status = FileStatus::Missing;
            }
            else
            {
                status = it->second;
            }
            b = status == dep.status;
        }
        else if (std::holds_alternative<ConditionTreeNode>(n))
        {  
           b = EvaluateCond(std::get<ConditionTreeNode>(n), eval, ss, flags);
        }
        else if (std::holds_alternative<FlagCondition>(n))
        {
            auto& fc = std::get<FlagCondition>(n);
            b = fc.value == flags.GetFlag(fc.key);
        }
        if (node.type == ConditionNodeType::And)
        {
            value &= b;
        }
        else if (node.type == ConditionNodeType::Or)
        {
            value |= b;
        }
    }
    return value;
}

SubstepInfo PrepareSubstep(Fomod const & fm, Eval const & eval)
{
    if (eval.currentStep < fm.installSteps.size() && eval.currentSubStep < fm.installSteps[eval.currentStep].subSteps.size())
    {
    }
    else
    {
        return SubstepInfo{};
    }

    auto& step = fm.installSteps[eval.currentStep];
    auto& subStep = step.subSteps[eval.currentSubStep];

    SubstepInfo ret;

    auto addFunc = [&](std::string const & file){
        ret.fileChecks.emplace(file, FileStatus::Missing);
    };

    if (eval.currentSubStep == 0)
    {
        // do visibility
        IterateCond(step.visible, addFunc);
    }

    for (auto&& file : subStep.selections)
    {
        for (auto&& cuse : file.useCondition.conditionalUsability)
        {
            IterateCond(cuse.condition, addFunc);
        }
    }

    return ret;
}

Flags AccumulateFlags(Eval const & eval)
{
    Flags ret;
    for (auto&& step : eval.stepCache)
    {
        for (auto&& sub : step.substeps)
        {
            ret.Apply(sub.setFlags);
        }
    }
    return ret;
}

bool EvalSubstep(Fomod const & fm, Eval & eval, SubstepInfo & ss)
{
    if (eval.currentStep < fm.installSteps.size() && eval.currentSubStep < fm.installSteps[eval.currentStep].subSteps.size())
    {
    }
    else
    {
        return true;
    }

    auto& step = fm.installSteps[eval.currentStep];
    auto& subStep = step.subSteps[eval.currentSubStep];

    bool cond = true;
    Flags flags = AccumulateFlags(eval);

    if (eval.currentSubStep == 0)
    {
        cond = EvaluateCond(step.visible, eval, ss, flags);
    }

    ss.options.clear();

    if (cond)
    {
        ss.optionType = subStep.selectType;
        for (auto&& opt : subStep.selections)
        {
            Option o;
            o.imagePath = opt.imagePath;
            o.optionText = opt.description;
            o.name = opt.name;
            o.usability = opt.useCondition.defaultUse;
            for (auto&& cuse : opt.useCondition.conditionalUsability)
            {
                bool matches = EvaluateCond(cuse.condition, eval, ss, flags);
                if (matches)
                {
                    o.usability = cuse.use;
                    goto afterUseCalc;
                }
            }
        afterUseCalc:
            
            o.selected = opt.useCondition.defaultUse == FileUsability::Recommended || opt.useCondition.defaultUse == FileUsability::Required;

            ss.options.emplace_back(std::move(o));
        }
        ss.name = subStep.name;
        ss.stepName = step.name;
        return true;
    }
    else
    {
        eval.stepCache.emplace_back();
        eval.currentStep++;
        eval.currentSubStep = 0;
        return false;
    }
}

bool ApplySubstep(Fomod const & fm, Eval & eval, SubstepInfo & ss)
{
    if (eval.currentStep < fm.installSteps.size() && eval.currentSubStep < fm.installSteps[eval.currentStep].subSteps.size())
    {
    }
    else
    {
        return true;
    }

    auto& step = fm.installSteps[eval.currentStep];
    auto& subStep = step.subSteps[eval.currentSubStep];

    int optCount = 0;

    for (auto&& opt : ss.options)
    {
        optCount += opt.selected;
    }

    if (ss.optionType == SelectionType::AtLeastOne)
    {
        if (optCount == 0)
        {
            return false;
        }
    }
    else if (ss.optionType == SelectionType::AtMostOne)
    {
        if (optCount > 1)
        {
            return false;
        }
    }
    else if (ss.optionType == SelectionType::ExactlyOne)
    {
        if (optCount != 1)
        {
            return false;
        }
    }

    if (eval.currentSubStep == 0)
    {
        eval.stepCache.emplace_back();
    }

    SubstepState newSubState;

    for (auto&& opt : ss.options)
    {
        if (opt.selected)
        {
            int i = -1;
            for (int j = 0; j < subStep.selections.size(); ++j)
            {
                if (subStep.selections[j].name == opt.name)
                {
                    i = j;
                    break;
                }
            }

            if (i != -1)
            {
                newSubState.choices.add(i);
                for (auto&& f : subStep.selections[i].setFlags)
                {
                    newSubState.setFlags.SetFlag(f.key, f.value);
                }
            }
        }
    }

    eval.stepCache[eval.currentStep].substeps.emplace_back(std::move(newSubState));

    ++eval.currentSubStep;
    if (eval.currentSubStep == step.subSteps.size())
    {
        eval.currentSubStep = 0;
        eval.currentStep++;
    }

    return true;
}

bool Configured(Fomod const & fm, Eval const & eval)
{
    return eval.currentStep >= fm.installSteps.size();
}

SubstepInfo PrepareInstallActions(Fomod const & m, Eval const & eval)
{
    SubstepInfo ret;

    auto addFunc = [&](std::string const & file) {
        ret.fileChecks.emplace(file, FileStatus::Missing);
    };

    for (auto&& fileInstall : m.conditionalInstall)
    {
        IterateCond(fileInstall.condition, addFunc);
    }

    return ret;
}

InstallActions GetInstallActions(Fomod const & m, Eval const & eval, SubstepInfo const & ss)
{
    InstallActions ret;

    for (auto&& req : m.alwaysInstall)
    {
        auto& act = ret.actions.emplace_back();
        act.action = req.action;
        act.from = req.source;
        act.to = req.destination.has_value() ? *req.destination : req.source;
        act.priority = req.priority;
    }

    Flags flags = AccumulateFlags(eval);

    for (auto&& fileInstall : m.conditionalInstall)
    {
        if (EvaluateCond(fileInstall.condition, eval, ss, flags))
        {
            for (auto&& action : fileInstall.files)
            {
                auto& act = ret.actions.emplace_back();
                act.action = action.action;
                act.from = action.source;
                act.to = action.destination.has_value() ? *action.destination : action.source;
                act.priority = action.priority;

                // not sure how alwaysInstall should be handled
            }
        }
    }

    for (int i = 0; i < m.installSteps.size(); ++i)
    {
        auto& installStep = m.installSteps[i];
        for (int j = 0; j < installStep.subSteps.size(); ++j)
        {
            auto& subStep = installStep.subSteps[j];
            if (!eval.stepCache[i].substeps.empty())
            {
                for (auto&& index : eval.stepCache[i].substeps[j].choices.values)
                {
                    auto& opt = subStep.selections[index];
                    for (auto&& action : opt.files)
                    {
                        auto& act = ret.actions.emplace_back();
                        act.action = action.action;
                        act.from = action.source;
                        act.to = action.destination.has_value() ? *action.destination : action.source;
                        act.priority = action.priority;
                    }
                }
            }
        }
    }

    std::sort(ret.actions.begin(), ret.actions.end(), [](FileInstallAction const & a, FileInstallAction const & b) {
        return a.priority < b.priority;
    });

    return ret;
}







};



