uniform sampler2D texture;
// World-pixel centres of aggro Agents (tile centre). Unused slots ignored.
uniform vec2 center0;
uniform vec2 center1;
uniform vec2 center2;
uniform vec2 center3;
uniform vec2 center4;
uniform vec2 center5;
uniform vec2 center6;
uniform vec2 center7;
uniform int centerCount;
uniform float time;
uniform vec2 viewCenter;
uniform vec2 viewSize;
uniform vec2 rtSize;

vec2 agentCenter(int i) {
    if (i == 0) return center0;
    if (i == 1) return center1;
    if (i == 2) return center2;
    if (i == 3) return center3;
    if (i == 4) return center4;
    if (i == 5) return center5;
    if (i == 6) return center6;
    return center7;
}

// Rounded-square distance: p-norm softer than Chebyshev so corners are not knife-edge.
float roundedSquareDist(vec2 d) {
    vec2 a = abs(d);
    // p=2.6 ? square with clearly softened corners (p?? is sharp Chebyshev).
    const float p = 2.6;
    return pow(pow(max(a.x, 1e-5), p) + pow(max(a.y, 1e-5), p), 1.0 / p);
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;

    vec2 ndc = gl_FragCoord.xy / max(rtSize, vec2(1.0));
    vec2 viewUv = vec2(ndc.x, 1.0 - ndc.y);
    vec2 world = viewCenter + (viewUv - vec2(0.5)) * viewSize;

    // Half-open 10x10 box in tile units (matches Agent::AREA_HALF).
    const float halfTiles = 5.0;
    float bestRing = 0.0;
    vec2 bestPush = vec2(0.0);

    for (int i = 0; i < 8; ++i) {
        if (i >= centerCount)
            break;

        vec2 dTiles = (world - agentCenter(i)) / 32.0;
        float hardCheby = max(abs(dTiles.x), abs(dTiles.y));
        if (hardCheby >= halfTiles)
            continue;

        float dist = roundedSquareDist(dTiles);

        // Two fronts only: emit the next when the previous is halfway out
        // (phase offset 0.5). No idle gap waiting for a wave to fully vanish.
        float ringSum = 0.0;
        for (int w = 0; w < 2; ++w) {
            float phase = float(w) * 0.5;
            float radius = fract(time * 0.45 + phase) * halfTiles;
            float band = abs(dist - radius);
            float crest = 1.0 - smoothstep(0.0, 0.55, band);
            // Soft birth at centre; fade out as the front leaves the box.
            crest *= smoothstep(0.0, 0.4, radius);
            crest *= 1.0 - smoothstep(halfTiles - 1.0, halfTiles, hardCheby);
            crest *= 1.0 - smoothstep(halfTiles - 0.85, halfTiles + 0.15, radius);
            ringSum += crest;
        }
        float ring = min(ringSum, 1.25);

        // Mostly radial push with a light square bias so corners don't kink.
        float len = length(dTiles);
        vec2 pushRadial = (len > 0.001) ? (dTiles / len) : vec2(0.0);
        vec2 pushAxis = abs(dTiles.x) >= abs(dTiles.y)
            ? vec2(sign(dTiles.x), 0.0)
            : vec2(0.0, sign(dTiles.y));
        vec2 push = normalize(mix(pushRadial, pushAxis, 0.28) + vec2(1e-5));
        if (dist < 0.05)
            push = vec2(0.0);

        if (ring > bestRing) {
            bestRing = ring;
            bestPush = push;
        }
    }

    vec2 sampleUv = uv;
    if (bestRing > 0.001) {
        float ax = 5.0 / 640.0;
        float ay = 5.0 / 480.0;
        sampleUv.x += bestPush.x * bestRing * ax;
        sampleUv.y += bestPush.y * bestRing * ay;
        sampleUv = clamp(sampleUv, 0.0, 1.0);
    }

    vec4 base = texture2D(texture, sampleUv);

    // Brightness wave locked to the same crests as the distortion.
    if (bestRing > 0.001) {
        float bright = clamp(bestRing, 0.0, 1.0);
        // Lift luminosity on the crest (same idea as water's Color caustic layer).
        float lumScale = mix(1.0, 1.28, bright);
        base.rgb = clamp(base.rgb * lumScale, 0.0, 1.0);
    }

    gl_FragColor = base;
}
