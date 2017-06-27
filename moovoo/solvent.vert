#version 450

layout (push_constant) uniform Uniform {
  mat4 worldToPerspective;
  mat4 modelToWorld;
  mat4 cameraToWorld;

  vec3 rayStart;
  float timeStep;
  vec3 rayDir;
  uint numAtoms;
  uint numConnections;
  uint pickIndex;
  uint pass;
} u;

layout(std430, binding=1) buffer Solvent {
  vec3 pos[];
} s;

void main() {
  vec3 worldPos = s.pos[gl_VertexIndex];
  gl_Position = u.worldToPerspective * vec4(worldPos, 1.0);
  gl_PointSize = 100;
}

