#ifndef TRANCE_THEME_H
#define TRANCE_THEME_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <google/protobuf/repeated_field.h>
#include "image.h"
#include "util.h"

namespace trance_pb {
  class Session;
  class Theme;
}

// Theme consists of images, animations, and associated text strings.
class Theme {
public:

  Theme(const trance_pb::Theme& proto);
  Theme(const Theme& theme);

  // Get a random loaded in-memory Image, text string, etc.
  //
  // Note: these are called from the main rendering thread and can upload
  // images from RAM to video memory on-demand. The other loading functions
  // are called from the async_update thread and load images from files
  // into RAM when requested.
  Image get_image() const;
  Image get_animation(std::size_t frame) const;
  const std::string& get_text() const;
  const std::string& get_font() const;

  // Set the target number of images this set should keep in memory.
  // Once changed, the asynchronous image-loading thread will gradually
  // load/unload images until we're at the target.
  void set_target_load(std::size_t target_load);

  // Randomly swap out one in-memory image for another unloaded one.
  void perform_swap();
  // Perform at most one load or unload towards the target.
  void perform_load();
  // Perform all loads/unloads to reach the target.
  void perform_all_loads();

  // How many images are actually loaded.
  bool all_loaded() const;
  std::size_t loaded() const;

private:

  void load_image_internal();
  void unload_image_internal();
  void load_animation_internal();
  void unload_animation_internal();

  using StringShuffler =
      Shuffler<google::protobuf::RepeatedPtrField<std::string>>;
  StringShuffler _image_paths;
  StringShuffler _animation_paths;
  StringShuffler _font_paths;
  StringShuffler _text_lines;

  std::size_t _target_load;
  std::unordered_map<std::size_t, Image> _images;
  std::vector<Image> _animation_images;
  mutable std::mutex _image_mutex;
  mutable std::mutex _animation_mutex;

};

// ThemeBank keeps two Themes active at all times with a number of images
// in memory each so that a variety of these images can be displayed with no
// load delay. It also asynchronously loads a third theme into memory so that
// the active themes can be swapped out.
class ThemeBank {
public:

  ThemeBank(const trance_pb::Session& session);
  // Get the main or alternate theme.
  const Theme& get(bool alternate = false) const;

  // Call to upload a random image from the next theme which has been loaded
  // into RAM but not video memory.
  // This has to happen on the main rendering thread since OpenGL contexts
  // are single-threaded by default, but this function call can be timed to
  // mitigate the upload cost of switching active themes.
  void maybe_upload_next();

  // If the next theme has been fully loaded, swap it out for one of the two
  // active themes.
  bool change_themes();

  // Called from separate update thread to perform async loading/unloading.
  void async_update();

private:

  static const std::size_t switch_cooldown = 500;

  std::size_t _prev;
  std::size_t _a;
  std::size_t _b;
  std::size_t _next;

  std::unordered_map<std::string, trance_pb::Theme> _theme_map;
  std::vector<Theme> _themes;
  uint32_t _image_cache_size;
  uint32_t _updates;
  std::atomic<uint32_t> _cooldown;

};

#endif