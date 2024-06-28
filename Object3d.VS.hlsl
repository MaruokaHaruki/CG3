#include "Object3d.hlsli"

struct TransformationMatrix{
    float4x4 WVP;
    float4x4 World;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input){
    VertexShaderOutput output;
    output.texcoord = input.texcoord;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    //NOTE:法線の変換には拡縮回転情報のみが必要なため取り出す処理を行っている
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.World));
    
    return output;
}