#ifndef TRANCE_UTIL_H
#define TRANCE_UTIL_H

#include <random>
#include <type_traits>

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
T random_excluding(const T& max, const T& exclude)
{
  if (max == 0) {
    throw std::runtime_error("random() called with maximum 0");
  }
  if (max == 1) {
    return 0;
  }
  if (exclude < 0 || exclude >= max) {
    return random(max);
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

template<typename T>
class Shuffler {
public:
  Shuffler(T& data)
  : _data(data)
  , _enabled(data.size(), true)
  , _enabled_count(data.size())
  , _last_enabled_id(data.empty() ? 0 : random(data.size()))
  , _last_disabled_id(data.empty() ? 0 :  random(data.size())) {}

  bool empty() const
  {
    return _data.empty();
  }

  std::size_t size() const
  {
    return _data.size();
  }

  std::size_t enabled_count() const
  {
    return _enabled_count;
  }

  void set_enabled(std::size_t index, bool enabled)
  {
    if (_enabled[index] != enabled) {
      _enabled_count += (enabled ? 1 : -1);
    }
    _enabled[index] = enabled;
  }

  // Get some value, but not the same one as last time.
  const typename T::value_type& next(bool get_enabled = true) const
  {
    static T::value_type empty;
    if (get_enabled ? !_enabled_count : _enabled_count == _data.size()) {
      return empty;
    }
    return get(next_index(get_enabled));
  }

  template<typename U = T,
           typename std::enable_if<!std::is_const<U>::value>::type* = nullptr>
  typename T::value_type& next(bool get_enabled = true)
  {
    static T::value_type empty;
    if (get_enabled ? !_enabled_count : _enabled_count == _data.size()) {
      return empty;
    }
    return get(next_index(get_enabled));
  }

  const typename T::value_type& get(std::size_t index) const
  {
    return *(_data.begin() + index);
  }

  template<typename U = T,
           typename std::enable_if<!std::is_const<U>::value>::type* = nullptr>
  typename T::value_type& get(std::size_t index)
  {
    return *(_data.begin() + index);
  }

  std::size_t next_index(bool get_enabled = true) const
  {
    auto count = get_enabled ? _enabled_count : _data.size() - _enabled_count;
    auto& last_id = get_enabled ? _last_enabled_id : _last_disabled_id;
    if (!count) {
      return -1;
    }

    std::size_t last_tindex = 0;
    for (std::size_t i = 0; i < last_id; ++i) {
      last_tindex += _enabled[i] == get_enabled;
    }
    std::size_t random_tindex =
      count > 1 && _enabled[last_id] == get_enabled ?
      random_excluding(count, last_tindex) : random(count);

    std::size_t tindex = 0;
    for (std::size_t i = 0; i < _data.size(); ++i) {
      tindex += _enabled[i] == get_enabled;
      if (random_tindex < tindex) {
        return last_id = i;
      }
    }
    return -1;
  }

private:

  T& _data;
  std::vector<bool> _enabled;
  std::size_t _enabled_count;
  mutable std::size_t _last_enabled_id;
  mutable std::size_t _last_disabled_id;
  
};

#endif