#include "fonts.h"
#include "director.h"
#include "util.h"
#include <algorithm>

FontCache::FontCache(const std::vector<std::string>& paths)
: _last_id{random(paths.size())}
, _paths{paths}
{
}

const std::string& FontCache::get_path(bool force_change) const
{
  const static std::string empty;
  if (!_paths.size()) {
    return empty;
  }
  if (_paths.size() == 1) {
    return _paths.front();
  }
  _last_id = force_change ?
      random_excluding(_paths.size(), _last_id) : random(_paths.size());
  return _paths[_last_id];
}

sf::Text FontCache::get_text(
    const std::string& text,
    const std::string& font_path, std::size_t char_size) const
{
  char_size -= char_size % char_size_lock;
  Font::key_t k{font_path, char_size};

  auto it = _map.find(k);
  if (it != _map.end()) {
    auto jt = _list.begin();
    while (&*jt != it->second) {
      ++jt;
    }
    _list.splice(_list.begin(), _list, jt);
    return sf::Text{text, *it->second->font, char_size};
  }

  _list.emplace_front(font_path, char_size);
  _map.emplace(k, &_list.front());
  if (_list.size() > std::max(2u, Settings::settings.font_cache_size)) {
    _map.erase(_list.back().key);
    _list.pop_back();
  }
  return sf::Text{text, *_list.front().font, char_size};
}