uniform sampler2D texture;
// R = body water (UV distort), G = all water incl. surface (Color caustic wave),
// B = lava body (UV distort only, shorter wavelengths).
uniform sampler2D waterMask;
uniform float time;
// Water Color-layer tint (hue/sat); luminosity comes from the scene + caustics.
uniform vec3 blendColor;
// Same cave tint controls as tint.frag (hue, sat, lum in [0,1]).
uniform vec3 tint;
// Cave camera view in world px ? caustics are evaluated in world space (stationary).
uniform vec2 viewCenter;
uniform vec2 viewSize;
// Output RT size (e.g. 640×480); used with gl_FragCoord for stable world mapping.
uniform vec2 rtSize;

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

// Cheap value-noise helpers for irregular caustic domains.
float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += a * valueNoise(p);
        p = p * 2.03 + vec2(17.1, 9.3);
        a *= 0.5;
    }
    return v;
}

// Original water caustic field (unchanged from pre-lava).
float causticField(vec2 world) {
    // Longer wavelengths: ~2?4 tiles across a crest (was ~finer than 1 tile).
    vec2 p = world / 64.0;
    float t = time * 0.85;

    // Slow noise warp so ridges don't form a regular lattice.
    vec2 warp = vec2(
        fbm(p * 0.55 + vec2(t * 0.07, 3.1)),
        fbm(p * 0.55 + vec2(9.2, -t * 0.06))
    );
    vec2 q = p + (warp - 0.5) * 1.35;

    // Uneven directions / frequencies (not harmonic multiples).
    float w1 = sin(q.y * 6.2 + t * 0.9 + 1.1 * sin(q.x * 3.4 + t * 0.55 + 0.7));
    float w2 = sin(q.x * 5.1 + t * 1.05 + 0.9 * sin(q.y * 4.3 - t * 0.7 + 1.9));
    float w3 = sin(dot(q, vec2(4.7, 3.1)) + t * 0.72 + 0.8 * sin(dot(q, vec2(-2.9, 5.4)) + t * 0.4));
    float w4 = sin(dot(q, vec2(-3.6, 4.9)) + t * 1.18 + 0.65 * sin(q.x * 2.2 + q.y * 1.7 + t * 0.33));
    // Slightly offset third axis so diagonals don't line up.
    float w5 = sin(dot(q + vec2(fbm(q + 2.0), fbm(q - 1.4)) * 0.4, vec2(2.8, -5.6)) + t * 0.95);

    float field = w1 * 0.28 + w2 * 0.24 + w3 * 0.22 + w4 * 0.16 + w5 * 0.14;

    // Rare, larger-scale sparkle bursts (still sparse).
    float env = pow(max(0.0, sin(t * 0.17 + fbm(p * 0.3) * 6.0)), 14.0);
    float band = smoothstep(0.45, 0.8, fbm(q * 0.8 + t * 0.05));
    float spark = sin(q.y * 18.0 + t * 2.4 + 1.2 * sin(q.x * 14.0 + t * 1.8))
                * sin(q.x * 15.5 + t * 2.1 + 0.9 * sin(q.y * 12.0 - t * 1.6));
    field += spark * env * band * 0.22;

    return field;
}

// Separate lava distortion field: shorter wavelengths, different axes / tempo
// (not a scaled copy of the water caustic).
float lavaDistortField(vec2 world) {
    vec2 p = world / 28.0;
    float t = time * 1.15;

    vec2 warp = vec2(
        fbm(p * 0.9 + vec2(-t * 0.11, 5.7)),
        fbm(p * 0.9 + vec2(4.3, t * 0.13))
    );
    vec2 q = p + (warp - 0.5) * 0.85;

    float w1 = sin(q.x * 11.4 + t * 1.4 + 0.7 * sin(q.y * 7.8 - t * 0.95));
    float w2 = sin(q.y * 9.6 - t * 1.25 + 0.85 * sin(q.x * 6.5 + t * 0.8));
    float w3 = sin(dot(q, vec2(8.2, -6.1)) + t * 1.05
                 + 0.6 * sin(dot(q, vec2(5.3, 7.4)) - t * 0.55));
    float w4 = sin(dot(q, vec2(-7.1, 5.8)) + t * 1.55
                 + 0.5 * sin(q.y * 4.9 + q.x * 3.2 + t * 0.42));

    return w1 * 0.34 + w2 * 0.30 + w3 * 0.22 + w4 * 0.14;
}

float causticLight(float field) {
    float n = clamp(field * 0.5 + 0.5, 0.0, 1.0);
    return pow(n, 2.6);
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec4 masks = texture2D(waterMask, uv);
    float bodyMask = masks.r;
    float colorMask = masks.g;
    float lavaMask = masks.b;

    // World from fragment position (FBO origin bottom-left), not from texcoords ?
    // RT texture V-flip must not affect the caustic domain, or the pattern crawls
    // with the camera (mirrored about the view centre).
    vec2 ndc = gl_FragCoord.xy / max(rtSize, vec2(1.0));
    vec2 viewUv = vec2(ndc.x, 1.0 - ndc.y);
    vec2 world = viewCenter + (viewUv - vec2(0.5)) * viewSize;

    float field = causticField(world);
    float light = causticLight(field);

    float ax = 3.2 / 640.0;
    float ay = 1.1 / 480.0;

    vec2 sampleUv = uv;
    if (lavaMask > 0.01) {
        float lavaField = lavaDistortField(world);
        sampleUv.x += lavaField * ax * 1.1;
        sampleUv.y += (lavaField * 0.4 + sin((world.x / 28.0) * 8.5 + time * 1.15) * 0.3) * ay;
        sampleUv = clamp(sampleUv, 0.0, 1.0);
        float sampleMask = texture2D(waterMask, sampleUv).b;
        if (sampleMask <= 0.01)
            sampleUv = uv;
    }
    else if (bodyMask > 0.01) {
        sampleUv.x += field * ax;
        sampleUv.y += (field * 0.35 + sin((world.x / 64.0) * 5.5 + time * 0.9) * 0.25) * ay;
        sampleUv = clamp(sampleUv, 0.0, 1.0);
        float sampleMask = texture2D(waterMask, sampleUv).r;
        if (sampleMask <= 0.01)
            sampleUv = uv;
    }

    vec4 base = texture2D(texture, sampleUv);

    if (colorMask > 0.01) {
        // Color layer follows the cave hue/sat/lum filter like the water tiles do.
        vec3 tintedBlend = applyCaveFilter(blendColor);
        float lumScale = mix(0.90, 1.24, light);
        vec3 colored = setLum(tintedBlend, lum(base.rgb) * lumScale);
        float amount = mix(0.45, 0.82, light);
        base.rgb = mix(base.rgb, colored, clamp(colorMask, 0.0, 1.0) * amount);
    }

    gl_FragColor = base;
}
