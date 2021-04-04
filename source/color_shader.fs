#version 150
out vec4 out_color;
in vec3 fColor;

void main() {
    out_color = vec4(fColor, 1.0);
}
