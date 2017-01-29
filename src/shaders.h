#ifndef TRANCE_SHADERS_H
#define TRANCE_SHADERS_H

namespace {

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
// Colour / alpha value.
uniform vec4 colour;

// Virtual position of vertex (in [-1 - |eye|, 1 + |eye|] X [-1, 1] X [0, 1] X [0, 1]).
// X- and Y-coordinates need to be scaled by (texture_size / window_size) to maintain correct
// aspect ratio. Z-coordinate is the zoom amount; W-coordinate is the zoom origin (same as zoom
// value will perfectly correct and show image at original size but _closer_ in VR).
attribute vec4 virtual_position;
// Texture coordinate for this vertex.
attribute vec2 texture_coord;

// Output texture coordinate.
varying vec2 out_texture_coord;
// Output colour / alpha value.
varying vec4 out_colour;

// Applies perspective projection onto unit square.
mat4 m_perspective = mat4(
    near_plane, 0., 0., 0.,
    0., near_plane, 0., 0.,
    0., 0., (near_plane + far_plane) / (near_plane - far_plane), -1.,
    0., 0., 2. * (near_plane * far_plane) / (near_plane - far_plane), 0.);

// Projects onto far plane with zoom coordinate.
mat4 m_virtual = mat4(
    (1 - virtual_position.w) * far_plane / near_plane + virtual_position.w, 0., 0., 0.,
    0., (1 - virtual_position.w) * far_plane / near_plane + virtual_position.w, 0., 0.,
    0., 0., far_plane - near_plane, 0.,
    -eye_offset, 0., -far_plane, 1.);

void main()
{
  gl_Position = m_perspective * m_virtual * vec4(virtual_position.xyz, 1.);
  out_texture_coord = texture_coord;
  out_colour = colour;
}
)";

const std::string new_fragment = R"(
// Active texture for this draw.
uniform sampler2D texture;
// Input texture coordinate.
varying vec2 out_texture_coord;
// Input alpha value.
varying vec4 out_colour;

void main()
{
  gl_FragColor = out_colour * texture2D(texture, out_texture_coord);
}
)";

const std::string spiral_vertex = R"(
// Position in [-1, 1] X [-1, 1].
attribute vec2 device_position;

// Output texture coordinate.
varying vec2 out_texture_coord;

void main() {
  gl_Position = vec4(device_position.xy, 0.0, 1.0);
  out_texture_coord = device_position;
}
)";

const std::string spiral_fragment = R"(
// See main shader for details.
uniform float near_plane;
uniform float far_plane;
uniform float eye_offset;

// Width divided by height. We fix y from [-1, 1] and let x span [-aspect_ratio, aspect_ratio].
uniform float aspect_ratio;
// A divisor of 360 (determines the number of spiral arms).
uniform float width;
// Switch between spiral types.
uniform float spiral_type;
// Makes the spiral spin.
uniform float time;

uniform vec4 acolour;
uniform vec4 bcolour;

// Input coordinate.
varying vec2 out_texture_coord;

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

// Raycasts from eye through near plane onto a cone defined by the near and far planes.
// See https://www.geometrictools.com/Documentation/IntersectionLineCone.pdf for details.
vec2 cone_intersection(vec2 aspect_position)
{
  // Cone origin.
  vec3 cone_origin = vec3(0., 0., far_plane);
  // Cone axis unit vector.
  vec3 cone_axis = vec3(0., 0., -1.);
  // Cone angle, chosen such that the cone intersects the corners of the near plane.
  float max_width = aspect_ratio + abs(eye_offset);
  float cone_angle = atan(
      (sqrt(max_width * max_width + 1.)) / (far_plane - near_plane));

  // Eye position.
  vec3 ray_origin = vec3(eye_offset, 0., 0.);
  // Unit vector from eye to near plane.
  vec3 ray_vector = normalize(vec3(aspect_position, near_plane));

  // Quadratic equation for line-cone intersection.
  vec3 m = cone_axis * cone_axis - cos(cone_angle) * cos(cone_angle);
  vec3 delta = ray_origin - cone_origin;

  float a = dot(m, ray_vector * ray_vector);
  float b = 2 * dot(m, ray_vector * delta);
  float c = dot(m, delta * delta);

  // Solution for equation.
  float d = sqrt(b * b - 4 * a * c);
  float t0 = (-b - d) / (2 * a);
  float t1 = (-b + d) / (2 * a);
  float d0 = dot(cone_axis, ray_origin + t0 * ray_vector - cone_origin);
  float d1 = dot(cone_axis, ray_origin + t1 * ray_vector - cone_origin);

  float t = 0.;
  if (a == 0.) {
    // Ray parallel to cone; only one solution.
    t = -c / b;
  } else if (d == 0.) {
    // Only one intersection point.
    t = -b / (2 * a);
  } if (t0 < 0. || d0 < 0.) {
    // Intersection behind near plane or far plane (respectively).
    t = t1;
  } else {
    t = t0;
  }

  // This is the intersection point with the cone.
  vec3 cone_intersection = ray_origin + t * ray_vector;
  // Now we project back through the origin to correct the distortion (and cancel out if the
  // eye is at the origin).
  return near_plane * cone_intersection.xy / cone_intersection.z;
}

void main(void)
{
  // Near-plane position with correct aspect ratio.
  vec2 position = cone_intersection(out_texture_coord * vec2(aspect_ratio, 1.));
  float angle = 0.;
  float radius = length(position);

  if (position.x != 0. && position.y != 0.) {
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
  float amod = mod(angle - width * time - 2. * width * factor, width);
  float v = amod < width / 2. ? 0. : 1.;
  float t = .2 + 2. * (1.0 - pow(min(1., radius), .4));
  if (amod > width / 2.0 - t && amod < width / 2. + t) {
    v = (amod - width / 2. + t) / (2. * t);
  }
  if (amod < t) {
    v = 1. - (amod + t) / (2. * t);
  }
  if (amod > width - t) {
    v = 1. - (amod - width + t) / (2. * t);
  }
  gl_FragColor = mix(
      (acolour + bcolour) / 2.,
      mix(acolour, bcolour, v),
      clamp(radius * 1024. / (360. / width), 0., 1.));
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