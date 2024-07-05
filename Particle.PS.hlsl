#include "Particle.hlsli"

struct Material
{
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

// cbuffer を使用して定数バッファを定義
cbuffer MaterialBuffer : register(b0)
{
    Material gMaterial;
}

//cbuffer DirectionalLightBuffer : register(b1)
//{
//    DirectionalLight gDirectionalLight;
//}

Texture2D<float4> gTexture : register(t0); // SRVのRegister
// Texture2D<float4> gTexture1 : register(t4); // SRVのRegister

SamplerState gSampler : register(s0); // SamplerのRegister

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    
    PixelShaderOutput output;
    
    // TextureのSampling
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
   
    //// ランバート反射モデルの計算
    //if (gMaterial.enableLighting != 0)
    //{ // Lightngを使用する場合
    //    float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
    //    float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
    //    output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
    //    output.color.a = gMaterial.color.a * textureColor.a;
    //}
    //else
    //{ // Lightngを使用しない場合
    output.color = gMaterial.color * textureColor;
    
    // 透明にするべきところを判断
    if (textureColor.a == 0.0 || textureColor.a <= 0.5 || output.color.a == 0.0)
    {
        discard;
    }
    
    return output;
}
