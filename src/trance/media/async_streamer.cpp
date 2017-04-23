#include "async_streamer.h"
#include <common/util.h>

namespace
{
  std::size_t prev_index(std::size_t i, std::size_t buffer_size)
  {
    return (i + buffer_size - 1) % buffer_size;
  }
}

AsyncStreamer::AsyncStreamer(const std::function<std::unique_ptr<Streamer>()>& load_function,
                             size_t buffer_size)
: _load_function{load_function}, _buffer_size{buffer_size}
{
  _a.streamer = load_function();
  _a.buffer.resize(_buffer_size);
  _b.buffer.resize(_buffer_size);
  _current = &_a;
  _next = &_b;
  while (_a.streamer && !_a.end && _a.size < _buffer_size) {
    auto image = _a.streamer->next_frame();
    if (image) {
      _a.buffer[_a.size] = image;
      ++_a.size;
    } else {
      _a.end = true;
    }
  }
}

void AsyncStreamer::maybe_upload_next(const std::function<void(const Image&)>& function)
{
  {
    std::lock_guard<std::mutex> lock{_swap_mutex};
    if (_current->size) {
      function(_current->buffer[(_current->begin + random(_current->size)) % _buffer_size]);
    }
  }
  {
    std::lock_guard<std::mutex> lock{_swap_mutex};
    if (_next->size) {
      function(_next->buffer[(_next->begin + random(_next->size)) % _buffer_size]);
    }
  }
}

Image AsyncStreamer::get_frame(const std::function<void(const Image&)>& function) const
{
  std::lock_guard<std::mutex> lock{_swap_mutex};
  function(_current->buffer[_index]);
  Image image = _current->buffer[_index];
  return image;
}

void AsyncStreamer::advance_frame(uint32_t global_fps, bool maybe_switch, bool force_switch)
{
  std::lock_guard<std::mutex> lock{_swap_mutex};
  bool can_change = _current->streamer && _next->streamer && !_old_streamer &&
      (!_current->streamer->success() || !_current->size ||
       (maybe_switch && (_reached_end || force_switch) &&
        (_next->end || _next->size >= _buffer_size)));
  if (can_change) {
    std::swap(_current, _next);
    _next->begin = 0;
    _next->size = 0;
    _next->end = false;
    _reached_end = false;
    _backwards = false;
    _index = 0;
    std::lock_guard<std::mutex> lock{_old_mutex};
    _old_streamer.swap(_next->streamer);
    for (auto& image : _next->buffer) {
      _old_buffer.emplace_back(std::move(image));
    }
  }

  _update_counter += (120.f / global_fps) / 8.f;
  while (_update_counter > 1.f) {
    _update_counter -= 1.f;
    if (_backwards) {
      if (_index != _current->begin) {
        auto new_index = prev_index(_index, _buffer_size);
        if (new_index == _current->begin) {
          _index = prev_index(_index, _buffer_size);
        } else {
          _index = prev_index(_index, _buffer_size);
        }
      } else {
        _backwards = false;
        if (_index != prev_index(_current->begin + _current->size, _buffer_size)) {
          _index = (_index + 1) % _buffer_size;
        }
      }
    } else {
      if (_index != prev_index(_current->begin + _current->size, _buffer_size)) {
        _index = (_index + 1) % _buffer_size;
      } else {
        _backwards = true;
        if (_index != _current->begin) {
          auto new_index = prev_index(_index, _buffer_size);
          if (new_index == _current->begin) {
            _index = prev_index(_index, _buffer_size);
          } else {
            _index = prev_index(_index, _buffer_size);
          }
        }
      }
    }
  }
  if (_current->end && _index == prev_index(_current->begin + _current->size, _buffer_size)) {
    _reached_end = true;
  }
}

void AsyncStreamer::async_update(const std::function<void(const Image&)>& cleanup_function)
{
  {
    std::lock_guard<std::mutex> lock{_old_mutex};
    if (_old_streamer) {
      _old_streamer.reset();
    }
    if (!_old_buffer.empty()) {
      cleanup_function(_old_buffer.front());
      _old_buffer.pop_front();
    }
  }
  do {
    std::lock_guard<std::mutex> lock{_old_mutex};
    if (_old_buffer.size() <= _buffer_size) {
      break;
    }
    cleanup_function(_old_buffer.front());
    _old_buffer.pop_front();
  } while (true);

  do {
    std::unique_lock<std::mutex> swap_lock{_swap_mutex};
    if (!_current->streamer || _current->end || _index == _current->begin) {
      break;
    }
    swap_lock.unlock();
    auto image = _current->streamer->next_frame();
    swap_lock.lock();
    while (_index == _current->begin) {
      swap_lock.unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      swap_lock.lock();
    }
    if (!image) {
      _current->end = true;
      break;
    }
    if (_current->size == _buffer_size) {
      {
        std::lock_guard<std::mutex> lock{_old_mutex};
        _old_buffer.emplace_back(std::move(_current->buffer[_current->begin]));
      }
      _current->buffer[_current->begin] = image;
      _current->begin = (1 + _current->begin) % _buffer_size;
    } else {
      _current->buffer[(_current->begin + _current->size) % _buffer_size] = image;
      ++_current->size;
    }
  } while (true);

  std::unique_lock<std::mutex> swap_lock{_swap_mutex};
  if (!_next->streamer) {
    swap_lock.unlock();
    auto next_streamer = _load_function();
    swap_lock.lock();
    _next->streamer.swap(next_streamer);
  }
  for (auto i = 0; i < 8; ++i) {
    if (!_next->streamer || _next->end || _next->size == _buffer_size) {
      break;
    }
    swap_lock.unlock();
    auto image = _next->streamer->next_frame();
    swap_lock.lock();
    if (!image) {
      _next->end = true;
      break;
    }
    _next->buffer[_next->size] = image;
    ++_next->size;
  }
}
