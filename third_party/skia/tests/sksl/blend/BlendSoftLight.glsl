#version 400
out vec4 sk_FragColor;
uniform vec4 src;
uniform vec4 dst;
float _soft_light_component_hh2h2(vec2 s, vec2 d) {
    if (2.0 * s.x <= s.y) {
        return (((d.x * d.x) * (s.y - 2.0 * s.x)) / d.y + (1.0 - d.y) * s.x) + d.x * ((-s.y + 2.0 * s.x) + 1.0);
    } else if (4.0 * d.x <= d.y) {
        float DSqd = d.x * d.x;
        float DCub = DSqd * d.x;
        float DaSqd = d.y * d.y;
        float DaCub = DaSqd * d.y;
        return (((DaSqd * (s.x - d.x * ((3.0 * s.y - 6.0 * s.x) - 1.0)) + ((12.0 * d.y) * DSqd) * (s.y - 2.0 * s.x)) - (16.0 * DCub) * (s.y - 2.0 * s.x)) - DaCub * s.x) / DaSqd;
    } else {
        return ((d.x * ((s.y - 2.0 * s.x) + 1.0) + s.x) - sqrt(d.y * d.x) * (s.y - 2.0 * s.x)) - d.y * s.x;
    }
}
void main() {
    sk_FragColor = dst.w == 0.0 ? src : vec4(_soft_light_component_hh2h2(src.xw, dst.xw), _soft_light_component_hh2h2(src.yw, dst.yw), _soft_light_component_hh2h2(src.zw, dst.zw), src.w + (1.0 - src.w) * dst.w);
}
