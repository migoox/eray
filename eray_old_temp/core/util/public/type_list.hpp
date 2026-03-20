#pragma once
#include <variant>

namespace eray::util {
template <typename... Ts>
struct TypeList {};

/**
 * @brief Represents a list of types.
 *
 * This struct is used as a compile-time container to hold a variadic list of types.
 *
 * @tparam Ts The types to include in the list.
 *
 * @example
 * TypeList<int, float, double> myTypes;
 */
template <typename TTypeList>
struct VariantFromTypeList;

/**
 * @brief Converts a TypeList to a std::variant type.
 *
 * This struct extracts all types from a TypeList and wraps them in a std::variant.
 *
 * @tparam TTypeList The type list to convert.
 *
 * @example
 * using MyList = TypeList<int, float>;
 * using MyVariant = VariantFromTypeList<MyList>::type;  // std::variant<int, float>
 */
template <typename... TTypeList>
struct VariantFromTypeList<TypeList<TTypeList...>> {
  using type = std::variant<TTypeList...>;
};

template <typename T, typename TList>
struct ConceptFromTypeList;

/**
 * @brief Creates a concept that checks if the type is in the type list.
 *
 * Uses std::disjunction over std::is_same to determine if T matches any type in Ts.
 *
 * @tparam T
 * @tparam TTypeList
 *
 * using MyList = TypeList<int, float>;
 * template <typename T>
 * concept MyConcept = ConceptFromTypeList<T, MyList>;
 */
template <typename T, typename... TTypeList>
struct ConceptFromTypeList<T, TypeList<TTypeList...>> : std::disjunction<std::is_same<T, TTypeList>...> {};

}  // namespace eray::util
