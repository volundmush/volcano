#pragma once
#include <unordered_map>
#include <functional>
#include <string>
#include <expected>
#include "PartialMatch.hpp"

#include <enchantum/enchantum.hpp>

namespace vol::util
{

    template <typename FlagEnum, typename MapType = std::unordered_map<std::string, FlagEnum>>
        requires std::is_enum_v<FlagEnum>
    auto getEnumMap(const std::function<bool(FlagEnum v)> &filter = {})
    {
        MapType flag_map;
        for (const auto &[val, name] : enchantum::entries<FlagEnum>)
        {
            if (filter && !filter(val))
                continue;
            flag_map[std::string(name)] = val;
        }
        return flag_map;
    }

    template <typename FlagEnum, typename ListType = std::vector<FlagEnum>>
        requires std::is_enum_v<FlagEnum>
    auto getEnumList(const std::function<bool(FlagEnum v)> &filter = {})
    {
        ListType flag_list;
        for (const auto &[val, name] : enchantum::entries<FlagEnum>)
        {
            if (filter && !filter(val))
                continue;
            flag_list.emplace_back(val);
        }
        return flag_list;
    }

    template <typename FlagEnum, typename ListType = std::vector<std::string>>
        requires std::is_enum_v<FlagEnum>
    auto getEnumNameList(const std::function<bool(FlagEnum v)> &filter = {})
    {
        ListType flag_list;
        for (const auto &[val, name] : enchantum::entries<FlagEnum>)
        {
            if (filter && !filter(val))
                continue;
            flag_list.emplace_back(name);
        }
        return flag_list;
    }

    template <typename T>
        requires std::is_enum_v<T>
    std::expected<T, std::string> chooseEnum(std::string_view arg, std::string_view context, const std::function<bool(T)> filter = {})
    {
        auto emap = getEnumMap<T>(filter);

        auto res = partialMatch(arg, emap);
        if (!res)
        {
            return std::unexpected("No match found for " + std::string(context) + " '" + std::string(arg) + "'. " + res.error());
        }
        return res.value()->second;
    }

    template <typename EnumType>
        requires std::is_enum_v<EnumType>
    std::expected<std::string, std::string> handleSetEnum(EnumType &field, std::string_view arg, std::string_view fieldName, const std::function<bool(EnumType)> &filter = {})
    {
        if (arg.empty())
        {
            return fmt::format("You must provide a value for {}.", fieldName);
        }
        auto res = chooseEnum<EnumType>(arg, std::string(fieldName), filter);
        if (!res)
        {
            return std::unexpected(res.error());
        }
        field = res.value();
        return fmt::format("Set {} to {}.", fieldName, enchantum::to_string(field));
    }

}