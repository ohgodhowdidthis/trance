#ifndef TRANCE_THEME_H
#define TRANCE_THEME_H

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "image.h"
#include "util.h"

#pragma warning(push, 0)
#include <google/protobuf/repeated_field.h>
#pragma warning(pop)

namespace trance_pb
{
class Program;
class Session;
class System;
class Theme;
}

// ThemeBank keeps two Themes active at all times with a number of images
// in memory each so that a variety of these images can be displayed with no
// load delay. It also asynchronously loads a third theme into memory so that
// the active themes can be swapped out.
class ThemeBank
{
public:
  ThemeBank(const std::string& root_path, const trance_pb::Session& session,
            const trance_pb::System& system, const trance_pb::Program& program);

  const std::string& get_root_path() const;
  void set_program(const trance_pb::Program& program);

  // Get a random loaded in-memory image, text string, etc.
  //
  // These are called from the main (rendering) thread and can upload
  // images from RAM to video memory on-demand.
  Image get_image(bool alternate);
  Image get_animation(bool alternate, std::size_t frame);
  const std::string& get_text(bool alternate, bool exclusive);
  const std::string& get_font(bool alternate);

  // Call to upload a random image from the next theme which has been loaded
  // into RAM but not video memory.
  //
  // This has to happen on the main rendering thread since OpenGL contexts
  // are single-threaded by default, but this function call can be timed to
  // mitigate the upload cost of switching active themes.
  void maybe_upload_next();

  // If the next theme has been fully loaded, swap it out for one of the two
  // active themes.
  bool change_themes();
  bool swaps_to_match_theme() const;

  // Called from separate update thread to perform async loading/unloading.
  void async_update();

private:
  uint32_t cache_per_theme() const;
  static const std::size_t switch_cooldown = 500;
  static const std::size_t last_image_count = 8;

  // Data for each possible theme.
  struct ThemeInfo {
    // Number of images.
    const std::size_t size;
    // Whether the theme is enabled in the theme shuffler.
    bool enabled;
    // Used for synchronizing image loads.
    std::mutex load_mutex;
    // Used for synchronizing theme changes.
    std::atomic<std::size_t> loaded_size;
    // Indexes of images that this theme has caused to be loaded.
    std::vector<std::size_t> loaded_index;
    // Shuffler for loading images; maps onto all_images.
    Shuffler load_shuffler;
    // Shuffler for picking loaded images; also maps onto all_images.
    Shuffler image_shuffler;
    // Shuffler for choosing animations. Maps on to all_animations.
    Shuffler animation_shuffler;
    // All font paths for this theme.
    std::vector<std::string> font_paths;
    // All texts for this theme.
    std::vector<std::string> text_lines;
    // Lookup from text to index.
    std::unordered_map<std::string, std::vector<std::size_t>> text_lookup;
    // Shuffler for choosing text lines. Maps on to text_lines above.
    Shuffler text_shuffler;
  };

  // Data for each possible image.
  struct ImageInfo {
    const std::string path;
    // Reference count; corresponds to ThemeInfo::loaded_index.
    uint32_t use_count;
    std::unique_ptr<Image> image;
  };

  struct AnimationInfo {
    std::atomic<bool> loaded = false;
    // Index into all_animations.
    std::size_t index = 0;
    std::mutex mutex;
    std::vector<Image> frames;
  };

  void advance_theme();
  bool all_loaded() const;
  bool all_unloaded() const;
  AnimationInfo& animation(std::size_t active_theme_index);

  // Called from the async_update thread and can load images from files
  // into RAM as necessary.
  void do_swap(std::size_t active_theme_index);
  void do_reconcile(ThemeInfo& theme);
  void do_load(ThemeInfo& theme);
  void do_unload(ThemeInfo& theme);
  void do_load_animation(ThemeInfo& theme, AnimationInfo& animation, bool only_unload);
  void do_video_upload(const Image& image) const;
  void do_purge();

  const std::string _root_path;
  // Data for all images.
  std::vector<ImageInfo> _all_images;
  std::vector<std::size_t> _last_images;
  std::vector<std::string> _all_animations;
  std::array<AnimationInfo, 4> _loaded_animations;
  std::atomic<std::size_t> _animation_index;
  std::string _last_text;

  // Maps theme name to index in theme vector.
  std::unordered_map<std::string, std::size_t> _theme_map;
  std::unordered_map<std::string, uint32_t> _enabled_theme_weights;
  std::string _pinned_theme;
  // Vector of themes.
  std::vector<std::unique_ptr<ThemeInfo>> _themes;
  // Currently-active themes in queue.
  std::array<std::atomic<ThemeInfo*>, 4> _active_themes;

  const uint32_t _image_cache_size;
  uint32_t _swaps_to_match_theme;
  uint32_t _updates;
  std::atomic<uint32_t> _cooldown;

  mutable std::mutex _purge_mutex;
  mutable std::vector<std::shared_ptr<sf::Image>> _purgeable_images;
};

#endif