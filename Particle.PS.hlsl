#include "Particle.hlsli"

struct Material{
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
};

//struct DirectionalLight{
//    float4 color;
//    float3 direction;
//    float intensity;
//};

ConstantBuffer<Material> gMaterial : register(b0);

//ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t0); //SRVのRegister
//Texture2D<float4> gTexture1 : register(t4); //SRVのRegister

SamplerState gSampler : register(s0); //SamplerのRegister

struct PixelShaderOutput{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input){
    //TextureのSampling
    PixelShaderOutput output;
    float4 transformedUV = mul(float4(input.texcoord,0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    output.color = gMaterial.color * textureColor;
    
    //透明にするべきところを判断
    if (textureColor.a == 0.0){
        discard;
    }
    
    return output;
}