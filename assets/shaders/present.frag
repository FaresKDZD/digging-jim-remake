uniform sampler2D texture;
uniform vec2 textureSize;
uniform vec2 outputSize;

// Sharp-bilinear (Themaister / Libretro). Ceil plateau gives a larger sharp
// region than floor, which matters for 480→1080 (2.25×) stretch-to-fill.
void main()
{
    vec2 scale = max(ceil(outputSize / textureSize - vec2(1e-5)), vec2(1.0));
    vec2 texel = gl_TexCoord[0].xy * textureSize;
    vec2 texelFloored = floor(texel);
    vec2 s = fract(texel);
    vec2 regionRange = 0.5 - 0.5 / scale;
    vec2 centerDist = s - 0.5;
    vec2 f = (centerDist - clamp(centerDist, -regionRange, regionRange)) * scale + 0.5;
    gl_FragColor = texture2D(texture, (texelFloored + f) / textureSize);
}
