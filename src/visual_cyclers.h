#ifndef TRANCE_VISUAL_CYCLERS_H
#define TRANCE_VISUAL_CYCLERS_H
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// Interface for constructing visualiser patterns.
class Cycler
{
public:
  Cycler();
  virtual ~Cycler() = default;

  // The length of the cycle; i.e. how many times advance() must be called until complete() returns
  // true.
  virtual uint32_t length() const = 0;
  // The current cycle head position.
  virtual uint32_t position() const = 0;
  // Reset the cycle head to the begininning of the sequence.
  virtual void reset() = 0;
  // Invoke actions under the cycle head and advance it. If the cycle head is currently at the end
  // of the sequence already, the position is reset first.
  virtual void advance(bool trigger_actions = true) = 0;

  // Sets a flag indicating whether the cycle is currently active.
  virtual void activate(bool active);

  // Whether the cycle is currently active.
  bool active() const;
  // Whether the cycle head is at the end of the sequence.
  bool complete() const;
  // The current frame of the cycler. This is position() - 1 mod length().
  uint32_t frame() const;
  // Current frame, as a float, scaled between 0 and 1.
  float progress() const;

private:
  bool _active;
};

// Performs an action periodically.
class ActionCycler : public Cycler
{
public:
  // No-op action with the given length.
  ActionCycler(uint32_t length);
  // Performs the action every frame.
  ActionCycler(const std::function<void()>& action);
  // Performs the action on the first frame of every N.
  ActionCycler(uint32_t length, const std::function<void()>& action);
  // Performs the action on the Kth frame of every N.
  ActionCycler(uint32_t length, uint32_t action_frame, const std::function<void()>& action);

  uint32_t length() const override;
  uint32_t position() const override;
  void reset() override;
  void advance(bool trigger_actions = true) override;

private:
  uint32_t _position;
  uint32_t _length;
  uint32_t _action_frame;
  std::function<void()> _action;
};

// Performs multiple actions in parallel. The cycle is complete when all subcycles are completed
// (so the length is the maximum of the subcycle lengths).
class OneShotCycler : public Cycler
{
public:
  OneShotCycler(std::vector<Cycler*> subcycles);

  uint32_t length() const override;
  uint32_t position() const override;
  void reset() override;
  void advance(bool trigger_actions = true) override;
  void activate(bool active) override;

private:
  void calculate_active();
  std::vector<std::unique_ptr<Cycler>> _subcycles;
};

// Performs multiple actions repeatedly in parallel. The cycle is complete when all subcycles
// are completed at the same time (so the length is the least common multiple of the subcycle
// lengths).
class ParallelCycler : public Cycler
{
public:
  ParallelCycler(std::vector<Cycler*> subcycles);

  uint32_t length() const override;
  uint32_t position() const override;
  void reset() override;
  void advance(bool trigger_actions = true) override;
  void activate(bool active) override;

private:
  std::vector<std::unique_ptr<Cycler>> _subcycles;
  uint32_t _position;
  uint32_t _length;
};

// Performs multiple actions in sequence. The cycle is complete when all subcycles are completed
// (so the length is the sum of the subcycle lengths).
class SequenceCycler : public Cycler
{
public:
  SequenceCycler(std::vector<Cycler*> subcycles);
  uint32_t index() const;

  uint32_t length() const override;
  uint32_t position() const override;
  void reset() override;
  void advance(bool trigger_actions = true) override;
  void activate(bool active) override;

private:
  void calculate_active();
  std::vector<std::unique_ptr<Cycler>> _subcycles;
};

// Performs an action repeatedly. The length is the length of the subcycle multiplied by the number
// of repetitions.
class RepeatCycler : public Cycler
{
public:
  RepeatCycler(uint32_t repetitions, Cycler* subcycle);
  uint32_t index() const;

  uint32_t length() const override;
  uint32_t position() const override;
  void reset() override;
  void advance(bool trigger_actions = true) override;
  void activate(bool active) override;

private:
  std::unique_ptr<Cycler> _subcycle;
  uint32_t _repetitions;
  uint32_t _index;
};

// Offsets a subcycle within its period. The length is the same as that of the subcycle.
class OffsetCycler : public Cycler
{
public:
  OffsetCycler(uint32_t offset, Cycler* subcycle);

  uint32_t length() const override;
  uint32_t position() const override;
  void reset() override;
  void advance(bool trigger_actions = true) override;
  void activate(bool active) override;

private:
  void advance_to_offset();
  std::unique_ptr<Cycler> _subcycle;
  uint32_t _offset;
  uint32_t _position;
};

#endif