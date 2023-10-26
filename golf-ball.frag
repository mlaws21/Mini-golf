// Lots of code & insperation taken from: https://www.shadertoy.com/view/Xs2fWy

#version 330 core
in vec3 pos;
in vec3 norm;

out vec4 fragColor;
const float nbs = 6.1;
const float ncr0 = 6.;
const float cd = 4.1;
const float cdm = 0.017;
const float psm = 0.0004;
#define pi 3.141593

const float sr = 1.8;
float radial_pattern(vec3 pos)
{
vec2 uv0 = pos.xy;
float a0 = atan(uv0.x, uv0.y);
float ro = acos(abs(pos.z))/pi;

float l = ro*cd;

float sn = floor(0.5 + l*nbs);
float ccr = sn/(cd*nbs); 

float ncr = sn*ncr0;
float ncr2p = ncr/(2.*pi);

// Couldn't use this one yet
//float f1 = l/sqrt(1. - z0*z0);

// Empiric trick so that the holes on the "equator" don't look "compressed"
if (sn>5.)
a0*= (ncr - floor(pow(sn - 6., 1.75)))/ncr;

// To break the symmetry at the "equator"
a0+= pos.z<0.?0.04:0.;

vec2 uv = ro*vec2(sin(a0), cos(a0));

float a = (floor(a0*ncr2p) + 0.5)/ncr2p;
vec2 cpos = ccr*vec2(sin(a), cos(a));

return sn==0.?length(uv):distance(uv, cpos);
}
float linear_remap(float value, float min1, float max1, float min2, float max2) {
return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

float map(vec3 pos)
{   
    vec3 posr = pos;
 
    float d = radial_pattern(normalize(posr));
    
    return length(posr) - sr - 0.022*(pow(smoothstep(-0.001, cdm, d), 1.3) -1.);
}

vec3 getNormal(vec3 pos, float e)
{
vec2 q = vec2(0, e);
vec3 norm = normalize(vec3(map(pos + q.yxx) - map(pos - q.yxx),
                    map(pos + q.xyx) - map(pos - q.xyx),
                    map(pos + q.xxy) - map(pos - q.xxy)));
return norm;
}

vec3 obj_color(vec3 norm, vec3 pos)
{
float d = radial_pattern(normalize(pos));

float ic = smoothstep(-psm, psm, d - cdm);
vec3 colo = mix(vec3(0.8), vec3(0.1), ic); 


return colo;
}
void main() {
    // close enuf ¯\_(ツ)_/¯
    fragColor = vec4(vec3(1.0) - 0.3 * obj_color(norm, pos), 1.0); //vec3(1.0) - 0.3 * 
}

