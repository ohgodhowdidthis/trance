#ifndef TRANCE_PROGRAM_H
#define TRANCE_PROGRAM_H

#include <cstddef>
#include "images.h"

class Director;

// Interface to an object which can render and control the program state.
// These programs are swapped out by the Director every so often for different
// styles.
class Program {
public:

  Program(Director& director);
  virtual ~Program() {}

  virtual void update() = 0;
  virtual void render() const = 0;

protected:

  const Director& director() const;
  Director& director();

private:

  Director& _director;

};

class AccelerateProgram : public Program {
public:

  AccelerateProgram(Director& director, bool start_fast);
  void update() override;
  void render() const override;

private:

  static const unsigned int max_speed = 48;
  static const unsigned int min_speed = 0;

  Image _current;
  std::string _current_text;

  bool _text_on;
  unsigned int _change_timer;
  unsigned int _change_speed;
  unsigned int _change_speed_timer;
  bool _change_faster;

};

class SubTextProgram : public Program {
public:

  SubTextProgram(Director& director);
  void update() override;
  void render() const override;

private:

  static const unsigned int speed = 48;
  static const unsigned int sub_speed = 12;
  static const unsigned int cycles = 32;

  Image _current;
  std::string _current_text;
  bool _text_on;
  unsigned int _change_timer;
  unsigned int _sub_timer;
  unsigned int _cycle;
  unsigned int _sub_speed_multiplier;

};

class SlowFlashProgram : public Program {
public:

  SlowFlashProgram(Director& director);
  void update() override;
  void render() const override;

private:

  static const unsigned int max_speed = 64;
  static const unsigned int min_speed = 4;
  static const unsigned int cycle_length = 16;
  static const unsigned int set_length = 4;

  Image _current;
  std::string _current_text;
  unsigned int _change_timer;

  bool _flash;
  unsigned int _image_count;
  unsigned int _cycle_count;

};

class FlashTextProgram : public Program {
public:

  FlashTextProgram(Director& director);
  void update() override;
  void render() const override;

private:

  static const unsigned int length = 64;
  static const unsigned int font_length = 8;
  static const unsigned int cycles = 8;

  Image _start;
  Image _end;
  std::string _current_text;
  unsigned int _timer;
  unsigned int _cycle;
  unsigned int _font_timer;

};

class ParallelProgram : public Program {
public:

  ParallelProgram(Director& director);
  void update() override;
  void render() const override;

private:

  static const unsigned int length = 32;
  static const unsigned int cycles = 64;

  Image _image;
  Image _alternate;
  bool _switch_alt;
  bool _text_on;
  std::string _current_text;
  unsigned int _timer;
  unsigned int _cycle;

};

class SuperParallelProgram : public Program {
public:

  SuperParallelProgram(Director& director);
  void update() override;
  void render() const override;

private:

  static const std::size_t image_count = 4;
  static const unsigned int font_length = 8;
  static const unsigned int length = 2;
  static const unsigned int cycles = 512;

  std::vector<Image> _images;
  std::size_t _index;
  std::string _current_text;
  unsigned int _timer;
  unsigned int _font_timer;
  unsigned int _cycle;

};

#endif