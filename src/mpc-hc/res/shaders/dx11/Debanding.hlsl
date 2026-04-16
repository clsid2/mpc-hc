// $MinimumShaderProfile: ps_4_0

/* --- Settings --- */

#define Threshold   64.0
//[0.0:4096.0] //-The debanding filter's cut-off threshold. Higher numbers increase the debanding strength dramatically but progressively diminish image details. (Default 64)

#define Range       16.0
//[1.0:64.0] //-The debanding filter's initial radius. The radius increases linearly for each iteration. A higher radius will find more gradients, but a lower radius will smooth more aggressively. (Default 16)

#define Iterations  1
//[1:16] //-The number of debanding steps to perform per sample. Each step reduces a bit more banding, but takes time to compute. Note that the strength of each step falls off very quickly, so high numbers (>4) are practically useless. (Default 1)

#define Grain       48.0
//[0.0:4096.0] //-Add some extra noise to the image. This significantly helps cover up remaining quantization artifacts. Higher numbers add more noise. (Default 48)

/* ---  Defining Constants --- */

Texture2D tex : register(t0);
SamplerState samp : register(s0);

cbuffer PS_CONSTANTS : register(b0)
{
	float  px;
	float  py;
	float2 wh;
	uint   counter;
	float  clock;
};

#define pixel float2(px,py)

/* ---  Main code --- */

   /*-----------------------------------------------------------.
  /                     Deband Filter                           /
  '-----------------------------------------------------------*/

float permute(float x) { return ((34.0 * x + 1.0) * x) % 289.0; }
float rand(float x)    { return frac(x * 0.024390243); }


float3 average(float2 coord, float range, inout float h)
{
	// Compute a random rangle and distance
	float dist = rand(h) * range;     h = permute(h);
	float dir  = rand(h) * 6.2831853; h = permute(h);

	float2 pt = dist * pixel;
	float2 o = float2(cos(dir), sin(dir));

	// Sample at quarter-turn intervals around the source pixel
	float3 ref[4];
	ref[0] = tex.Sample(samp, coord + pt * float2( o.x,  o.y)).rgb;
	ref[1] = tex.Sample(samp, coord + pt * float2(-o.y,  o.x)).rgb;
	ref[2] = tex.Sample(samp, coord + pt * float2(-o.x, -o.y)).rgb;
	ref[3] = tex.Sample(samp, coord + pt * float2( o.y, -o.x)).rgb;

	// Return the (normalized) average
	return (ref[0] + ref[1] + ref[2] + ref[3]) * 0.25;
}


/* --- Main --- */

float4 main(float4 pos : SV_POSITION, float2 coord : TEXCOORD) : SV_Target
{
	float h;
	// Initialize the PRNG by hashing the position + a random uniform
	float3 m = float3(coord, clock * 0.0002) + 1.0;
	h = permute(permute(permute(m.x) + m.y) + m.z);

	// Sample the source pixel
	float3 col = tex.Sample(samp, coord).rgb;
	float3 avg; float3 diff;

	#if (Iterations == 1)
		[unroll]
	#endif
	for (int i = 1; i <= Iterations; i++) {
		// Sample the average pixel and use it instead of the original if the difference is below the given threshold
		avg = average(coord, i * Range, h);
		diff = abs(col - avg);
		col = lerp(avg, col, diff > Threshold * 0.00006103515625 * i);
	}

	if (Grain > 0.0) {
		// Add some random noise to smooth out residual differences
		float3 noise;
		noise.x = rand(h); h = permute(h);
		noise.y = rand(h); h = permute(h);
		noise.z = rand(h); h = permute(h);
		col += (Grain * 0.000122070313) * (noise - 0.5);
	}

	return float4(col, 1.0);
}
