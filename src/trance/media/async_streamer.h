#ifndef TRANCE_SRC_TRANCE_MEDIA_ASYNC_STREAMER_H
#define TRANCE_SRC_TRANCE_MEDIA_ASYNC_STREAMER_H
#include <common/media/image.h>
#include <common/media/streamer.h>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

// TODO: should really use image pools to avoid allocating every frame separately.
class AsyncStreamer
{
public:
  AsyncStreamer(const std::function<std::unique_ptr<Streamer>()>& load_function,
                size_t buffer_size);

  void maybe_upload_next(const std::function<void(const Image&)>& function);
  Image get_frame(const std::function<void(const Image&)>& function) const;
  void advance_frame(uint32_t global_fps, bool maybe_switch, bool force_switch);

  // Called from async update thread.
  void async_update(const std::function<void(const Image&)>& cleanup_function);

private:
  mutable std::mutex _swap_mutex;
  mutable std::mutex _old_mutex;
  struct Animation {
    std::unique_ptr<Streamer> streamer;
    std::vector<Image> buffer;
    std::size_t begin = 0;
    std::size_t size = 0;
    bool end = false;
  };
  std::function<std::unique_ptr<Streamer>()> _load_function;
  const size_t _buffer_size;
  Animation _a;
  Animation _b;
  Animation* _current;
  Animation* _next;
  std::deque<Image> _old_buffer;
  std::unique_ptr<Streamer> _old_streamer;

  float _update_counter = 0.f;
  std::size_t _index = 0;
  bool _backwards = false;
  bool _reached_end = false;
};

#endif