uniform sampler2D texture;
uniform vec2 textureSize;
uniform vec2 outputSize;

// Sharp-bilinear upscale: nearest in the texel interior, bilinear only at edges.
// Stops pixel crawl when stretching 640x480 into a non-integer window size.
void main()
{
    vec2 scale = outputSize / textureSize;
    vec2 texel = gl_TexCoord[0].xy * textureSize;
    vec2 texelFloored = floor(texel);
    vec2 s = fract(texel);
    vec2 regionRange = 0.5 - 0.5 / scale;
    vec2 centerDist = s - 0.5;
    vec2 f = (centerDist - clamp(centerDist, -regionRange, regionRange)) * scale + 0.5;
    gl_FragColor = texture2D(texture, (texelFloored + f) / textureSize);
}
