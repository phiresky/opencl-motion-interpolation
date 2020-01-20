varying vec3 norm;
varying vec4 viewPos;
varying vec2 tex;

uniform sampler2D texDiffuse;

void main()
{
	vec4 surfColor = vec4(tex.x, tex.y, 0.5, 1.0);
	
	surfColor = texture( texDiffuse, tex.xy );
	
	gl_FragColor = surfColor;
}