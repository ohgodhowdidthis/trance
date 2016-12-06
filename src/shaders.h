#ifndef TRANCE_SHADERS_H
#define TRANCE_SHADERS_H

namespace {

const std::string text_vertex = R"(
uniform vec4 colour;
attribute vec2 position;
attribute vec2 texcoord;
varying vec2 vtexcoord;
varying vec4 vcolour;

void main()
{
  gl_Position = vec4(2.0 * position.x, -2.0 * position.y, 0.0, 1.0);
  vtexcoord = texcoord;
  vcolour = colour;
}
)";
const std::string text_fragment = R"(
uniform sampler2D texture;
varying vec2 vtexcoord;
varying vec4 vcolour;

void main()
{
  gl_FragColor = vcolour * texture2D(texture, vtexcoord);
}
)";

const std::string new_vertex = R"(
// Distance to near plane. Controls the field of view. Since the near plane extends across
// (-1, -1) to (+1, +1) in the XY-plane, we have FoV = 2 * arctan(1 / near_plane). Should
// probably be constant. Can be 1 without loss of generality.
uniform float near_plane;
// Distance to far plane. Controls the effect of zoom. Holding near_plane fixed, increasing
// far_plane makes the zoom effect faster and more pronounced. The zoom speed is proportional
// to near_plane - far_plane.
uniform float far_plane;
// Unitless eye offset relative to near plane.
uniform float eye_offset;
// Alpha value.
uniform float alpha;

// Virtual position of vertex (in [-1 - |eye|, 1 + |eye|] X [-1, 1] X [0, 1]).
// Z-coordinate is the zoom amount; X- and Y-coordinates need to be scaled by
// (texture_size / window_size) to maintain correct aspect ratio!
attribute vec3 virtual_position;
// Texture coordinate for this vertex.
attribute vec2 texture_coord;

// Output texture coordinate.
varying vec2 out_texture_coord;
// Output alpha value.
varying float out_alpha;

// Applies perspective projection onto unit square.
mat4 m_perspective = mat4(
    near_plane, 0., 0., 0.,
    0., near_plane, 0., 0.,
    0., 0., (near_plane + far_plane) / (near_plane - far_plane), -1.,
    0., 0., 2. * (near_plane * far_plane) / (near_plane - far_plane), 0.);

// Applies the zoom coordinate.
mat4 m_virtual = mat4(
    far_plane / near_plane, 0., 0., -(far_plane / near_plane) * eye_offset,
    0., far_plane / near_plane, 0., 0.,
    0., 0., far_plane - near_plane, 0.,
    0., 0., -far_plane, 1.);

// Avoids the very edge of images.
const float texture_epsilon = 1. / 256;

void main()
{
  gl_Position = m_perspective * m_virtual * vec4(virtual_position, 1.);
  out_texture_coord =
      texture_coord * (1. - texture_epsilon) + texture_epsilon / 2.;
  out_alpha = alpha;
}
)";

const std::string new_fragment = R"(
// Active texture for this draw.
uniform sampler2D texture;
// Input texture coordinate.
varying vec2 out_texture_coord;
// Input alpha value.
varying float out_alpha;

void main()
{
  gl_FragColor = vec4(texture2D(texture, out_texture_coord).rgb, out_alpha);
}
)";

const std::string image_vertex = R"(
uniform vec2 min_coord;
uniform vec2 max_coord;
uniform vec2 flip;
uniform float alpha;
uniform float zoom;
attribute vec2 position;
attribute vec2 texcoord;
varying vec2 vtexcoord;
varying float valpha;

void main()
{
  vec2 pos = position / 2.0 + 0.5;
  pos = pos * (max_coord - min_coord) + min_coord;
  pos = (pos - 0.5) * 2.0;
  gl_Position = vec4(pos, 0.0, 1.0);
  float z = min(0.5, 0.1 * zoom + 0.005);
  vtexcoord = vec2(texcoord.x > 0.5 ? 1.0 - z : z,
                   texcoord.y > 0.5 ? 1.0 - z : z);
  vtexcoord = vec2(flip.x != 0.0 ? 1.0 - vtexcoord.x : vtexcoord.x,
                   flip.y != 0.0 ? 1.0 - vtexcoord.y : vtexcoord.y);
  valpha = alpha;
}
)";
const std::string image_fragment = R"(
uniform sampler2D texture;
varying vec2 vtexcoord;
varying float valpha;

void main()
{
  gl_FragColor = vec4(texture2D(texture, vtexcoord).rgb, valpha);
}
)";

const std::string spiral_vertex = R"(
attribute vec2 position;

void main() {
  gl_Position = vec4(position.xy, 0.0, 1.0);
}
)";
const std::string spiral_fragment = R"(
uniform float time;
uniform vec2 resolution;
uniform float offset;
uniform vec4 acolour;
uniform vec4 bcolour;

// A divisor of 360 (determines the number of spiral arms).
uniform float width;
uniform float spiral_type;

float spiral1(float r)
{
  return log(r);
}

float spiral2(float r)
{
  return r * r;
}

float spiral3(float r)
{
  return r;
}

float spiral4(float r)
{
  return sqrt(r);
}

float spiral5(float r)
{
  return -abs(r - 1);
}

float spiral6(float r)
{
  float r1 = r * 1.2;
  float r2 = (1.5 - 0.5 * r) * 1.2;
  return r < 1 ? pow(r1, 6.0) : -pow(r2, 6.0);
}

float spiral7(float r)
{
  float m = mod(r, 0.2);
  m = m < 0.1 ? m : 0.2 - m;
  return r + m * 3.0;
}

void main(void)
{
  vec2 aspect = vec2(resolution.x / resolution.y, 1.0);
  vec2 op = gl_FragCoord.xy - vec2(offset, 0.0);
  vec2 position = -aspect.xy + 2.0 * op / resolution.xy * aspect.xy;
  float angle = 0.0;
  float radius = length(position);
  if (position.x != 0.0 && position.y != 0.0) {
    angle = degrees(atan(position.y, position.x));
  }

  float factor =
      spiral_type == 1 ? spiral1(radius) :
      spiral_type == 2 ? spiral2(radius) :
      spiral_type == 3 ? spiral3(radius) :
      spiral_type == 4 ? spiral4(radius) :
      spiral_type == 5 ? spiral5(radius) :
      spiral_type == 6 ? spiral6(radius) :
                         spiral7(radius);
  float amod = mod(angle - width * time - 2.0 * width * factor, width);
  float v = amod < width / 2 ? 0.0 : 1.0;
  float t = 0.2 + 2.0 * (1.0 - pow(min(1.0, radius), 0.4));
  if (amod > width / 2.0 - t && amod < width / 2.0 + t) {
    v = (amod - width / 2.0 + t) / (2.0 * t);
  }
  if (amod < t) {
    v = 1.0 - (amod + t) / (2.0 * t);
  }
  if (amod > width - t) {
    v = 1.0 - (amod - width + t) / (2.0 * t);
  }
  gl_FragColor = mix(acolour, bcolour, v);
}
)";

const std::string yuv_vertex = R"(
attribute vec2 position;

void main() {
  gl_Position = vec4(position.xy, 0.0, 1.0);
}
)";
const std::string yuv_fragment = R"(
uniform sampler2D source;
uniform vec2 resolution;
uniform float yuv_mix;

const mat3 map = mat3(
    0.257, -0.148, 0.439,
    0.504, -0.291, -0.368,
    0.098, 0.439, -0.071);
const vec3 offset = vec3(16.0, 128.0, 128.0) / 255.0;

void main(void)
{
  vec2 coord = gl_FragCoord.xy / resolution;
  coord.y *= -1;
  vec3 rgb = texture2D(source, coord).rgb;
  vec3 yuv = clamp(offset + map * rgb, 0.0, 1.0);
  gl_FragColor = vec4(mix(rgb, yuv, yuv_mix), 1.0);
}
)";

}

#endif