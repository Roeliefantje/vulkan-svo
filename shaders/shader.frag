#version 450

layout(binding = 0) uniform sampler2D inputImage;

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;


void main() {
//    outColor = vec4(texCoord, 1.0, 1.0);
//    outColor = vec4(1.0, 0.0, 1.0, 1.0);
    outColor = texture(inputImage, texCoord);
}