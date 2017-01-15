#include "visual_cyclers.h"
#include <algorithm>

Cycler::Cycler() : _active{true}
{
}

void Cycler::activate(bool active)
{
  _active = active;
}

bool Cycler::active() const
{
  return _active;
}

bool Cycler::complete() const
{
  return position() == length();
}

uint32_t Cycler::frame() const
{
  auto p = position();
  return p ? p - 1 : length() - 1;
}

float Cycler::progress() const
{
  return static_cast<float>(frame()) / length();
}

ActionCycler::ActionCycler(uint32_t length)
: _position{0}, _length{length}, _action_frame{0}, _action{[] {}}
{
}

ActionCycler::ActionCycler(const std::function<void()>& action)
: _position{0}, _length{1}, _action_frame{0}, _action{action}
{
}

ActionCycler::ActionCycler(uint32_t length, const std::function<void()>& action)
: _position{0}, _length{length}, _action_frame{0}, _action{action}
{
}

ActionCycler::ActionCycler(uint32_t length, uint32_t action_frame,
                           const std::function<void()>& action)
: _position{0}, _length{length}, _action_frame{action_frame}, _action{action}
{
}

uint32_t ActionCycler::length() const
{
  return _length;
}

uint32_t ActionCycler::position() const
{
  return _position;
}

void ActionCycler::reset()
{
  _position = 0;
}

void ActionCycler::advance(bool trigger_actions)
{
  if (complete()) {
    reset();
  }
  if (trigger_actions && _position == _action_frame) {
    _action();
  }
  if (_length) {
    ++_position;
  }
}

OneShotCycler::OneShotCycler(std::vector<Cycler*> subcycles)
{
  for (Cycler* cycle : subcycles) {
    _subcycles.emplace_back();
    _subcycles.back().reset(cycle);
  }
  calculate_active();
}

uint32_t OneShotCycler::length() const
{
  uint32_t result = 0;
  for (const auto& cycle : _subcycles) {
    result = std::max(result, cycle->length());
  }
  return result;
}

uint32_t OneShotCycler::position() const
{
  uint32_t result = 0;
  for (const auto& cycle : _subcycles) {
    result = std::max(result, cycle->position());
  }
  return result;
}

void OneShotCycler::reset()
{
  for (auto& cycler : _subcycles) {
    cycler->reset();
  }
  calculate_active();
}

void OneShotCycler::advance(bool trigger_actions)
{
  bool any_advance = false;
  for (auto& cycle : _subcycles) {
    if (!cycle->complete()) {
      cycle->advance(trigger_actions);
      any_advance = true;
    }
  }
  if (!any_advance) {
    reset();
    for (auto& cycle : _subcycles) {
      if (!cycle->complete()) {
        cycle->advance(trigger_actions);
      }
    }
  }
  calculate_active();
}

void OneShotCycler::activate(bool active)
{
  Cycler::activate(active);
  calculate_active();
}

void OneShotCycler::calculate_active()
{
  auto p = position();
  for (auto& cycle : _subcycles) {
    cycle->activate(active() && p <= cycle->position());
  }
}

ParallelCycler::ParallelCycler(std::vector<Cycler*> subcycles) : _position{0}, _length{0}
{
  for (Cycler* cycle : subcycles) {
    _subcycles.emplace_back();
    _subcycles.back().reset(cycle);
  }
  if (subcycles.empty()) {
    return;
  }
  auto lcm = [](uint32_t a, uint32_t b) {
    auto gcd = [](uint32_t a, uint32_t b) {
      if (a < b) {
        std::swap(a, b);
      }
      while (b) {
        auto r = a % b;
        a = b;
        b = r;
      }
      return a;
    };
    if (a < b) {
      std::swap(a, b);
    }
    if (!b) {
      return a;
    }
    return b * (a / gcd(a, b));
  };

  _length = subcycles.front()->length();
  for (std::size_t i = 1; i < subcycles.size(); ++i) {
    _length = lcm(_length, subcycles[i]->length());
  }
}

uint32_t ParallelCycler::length() const
{
  return _length;
}

uint32_t ParallelCycler::position() const
{
  return _position;
}

void ParallelCycler::reset()
{
  for (auto& cycler : _subcycles) {
    cycler->reset();
  }
  _position = 0;
}

void ParallelCycler::advance(bool trigger_actions)
{
  if (complete()) {
    _position = 0;
  }
  for (auto& cycler : _subcycles) {
    cycler->advance(trigger_actions);
  }
  ++_position;
}

void ParallelCycler::activate(bool active)
{
  Cycler::activate(active);
  for (auto& cycler : _subcycles) {
    cycler->activate(active);
  }
}

SequenceCycler::SequenceCycler(std::vector<Cycler*> subcycles)
{
  for (Cycler* cycle : subcycles) {
    _subcycles.emplace_back();
    _subcycles.back().reset(cycle);
  }
  calculate_active();
}

uint32_t SequenceCycler::index() const
{
  uint32_t total = 0;
  uint32_t index = 0;
  auto f = frame();
  for (const auto& subcycle : _subcycles) {
    total += subcycle->length();
    if (f < total) {
      return index;
    }
    ++index;
  }
  return 0;
}

uint32_t SequenceCycler::length() const
{
  uint32_t result = 0;
  for (const auto& subcycle : _subcycles) {
    result += subcycle->length();
  }
  return result;
}

uint32_t SequenceCycler::position() const
{
  uint32_t result = 0;
  for (const auto& subcycle : _subcycles) {
    result += subcycle->position();
  }
  return result;
}

void SequenceCycler::reset()
{
  for (auto& cycler : _subcycles) {
    cycler->reset();
  }
  calculate_active();
}

void SequenceCycler::advance(bool trigger_actions)
{
  for (auto& cycler : _subcycles) {
    if (!cycler->complete()) {
      cycler->advance(trigger_actions);
      calculate_active();
      return;
    }
  }
  reset();
  for (auto& cycler : _subcycles) {
    if (!cycler->complete()) {
      cycler->advance(trigger_actions);
      calculate_active();
      return;
    }
  }
}

void SequenceCycler::activate(bool active)
{
  Cycler::activate(active);
  calculate_active();
}

void SequenceCycler::calculate_active()
{
  for (auto& cycler : _subcycles) {
    cycler->activate(false);
  }
  if (!active()) {
    return;
  }
  for (std::size_t i = 0; 1 + i < _subcycles.size(); ++i) {
    if (_subcycles[i]->position() && !_subcycles[1 + i]->position()) {
      _subcycles[i]->activate(true);
      return;
    }
  }
  if (!_subcycles.empty()) {
    _subcycles.back()->activate(true);
  }
}

RepeatCycler::RepeatCycler(uint32_t repetitions, Cycler* subcycle)
: _subcycle{subcycle}, _repetitions{repetitions}, _index{0}
{
}

uint32_t RepeatCycler::index() const
{
  return frame() / _subcycle->length();
}

uint32_t RepeatCycler::length() const
{
  return _repetitions * _subcycle->length();
}

uint32_t RepeatCycler::position() const
{
  return _subcycle->position() + _index * _subcycle->length();
}

void RepeatCycler::reset()
{
  _index = 0;
  _subcycle->reset();
}

void RepeatCycler::advance(bool trigger_actions)
{
  if (_subcycle->complete()) {
    _index = (1 + _index) % _repetitions;
  }
  _subcycle->advance(trigger_actions);
}

void RepeatCycler::activate(bool active)
{
  Cycler::activate(active);
  _subcycle->activate(active);
}

OffsetCycler::OffsetCycler(uint32_t offset, Cycler* subcycle)
: _subcycle{subcycle}, _offset{offset}, _position{0}
{
  advance_to_offset();
}

uint32_t OffsetCycler::length() const
{
  return _subcycle->length();
}

uint32_t OffsetCycler::position() const
{
  return _position;
}

void OffsetCycler::reset()
{
  _position = 0;
  _subcycle->reset();
  advance_to_offset();
}

void OffsetCycler::advance(bool trigger_actions)
{
  ++_position;
  _subcycle->advance(trigger_actions);
}

void OffsetCycler::activate(bool active)
{
  Cycler::activate(active);
  _subcycle->activate(active);
}

void OffsetCycler::advance_to_offset()
{
  auto frames = length() - _offset % length();
  for (uint32_t i = 0; i < frames; ++i) {
    _subcycle->advance(false);
  }
}