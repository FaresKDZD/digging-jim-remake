uniform sampler2D texture;
uniform vec3 tint; // (hue, saturation, lightness) controls

// Convert RGB to HSL
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

// Convert HSL to RGB
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

void main()
{
    vec4 texColor = texture2D(texture, gl_TexCoord[0].xy);
    vec3 src = texColor.rgb;

    // Vertex colour first (Fish / Genom hue layers, Jim buffs), then cave filter
    // so coloured layers still shift with the level tint. Greyscale sprites with
    // white multiply are unchanged: filter(tex) == filter(tex * 1).
    vec3 coloured = src * gl_Color.rgb;

    // Alpha < 0.5 (Fish spriteTint / Genom wings-eyes-mouth): protect light pixels
    // from the multiply tint before the cave filter runs.
    if (gl_Color.a < 0.5) {
        float lum = max(src.r, max(src.g, src.b));
        float tintAmt = 1.0 - smoothstep(0.55, 0.92, lum);
        coloured = mix(src, coloured, tintAmt);
    }

    // Jim pyrobe / ruby buff used alpha in (0, 0.5) historically; keep multiply
    // semantics for that band (already applied above) then cave-filter the result.
    vec3 outc = applyCaveFilter(coloured);

    gl_FragColor = vec4(outc, texColor.a);
}
