attribute vec4 posAttr;
attribute vec4 texCoord;

varying  vec2 vTexCoord;
uniform  mat4 matrix;
attribute  vec4 colAttr;
varying  vec4 col;

void main(void)
{
    vec4 rotm;

    col = vec4((posAttr.x+1.0)/2.0,0,0,0);
    gl_Position = posAttr;
     rotm= texCoord*matrix;
    vTexCoord =vec2 (rotm.x, rotm.y);// vec2((posAttr.x+1.0)/2.0,1.0-((posAttr.y+1.0)/2.0));

}



