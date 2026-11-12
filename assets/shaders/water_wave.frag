uniform sampler2D texture;
uniform sampler2D waterMask;
uniform float time;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    float mask = texture2D(waterMask, uv).r;
    if (mask <= 0.01) {
        gl_FragColor = texture2D(texture, uv);
        return;
    }

    // Side-to-side wave only on water-covered tiles (~2–3 px at 640×480).
    float ax = 3.0 / 640.0;
    vec2 sampleUv = uv;
    sampleUv.x += sin(uv.y * 18.0 + time) * ax
                + sin(uv.y * 7.0 + time * 1.37) * ax * 0.45;
    sampleUv = clamp(sampleUv, 0.0, 1.0);

    // Stay inside water when sampling so dry neighbors don't bleed in.
    // Edge gaps keep the undistorted wet pixel (same colour as normal water).
    float sampleMask = texture2D(waterMask, sampleUv).r;
    if (sampleMask > 0.01) {
        gl_FragColor = texture2D(texture, sampleUv);
    }
    else {
        gl_FragColor = texture2D(texture, uv);
    }
}
