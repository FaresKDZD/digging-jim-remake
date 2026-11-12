uniform sampler2D texture;
// Blend colour for Photoshop-style Color mode (hue+sat from this, lum from scene).
uniform vec3 blendColor;
// Same cave tint controls as tint.frag (hue, sat, lum in [0,1]).
uniform vec3 tint;
// Animation clock for the wave wobble (seconds-ish).
uniform float time;

vec3 rgb2hsl(vec3 c) {
    float maxC = max(c.r, max(c.g, c.b));
    float minC = min(c.r, min(c.g, c.b));
    float delta = maxC - minC;

    float h = 0.0;
    if (delta > 0.0) {
        if (maxC == c.r) {
            h = mod((c.g - c.b) / delta, 6.0);
        } else if (maxC == c.g) {
            h = ((c.b - c.r) / delta) + 2.0;
        } else {
            h = ((c.r - c.g) / delta) + 4.0;
        }
        h /= 6.0;
        if (h < 0.0) h += 1.0;
    }

    float l = (maxC + minC) * 0.5;

    float s = 0.0;
    if (delta > 0.0) {
        s = delta / (1.0 - abs(2.0 * l - 1.0));
    }

    return vec3(h, s, l);
}

vec3 hsl2rgb(vec3 c) {
    float h = c.x;
    float s = c.y;
    float l = c.z;

    float C = (1.0 - abs(2.0 * l - 1.0)) * s;
    float X = C * (1.0 - abs(mod(h * 6.0, 2.0) - 1.0));
    float m = l - C / 2.0;

    vec3 rgb = vec3(0.0);
    if (h < 1.0 / 6.0) rgb = vec3(C, X, 0.0);
    else if (h < 2.0 / 6.0) rgb = vec3(X, C, 0.0);
    else if (h < 3.0 / 6.0) rgb = vec3(0.0, C, X);
    else if (h < 4.0 / 6.0) rgb = vec3(0.0, X, C);
    else if (h < 5.0 / 6.0) rgb = vec3(X, 0.0, C);
    else rgb = vec3(C, 0.0, X);

    return rgb + vec3(m);
}

vec3 applyCaveFilter(vec3 rgb) {
    vec3 hsl = rgb2hsl(rgb);

    float hueLinear = mod(2.0 / 3.0 - hsl.x + 1.0, 1.0) * tint.x * 2.0;
    float lumLinear = hsl.z + (1.0 - hsl.z + hsl.y) * (tint.z - 0.5) / 3.0;
    float lumLimit = hsl.z + (1.0 - hsl.z) * (1.0 - hsl.y) * tint.z * 1.0 / 3.0;

    hsl.x = mod(2.0 / 3.0 - min(hueLinear, 1.0) + 1.0, 1.0);
    hsl.y = min(hsl.y * tint.y * 2.0, 1.0);
    hsl.z = min(max(lumLinear, 0.0), lumLimit);

    return hsl2rgb(hsl);
}

float lum(vec3 c) {
    return dot(c, vec3(0.3, 0.59, 0.11));
}

vec3 clipColor(vec3 c) {
    float l = lum(c);
    float n = min(min(c.r, c.g), c.b);
    float x = max(max(c.r, c.g), c.b);
    if (n < 0.0) {
        c = l + (((c - l) * l) / (l - n + 1e-5));
    }
    if (x > 1.0) {
        c = l + (((c - l) * (1.0 - l)) / (x - l + 1e-5));
    }
    return clamp(c, 0.0, 1.0);
}

vec3 setLum(vec3 c, float l) {
    return clipColor(c + (l - lum(c)));
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    // Gentle liquid wave distortion (~2–3 px at 640×480).
    float ax = 3.0 / 640.0;
    float ay = 2.5 / 480.0;
    uv.x += sin(uv.y * 18.0 + time) * ax
          + sin(uv.y * 7.0 + time * 1.37) * ax * 0.45;
    uv.y += cos(uv.x * 14.0 + time * 1.21) * ay
          + cos(uv.x * 5.5 + time * 0.83) * ay * 0.4;
    uv = clamp(uv, 0.0, 1.0);

    vec4 base = texture2D(texture, uv);
    // Level tint shifts the Color-layer colour the same way it shifts cave tiles.
    vec3 colored = applyCaveFilter(blendColor);
    // Color blend: keep scene luminosity, take hue/sat from the tinted blend colour.
    vec3 outc = setLum(colored, lum(base.rgb));
    gl_FragColor = vec4(outc, base.a);
}
