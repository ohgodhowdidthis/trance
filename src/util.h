#ifndef TRANCE_UTIL_H
#define TRANCE_UTIL_H
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

inline bool ext_is(const std::string& path, const std::string& ext)
{
  std::string lower = path;
  for (char& c : lower) {
    c = tolower(c);
  }
  return lower.length() >= ext.length() + 1 &&
      lower.substr(lower.length() - ext.length() - 1) == "." + ext;
}

inline std::mt19937& get_mersenne_twister()
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
  return dist(get_mersenne_twister());
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

// Chooses randomly from elements. Elements start with 0 priority, which can
// be increased or decreased; calling next() gets a random element among all
// those with the highest priority.
class Shuffler {
public:
  Shuffler(std::size_t size)
  : _size{size}
  {
    _enabled_count[-1] = size;
    for (std::size_t i = 0; i < size; ++i) {
      _enabled.push_back(0);
    }
  }

  void increase(std::size_t index)
  {
    auto& v = _enabled[index];
    if (!_enabled_count.count(v)) {
      _enabled_count[v] = 0;
    }
    ++_enabled_count[v];
    ++v;
  }

  void decrease(std::size_t index)
  {
    auto& v = _enabled[index];
    --v;
    if (!_enabled_count.count(v)) {
      _enabled_count[v] = _size;
    }
    --_enabled_count[v];
  }

  void modify(std::size_t index, int32_t amount)
  {
    for (; amount > 0; --amount) {
      increase(index);
    }
    for (; amount < 0; ++amount) {
      decrease(index);
    }
  }

  std::size_t next() const
  {
    for (auto it = _enabled_count.rbegin(); it != _enabled_count.rend(); ++it) {
      auto r = random(it->second);
      std::size_t t = 0;
      for (std::size_t i = 0; i < _size; ++i) {
        t += _enabled[i] > it->first;
        if (r < t) {
          return i;
        }
      }
    }
    return _size ? random(_size) : -1;
  }

private:
  std::size_t _size;
  // _enabled_count[n] == | k s.t. _enabled[k] > n |.
  std::map<int32_t, std::size_t> _enabled_count;
  std::vector<int32_t> _enabled;
};

#endif