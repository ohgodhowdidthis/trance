#ifndef TRANCE_SRC_TRANCE_RENDER_VIDEO_EXPORT_H
#define TRANCE_SRC_TRANCE_RENDER_VIDEO_EXPORT_H
#include <trance/render/render.h>
#include <memory>

#pragma warning(push, 0)
#include <GL/glew.h>
#pragma warning(pop)

struct exporter_settings;
class Exporter;

class VideoExportRenderer : public Renderer
{
public:
  VideoExportRenderer(const exporter_settings& settings);
  ~VideoExportRenderer();

  bool vr_enabled() const override;
  uint32_t view_width() const override;
  uint32_t width() const override;
  uint32_t height() const override;
  float eye_spacing_multiplier() const override;

  void init() override;
  bool update() override;
  void render(const std::function<void(State)>& render_fn) override;

private:
  bool init_framebuffer(uint32_t& fbo, uint32_t& fb_tex, uint32_t width, uint32_t height) const;

  const exporter_settings& _settings;

  bool _convert_to_yuv;
  uint32_t _render_fbo;
  uint32_t _render_fb_tex;
  uint32_t _yuv_fbo;
  uint32_t _yuv_fb_tex;

  GLuint _yuv_program;
  GLuint _quad_buffer;

  std::unique_ptr<Exporter> _exporter;
  std::unique_ptr<uint8_t[]> _screen_data;
};

#endif