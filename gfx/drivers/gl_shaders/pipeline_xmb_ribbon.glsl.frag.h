static const char *stock_fragment_xmb =
   "#ifdef GL_ES\n"
   "precision mediump float;\n"
   "#endif\n"
   "uniform float time;\n"
   "varying vec3 vNormal;\n"
   "void main()\n"
   "{\n"
   "  float c = normalize(vNormal).z;\n"
   "  c = (1.0 - cos(c * c)) / 3.0;\n"
   "  gl_FragColor = vec4(1.0, 1.0, 1.0, c);\n"
   "}\n";
