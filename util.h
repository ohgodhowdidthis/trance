#ifndef TRANCE_UTIL_H
#define TRANCE_UTIL_H

#include <random>

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
  Shuffler(const T& data)
    : _data(data)
    , _last_id(random(data.size())) {}

  std::size_t size() const
  {
    return _data.size();
  }

  bool empty() const
  {
    return _data.empty();
  }

  // Get some value, but not the same one as last time.
  const typename T::value_type& next() const
  {
    static T::value_type empty;
    if (_data.empty()) {
      return empty;
    }
    if (_data.size() == 1) {
      return *_data.begin();
    }
    _last_id = random_excluding(std::size_t(_data.size()), _last_id);
    return *(_data.begin() + _last_id);
  }

private:

  const T& _data;
  mutable std::size_t _last_id;
  
};

#endif