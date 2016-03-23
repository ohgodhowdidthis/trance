**trance** is a program designed to aid self-hypnosis by displaying images,
animations and text in randomly-generated patterns.

Features
========

* Randomly-generated: no two sessions are the same.
* Graphical user interface for creating sessions.
* Hardware-accelerated rendering of images, animations and fonts.
* Audio support with multiple independent channels.
* Programmable playlist.
* Oculus Rift support.
* Video export.

Sessions
========

The `creator.exe` program can be used to create and edit `.session` files. This
allows full control over the behaviour. Sessions can be launched or exported as
video from within `creator.exe`.

Alternatively, the session player `trance.exe` can be executed from any
directory containing media files. This will generate a default session based
on media files found in the directory structure.

Data model
==========

* A **theme** is a combination of images, animations, fonts and text. Typically
  each theme combines images and animations with relevant text on a particular
  subject.
* A **program** is a selection of themes along with settings to control how they
  are displayed (e.g. playback speed; what configurations the images and text
  should be displayed in).
* A **playlist** determines how the session proceeds. Each item in the playlist
  is associated with a particular program, determines which item in the playlist
  should be played next, and can also trigger audio events (e.g. playing a file,
  fading the volume on a particular channel). In this way, the session can be
  synchronized with audio recordings.
* A **session** is a collection of themes and programs along with a playlist.

All parts of a session can be edited with the provided GUI (`creator.exe`).

Supported file formats
======================

* Images: `.jpg`, `.png`, `.bmp`
* Animations: `.gif`, `.webm`
* Fonts: `.ttf`
* Text: `.txt` (only used for generating default sessions)
* Audio files: `.wav`, `.flac`, `.ogg`, `.aiff`
* Video export: `.webm`, `.h264`, and (frame-by-frame) `.jpg`, `.png`, or `.bmp`
