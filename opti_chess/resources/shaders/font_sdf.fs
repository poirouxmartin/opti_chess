#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

void main() {
    float dist = texture(texture0, fragTexCoord).a;
    float width = fwidth(dist);
    float alpha = smoothstep(0.5 - width, 0.5 + width, dist);
    finalColor = vec4(colDiffuse.rgb, alpha * colDiffuse.a);
}
