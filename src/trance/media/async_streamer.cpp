#include "async_streamer.h"
#include <common/util.h>

AsyncStreamer::AsyncStreamer(const std::function<std::unique_ptr<Streamer>()>& load_function,
                             size_t buffer_size)
: _load_function{load_function}, _buffer_size{buffer_size}
{
}

bool AsyncStreamer::is_loaded() const
{
  std::lock_guard<std::mutex> lock{_current_mutex};
  if (_current.streamer) {
    return _current.end || _current.buffer.size() >= _buffer_size;
  }
  std::lock_guard<std::mutex> next_lock{_next_mutex};
  return _current.buffer.empty() && _next.buffer.empty();
}

void AsyncStreamer::load()
{
  std::lock_guard<std::mutex> lock{_current_mutex};
  if (!_current.streamer) {
    _current.streamer = _load_function();
    _current.buffer.clear();
    _current.end = false;

    _update_counter = 0.f;
    _reached_end = false;
    _backwards = false;
    _index = 0;
  }
}

void AsyncStreamer::clear()
{
  std::lock_guard<std::mutex> lock{_current_mutex};
  std::lock_guard<std::mutex> next_lock{_next_mutex};
  _current.streamer.reset();
  _next.streamer.reset();
  _current.end = false;
  _next.end = false;

  _update_counter = 0.f;
  _reached_end = false;
  _backwards = false;
  _index = 0;
}

void AsyncStreamer::maybe_upload_next(const std::function<void(const Image&)>& function)
{
  {
    std::lock_guard<std::mutex> lock{_current_mutex};
    if (!_current.buffer.empty()) {
      function(_current.buffer[random(_current.buffer.size())]);
    }
  }
  {
    std::lock_guard<std::mutex> lock{_next_mutex};
    if (!_next.buffer.empty()) {
      function(_next.buffer[random(_next.buffer.size())]);
    }
  }
}

Image AsyncStreamer::get_frame(const std::function<void(const Image&)>& function) const
{
  std::lock_guard<std::mutex> lock{_current_mutex};
  if (_index >= _current.buffer.size()) {
    return {};
  }
  function(_current.buffer[_index]);
  Image image = _current.buffer[_index];
  return image;
}

void AsyncStreamer::advance_frame(uint32_t global_fps, bool maybe_switch)
{
  if (!_current.streamer) {
    return;
  }
  _update_counter += (120.f / global_fps) / 8.f;
  while (_update_counter > 1.f) {
    _update_counter -= 1.f;

    std::lock_guard<std::mutex> lock{_current_mutex};
    {
      std::lock_guard<std::mutex> next_lock{_next_mutex};
      if (!_old_streamer && _old_buffer.empty() && _next.streamer &&
          (!_current.streamer->success() || _current.buffer.empty() ||
           (maybe_switch && _reached_end && (_next.end || _next.buffer.size() >= _buffer_size)))) {
        _current.streamer.swap(_next.streamer);
        _current.buffer.swap(_next.buffer);
        _current.end = _next.end;

        {
          std::lock_guard<std::mutex> lock{_old_mutex};
          _next.streamer.swap(_old_streamer);
          _next.buffer.swap(_old_buffer);
        }
        _next.end = false;

        _reached_end = false;
        _backwards = false;
        _index = 0;
        return;
      }
    }
    if (_backwards) {
      if (_index > 0) {
        --_index;
      } else {
        _backwards = false;
        if (_index < _current.buffer.size() - 1) {
          ++_index;
        }
      }
    } else {
      if (_index < _current.buffer.size() - 1) {
        if (++_index == _current.buffer.size() - 1 && _current.end) {
          _reached_end = true;
        }
      } else {
        _backwards = true;
        if (_current.end) {
          _reached_end = true;
        }
        if (_index > 0) {
          --_index;
        }
      }
    }
  }
}

void AsyncStreamer::async_update(const std::function<void(const Image&)>& cleanup_function)
{
  {
    std::lock_guard<std::mutex> lock{_old_mutex};
    if (_old_streamer) {
      _old_streamer.reset();
    }
    while (_old_buffer.size() > _buffer_size) {
      cleanup_function(_old_buffer.front());
      _old_buffer.pop_front();
    }
    if (!_old_buffer.empty()) {
      cleanup_function(_old_buffer.front());
      _old_buffer.pop_front();
    }
  }

  do {
    std::lock_guard<std::mutex> lock{_current_mutex};
    if (!_current.streamer || _current.end || !_index) {
      break;
    }
    auto image = _current.streamer->next_frame();
    if (image) {
      _current.buffer.push_back(image);
      _current.buffer.pop_front();
      --_index;
    } else {
      _current.end = true;
    }
  } while (true);

  {
    std::unique_lock<std::mutex> lock{_current_mutex};
    std::unique_lock<std::mutex> next_lock{_next_mutex};
    if (_current.streamer && !_next.streamer) {
      next_lock.unlock();
      lock.unlock();
      auto next_streamer = _load_function();
      lock.lock();
      next_lock.lock();
      if (_current.streamer && !_next.streamer) {
        _next.streamer = std::move(next_streamer);
        _next.buffer.clear();
        _next.end = false;
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock{_current_mutex};
    if (_current.streamer && !_current.end && _current.buffer.size() < _buffer_size) {
      auto image = _current.streamer->next_frame();
      if (image) {
        _current.buffer.push_back(image);
        if (_index > 0) {
          _current.buffer.pop_front();
          --_index;
        }
      } else {
        _current.end = true;
      }
    }
    if (!_current.streamer && !_current.buffer.empty()) {
      cleanup_function(_current.buffer.front());
      _current.buffer.pop_front();
    }
  }

  {
    std::lock_guard<std::mutex> lock{_next_mutex};
    if (_next.streamer && !_next.end && _next.buffer.size() < _buffer_size) {
      auto image = _next.streamer->next_frame();
      if (image) {
        _next.buffer.push_back(image);
      } else {
        _next.end = true;
      }
    }
    if (!_next.streamer && !_next.buffer.empty()) {
      cleanup_function(_next.buffer.front());
      _next.buffer.pop_front();
    }
  }
}
