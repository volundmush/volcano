#pragma once
#include <boost/algorithm/string.hpp>
#include <expected>

namespace vol::util
{

  struct default_key_t
  {
    template <class T>
    std::string operator()(const T &v) const
    {
      using U = std::decay_t<T>;
      if constexpr (std::is_same_v<U, std::string>)
      {
        return v;
      }
      else if constexpr (std::is_arithmetic_v<U>)
      {
        return std::to_string(v);
      }
      else if constexpr (requires { v.first; })
      { // pairs / map values
        return (*this)(v.first);
      }
      else if constexpr (requires { std::string{v}; })
      { // anything convertible to string
        return std::string{v};
      }
      else
      {
        static_assert(sizeof(T) == 0,
                      "No default key for this type. Provide a key extractor returning std::string.");
      }
    }
  };

  template <class Range,
            class KeyFn = default_key_t>
  auto partialMatch(std::string_view match_text,
                    Range &&range,
                    bool exact = false,
                    KeyFn key = {})
      -> std::expected<std::ranges::iterator_t<Range>, std::string>
  {
    using std::begin;
    using std::end;
    using It = std::ranges::iterator_t<Range>;

    // collect (key, iterator) so we can return the iterator directly
    std::vector<std::pair<std::string, It>> items;
    for (It it = begin(range); it != end(range); ++it)
    {
      items.emplace_back(key(*it), it);
    }

    std::sort(items.begin(), items.end(),
              [](auto &a, auto &b)
              { return a.first < b.first; });

    for (auto &[k, it] : items)
    {
      if (boost::iequals(k, match_text) ||
          (!exact && boost::istarts_with(k, match_text)))
      {
        return it;
      }
    }

    // build "choices" string
    std::string choices;
    for (size_t i = 0; i < items.size(); ++i)
    {
      if (i)
        choices += ", ";
      choices += items[i].first;
    }

    return std::unexpected("Choices are: " + choices);
  }

}