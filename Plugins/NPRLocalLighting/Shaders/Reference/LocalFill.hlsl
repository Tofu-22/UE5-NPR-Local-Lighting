// Runtime fill controls: shape, gain and optional ceiling have separate jobs.
float4 PR[2] = { L0PositionRadius, L1PositionRadius };
float4 CW[2] = { L0ColorWeight, L1ColorWeight };
float4 DO[2] = { L0DirectionOuter, L1DirectionOuter };
float4 CT[2] = { L0Control, L1Control };
float3 Fill = 0;
[unroll] for (int i = 0; i < 2; ++i)
{
    float Radius = PR[i].w * max(RadiusScale, 0);
    if (Radius <= 0 || CW[i].w <= 0) continue;
    float3 ToPixel = RelativePixel - PR[i].xyz;
    float Distance = length(ToPixel);
    // Softness is the fractional INNER transition width. The outer radius stays fixed.
    // 0 = intentionally hard edge; 0.25 = fade over the outer quarter of the radius.
    float Softness = saturate(EdgeSoftness);
    float T = saturate((1 - Distance / max(Radius, 0.00001)) / max(Softness, 0.00001));
    float Sphere = Softness <= 0 ? (1 - step(Radius, Distance)) : T * T * (3 - 2 * T);
    float Mask = Sphere * saturate(AO);
    float Cone = 1;
    if (CT[i].y > 0.5)
    {
        float Cos = dot(ToPixel / max(Distance, 0.00001), DO[i].xyz);
        float ConeT = saturate((Cos - DO[i].w) / max(CT[i].x - DO[i].w, 0.00001));
        Cone = ConeT * ConeT * (3 - 2 * ConeT);
    }
    float3 FillColor = UseCustomColor > 0.5 ? max(PLTint, 0) : max(CW[i].xyz, 0);
    Fill += Mask * Cone * FillColor * max(CW[i].w, 0);
}
Fill *= max(Strength, 0);
// MaxAdd limits only the additive fill (not the original hair). It is not a gain.
if (LimitEnabled <= 0.5) return Fill;
float Peak = max(Fill.r, max(Fill.g, Fill.b));
return Fill * (Peak > 0 ? min(1, max(MaxAdd, 0) / Peak) : 0);
