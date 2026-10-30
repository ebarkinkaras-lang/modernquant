#pragma once

#include <concepts>
#include <type_traits>

namespace quant {

// Chapter 10: Standard Library Concepts and User-Defined Concepts
template<typename T>
concept Number = std::integral<T> || std::floating_point<T>;

template<typename T>
concept FloatingPoint = std::floating_point<T>;

template<typename T>
concept CanAddAndSubtract = requires(T a, T b) {
    { a + b } -> std::same_as<T>;
    { a - b } -> std::same_as<T>;
};

template<typename T>
concept CanMultiply = requires(T a, T b) {
    { a * b } -> std::same_as<T>;
};

} // namespace quant
