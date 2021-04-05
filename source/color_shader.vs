#version 150
in vec3 position;
in vec3 vColor;
out vec3 fColor;

uniform float time = 0;

void main() {
    /* Simple model matrix that rotates around the X axis */
    mat4 modelMatrixX = mat4(1.0,        0.0,       0.0, 0.0,  // 1st column
                             0.0,  cos(time), sin(time), 0.0,  // 2nd column
                             0.0, -sin(time), cos(time), 0.0,  // 3rd column
                             0.0,        0.0,       0.0, 1.0); // 4th column

    mat4 modelMatrixY = mat4(cos(time), 0.0, -sin(time), 0.0,  // 1st column
                                   0.0, 1.0,        0.0, 0.0,  // 2nd column
                             sin(time), 0.0,  cos(time), 0.0,  // 3rd column
                                   0.0, 0.0,        0.0, 1.0); // 4th column
    
    gl_Position = modelMatrixY*vec4(position, 1.0);
    fColor = vColor;
}
