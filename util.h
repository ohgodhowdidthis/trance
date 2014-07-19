#ifndef TRANCE_UTIL_H
#define TRANCE_UTIL_H

#include <random>
#include <iostream>

inline std::mt19937& mersenne_twister()
{
  static std::random_device rd;
  static std::mt19937 mt{rd()};
  return mt;
}

template<typename T>
T random(const T& max)
{
  static_assert(std::is_integral<T>::value, "random<T> needs integral type T");
  std::uniform_int_distribution<T> dist{0, max - 1};
  return dist(mersenne_twister());
}

template<typename T>
T random_excluding(const T& max, const T& exclude)
{
  if (max == 0) {
    throw std::runtime_error("random() called with maximum 0");
  }
  if (max == 1) {
    return 0;
  }
  T t = random(max - 1);
  return t >= exclude ? 1 + t : t;
}

template<typename T>
bool random_chance(const T& divisor)
{
  return random(divisor) == 0;
}

inline bool random_chance()
{
  return random_chance(2);
}

#endif