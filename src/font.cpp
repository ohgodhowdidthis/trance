#include "font.h"
#include "director.h"
#include "util.h"

FontCache::FontCache(const std::string& root_path, uint32_t font_cache_size)
: _root_path{root_path}
, _font_cache_size{font_cache_size}
{
}

const Font& FontCache::get_font(
    const std::string& font_path, uint32_t char_size) const
{
  char_size -= char_size % char_size_lock;
  auto full_path = _root_path + "/" + font_path;
  Font::key_t k{full_path, char_size};

  auto it = _map.find(k);
  if (it != _map.end()) {
    auto jt = _list.begin();
    while (&*jt != it->second) {
      ++jt;
    }
    _list.splice(_list.begin(), _list, jt);
    return *it->second;
  }

  _list.emplace_front(full_path, char_size);
  _map.emplace(k, &_list.front());
  if (_list.size() > _font_cache_size) {
    _map.erase(_list.back().key);
    _list.pop_back();
  }
  return _list.front();
}