//NOTE:すべての構成からPS,VSを除外しないと使えねぇぞ！
struct VertexShaderOutput{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};