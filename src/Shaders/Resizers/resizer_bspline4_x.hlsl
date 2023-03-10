// B-spline4, pass X

#define tex2D(s, t) tex2Dlod(s, float4(t, 0., 0.))

sampler s0 : register(s0);
float2 dxdy : register(c0);

float4 main(float2 tex : TEXCOORD0) : COLOR
{
	float t = frac(tex.x);
	float2 pos = tex - float2(t, 0.);
	// original pixels
	float4 Q0 = tex2D(s0, (pos + float2(-.5, .5))*dxdy);
	float4 Q1 = tex2D(s0, (pos + .5)*dxdy);
	float4 Q2 = tex2D(s0, (pos + float2(1.5, .5))*dxdy);
	float4 Q3 = tex2D(s0, (pos + float2(2.5, .5))*dxdy);

	// calculate weights
	float t2 = t*t, t3 = t*t2;
	float4 w0123 = float4(1., 4., 1., 0.) / 6. + float4(-.5, 0., .5, 0.)*t + float4(.5, -1., .5, 0.)*t2 + float4(-1., 3., -3., 1.) / 6.*t3;

	return w0123.x*Q0 + w0123.y*Q1 + w0123.z*Q2 + w0123.w*Q3; // interpolation output
}
