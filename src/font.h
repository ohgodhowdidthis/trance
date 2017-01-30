#ifndef TRANCE_FONT_H
#define TRANCE_FONT_H

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#pragma warning(push, 0)
#include <SFML/Graphics.hpp>
#pragma warning(pop)

namespace trance_pb
{
  class Session;
}

// Wrapper for an sf::Font.
class Font
{
public:
  Font(const std::string& path, uint32_t large_char_size, uint32_t small_char_size);

  struct vertex {
    float x;
    float y;
    float u;
    float v;
  };

  const std::string& get_path() const;
  void bind_texture(bool large) const;
  sf::Vector2f get_size(const std::string& text, bool large) const;
  std::vector<vertex> get_vertices(const std::string& text, bool large) const;

private:
  struct rectangle {
    sf::Vector2f min;
    sf::Vector2f max;
  };

  uint32_t char_size(bool large) const;
  rectangle compute_size(const std::string& text, bool large) const;

  std::string _path;
  uint32_t _large_char_size;
  uint32_t _small_char_size;
  std::unique_ptr<sf::Font> _font;
};

// An LRU cache for font objects.
class FontCache
{
public:
  FontCache(const std::string& root_path, const trance_pb::Session& session,
            uint32_t large_char_size, uint32_t small_char_size, uint32_t font_cache_size);
  const Font& get_font(const std::string& font_path) const;

private:
  std::string _root_path;
  std::vector<std::string> _paths;
  uint32_t _large_char_size;
  uint32_t _small_char_size;
  uint32_t _font_cache_size;

  mutable std::size_t _last_id;
  mutable std::list<Font> _list;
  mutable std::unordered_map<std::string, Font*> _map;
};

#endif