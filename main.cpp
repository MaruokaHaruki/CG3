#include <Windows.h>
/// ===自作クラス=== ///
#include "base/WinApp.h"
#include "base/DirectXManager.h"
/// ===Win関連=== ///
#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>
/// ===コムポインタ=== ///
#include <wrl.h>
/// ===ファイル読み込み用=== ///
#include <fstream>
#include <sstream>
/// ===自作関数=== //
//構造体
#include "selfMath/structure/Vector3.h"
#include "selfMath/structure/Vector4.h"
#include "selfMath/structure/Matrix4x4.h"
#include "selfMath/structure/Transform.h"
#include "selfMath/structure/Matrix3x3.h"
//3x3行列演算
#include "selfMath/3x3Calc.h"
//4x4行列演算
#include "selfMath/4x4Calc.h"
//3次元アフィン演算
#include"selfMath/3dAffineCalc.h"
//レンダリングパイプライン
#include"selfMath/RendPipeLine.h"
//Wstring変換
#include "base/utils/WstringConve.h"
//ログ出力
#include "base/utils/Log.h"
/// ===DXC=== //
#include <dxcapi.h>
#pragma comment(lib,"dxcompiler.lib")
//DXTex
#include"base/externals/DirectXTex/DirectXTex.h"
/// ===imgui=== //
#include "base/externals/imgui/imgui.h"
#include"base/externals/imgui/imgui_impl_dx12.h"
#include "base/externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


///==============================================///
//構造体
///==============================================///
const struct Vector2 {
	float x;
	float y;
};

/// ===頂点データ=== ///　
const struct VertexData {
	Vector4 position;
	Vector2 texCoord;
	Vector3 normal;
};

/// ===マテリアル=== ///
struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

/// ===トランスレートマトリックス=== ///
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

/// ===並行光源=== ///
//NOTE:光源の色、向き、光度表す。向きは必ず正規化しておくこと。
struct DirectionalLight {
	Vector4 color;		//ライトの色
	Vector3 direction;	//ライトの向き
	float intensity;	//光度

};

/// ===マテリアルデータ=== ///
struct MaterialData {
	std::string textureFilePath;
};

/// ===ModelData=== ///
struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};


///==============================================///
///リソース作成の関数化
///==============================================///
#pragma region リソース作成の関数化
// 頂点リソースを生成する関数
Microsoft::WRL::ComPtr <ID3D12Resource> CreateBufferResource(Microsoft::WRL::ComPtr <ID3D12Device> device, size_t sizeInByte) {
	// バッファリソースの設定を作成
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInByte;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// アップロードヒープのプロパティを設定
	//頂点リソース用のヒープ設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// リソースを作成
	//TODO:一旦回避
	Microsoft::WRL::ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	// エラーチェック
	if (FAILED(hr) || !resource) {
		// リソースの作成に失敗した場合、エラーメッセージを出力して nullptr を返す
		return nullptr;
	}

	return resource;
}
#pragma endregion

///==============================================///
///ウィンドウプロシージャ関数
///==============================================///
#pragma region ウィンドウプロシージャ関数

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	//ImGuiに伝える
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
	//メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		//ウィンドウが破棄された
	case WM_DESTROY:
		//OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}
	//標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}
#pragma endregion

///==============================================///
///CompilerShader関数
///==============================================///
#pragma region CompilerShader関数

IDxcBlob* CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,
	IDxcUtils* dcxUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler) {

	/// ===hlseファイルを読む=== ///
	//これからシェーダーをコンパイルする旨をログに出す
	Log(ConvertString(std::format(L"Begin Compiler,path:{},profile:{}\n", filePath, profile)));
	//hlseファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dcxUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	//読めなかったら止める
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;//UTF-8の文字コードであることを通知

	/// ===コンパイルする=== ///
	LPCWSTR arguments[] = {
	filePath.c_str(),
	L"-E",L"main",
	L"-T",profile,
	L"-Zi",L"-Qembed_debug",
	L"-Od",
	L"-Zpr",
	};
	//実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult)
	);
	//コンパイルエラーではなくdxcが起動できないと致命的な状況
	assert(SUCCEEDED(hr));

	/// ===警告・エラーがでてないか確認する=== ///
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(shaderError->GetStringPointer());
		//警告・エラーダメ絶対
		assert(false);
	}

	/// ===Compile結果を受け取って返す=== ///
	//コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	//成功したログを出す
	Log(ConvertString(std::format(L"Compile Succeeded, path:{},profile:{}\n", filePath, profile)));
	//もう使わないリソースを開放
	shaderSource->Release();
	shaderResult->Release();
	//実行用のバイナリを返却
	return shaderBlob;
}
#pragma endregion

///==============================================///
///DescriptorHeap関数
///==============================================///
//rtvを変更することを忘れない
#pragma region DescriptorHeap関数
Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> CreateDescriptorHeap(Microsoft::WRL::ComPtr <ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	// 成功したログを出力
	Log("Descriptor heap created successfully.");
	return descriptorHeap;
}
#pragma endregion

///==============================================///
///DXTecを使ってデータを読む関数
///==============================================///
DirectX::ScratchImage LoadTexture(const std::string& filePath) {
	/// ===テクスチャファイルを読んでプログラムを扱えるようにする=== ///
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	/// ===ミニマップの作成=== ///
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	/// ===ミニマップ付きのデータを返す=== ///
	return mipImages;
}

///==============================================///
///DirectX12のTextureResourceを作る
///==============================================///
Microsoft::WRL::ComPtr <ID3D12Resource> CreateTextureResource(Microsoft::WRL::ComPtr <ID3D12Device> device, const DirectX::TexMetadata& metadata) {
	/// ===1.metadataを元にResouceの設定=== ///
	D3D12_RESOURCE_DESC resouceDesc{};
	resouceDesc.Width = UINT(metadata.width);								//Textureの幅
	resouceDesc.Height = UINT(metadata.height);								//Textureの高さ
	resouceDesc.MipLevels = UINT16(metadata.mipLevels);						//mipmapの数
	resouceDesc.DepthOrArraySize = UINT16(metadata.arraySize);				//奥行き or 配列Textureの配列数
	resouceDesc.Format = metadata.format;									//TextureのFormat
	resouceDesc.SampleDesc.Count = 1;										//サンプリングカウント
	resouceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);	//Textureの次元数。普段つかているのは2次元。

	/// ===2.利用するHeapの設定===///
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;//細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//プロセッサの近くに配置

	/// ===3.resouceを生成する=== ///
	//TODO:助けて
	Microsoft::WRL::ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,					//Heapの設定
		D3D12_HEAP_FLAG_NONE,				//Heapの特殊な設定、特になし
		&resouceDesc,						//Resourceの設定		
		D3D12_RESOURCE_STATE_GENERIC_READ,	//初回のResouceState。Textureは基本読むだけ
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));
	return resource;
}

///==============================================///
///TextureResouceにデータを転送する
///==============================================///
void UploadTextureData(Microsoft::WRL::ComPtr <ID3D12Resource> texture, const DirectX::ScratchImage& mipImages) {
	/// ===Mata情報を取得=== ///
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	/// ===全MipMapについて=== ///
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
		//全MipMapLevelを指定して書くImageを取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);
		//Textureに転送
		HRESULT hr = texture.Get()->WriteToSubresource(
			UINT(mipLevel),
			nullptr,				//全領域へコピー
			img->pixels,			//元データアドレス
			UINT(img->rowPitch),	//1ラインサイズ
			UINT(img->slicePitch)	//1枚サイズ
		);
		assert(SUCCEEDED(hr));
	}
}

///==============================================///
///深度BufferろステンシルBufferの生成関数
///==============================================///
Microsoft::WRL::ComPtr <ID3D12Resource> CreateDepthStencilTextureResource(Microsoft::WRL::ComPtr <ID3D12Device> device, int32_t width, int32_t height) {
	/// ===生成するResouceの設定=== ///
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;										//テクスチャの幅
	resourceDesc.Height = height;									//テクスチャの高さ
	resourceDesc.MipLevels = 1;										//mipmapの数
	resourceDesc.DepthOrArraySize = 1;								//奥行きor配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;			//DepthStencillとして利用可能なFormat
	resourceDesc.SampleDesc.Count = 1;								//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	//2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	//DepthStencillとして使う通知

	/// ===利用するHeapの設定=== ///
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//VRAM上に作る

	/// ===深度値のクリア設定=== ///
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;//1.0F(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//フォーマット。Resourceと合わせる


	/// ===設定を元にResourceの生成を行う=== ///
	Microsoft::WRL::ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,					//Heapの設定
		D3D12_HEAP_FLAG_NONE,				//heepの特殊な設定。特になし。
		&resourceDesc,						//Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,	//深度値を書き込む状態にしておく
		&depthClearValue,					//Clear最適値
		IID_PPV_ARGS(&resource));			//作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

///==============================================///
///DescriptorHandleの取得を関数化
///==============================================///
/// ===CPU=== ///
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += ( descriptorSize * index );
	return handleCPU;
}
/// ===GPU=== ///
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += ( descriptorSize * index );
	return handleGPU;
}

///=====================================================/// 
///mtlファイルの読み込み
///=====================================================///
MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData;
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}


///
/// OBJファイルを読む関数
///
ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	/// ===1.中で必要となる変数の宣言=== ///
	ModelData modelData;            //構築するModelData
	std::vector<Vector4> positions; //位置
	std::vector<Vector3> normals;   //法線
	std::vector<Vector2> texcoords; //テクスチャ座標
	std::string line;               //ファイルから読んだ1行を格納するもの

	/// ===2.ファイルを開く=== ///
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	/// ===3.実際にファイルを読み、ModelDataを構築していく=== ///
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier; //先頭の識別子を読む

		///identifierに応じた処理
		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f; //NOTE:同次座標を送っているためw=1
			position.x *= -1.0f; //反転
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y; // y軸を反転
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f; //反転
			normals.push_back(normal);

		} else if (identifier == "f") {
			VertexData triangle[3];
			//面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				//頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/'); // /区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				//要素へのIndexから、実際の値を取得して、頂点を構築する
				//NOTE:1始まりなので添字として利用するときは-1を忘れに
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				triangle[faceVertex] = { position, texcoord, normal };
			}
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);

		} else if (identifier == "mtllib") {
			//MaterialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			//基本的にobjファイルと同１改装にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	/// ===4.ModelDataを返す=== ///
	return modelData;
}

///=====================================================/// 
///リソースリークチェッカー
///=====================================================///
struct D3DResourceLeakCheker {
	~D3DResourceLeakCheker() {
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
			//開放を忘れてエラーが出た場合、205行目をコメントアウト
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
			//debug->Release();
		}
	}
};

///==============================================///
///Windowsアプリでのエントリーポイント(main関数)
///==============================================///
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	///----------------------------------------///
	//ウィンドウ生成
	///----------------------------------------///
	WinApp* win = new WinApp;
	win->CreateGameWindow(L"CG3");


	///-------------------------------------------/// 
	///リークチェック
	///-------------------------------------------///
	D3DResourceLeakCheker leakCheck;

	///----------------------------------------///
	//ダイレクトX生成
	///----------------------------------------///
	//インスタンスの取得
	DirectXManager* DXManager = new DirectXManager;
	//ダイレクトXの初期化
	DXManager->InitializeDirectX(win->GetWindowWidth(), win->GetWindowHeight(), win->GetWindowHandle());


	///----------------------------------------///
	//DescriptorHeapのサイズを取得
	///----------------------------------------///
	const uint32_t descriptorSizeSRV = DXManager->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = DXManager->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = DXManager->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);


	///----------------------------------------///
	//srv用DescriptorHeap
	///----------------------------------------///
	//rtvはDXM内
	//SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなのでShaderVisibleはTrue
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(DXManager->GetDevice().Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);


	///----------------------------------------///
	//DXCの初期化
	///----------------------------------------///
	//dxcCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	DXManager->SetHr(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils)));
	assert(SUCCEEDED(DXManager->GetHr()));
	DXManager->SetHr(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler)));
	assert(SUCCEEDED(DXManager->GetHr()));

	//現時点でincludeはしないが、includeに対応するために設定を行う
	IDxcIncludeHandler* includeHandler = nullptr;
	DXManager->SetHr(dxcUtils->CreateDefaultIncludeHandler(&includeHandler));
	assert(SUCCEEDED(DXManager->GetHr()));


	///----------------------------------------///
	//PSO
	///----------------------------------------///
	/// ===RootSignature作成(3DObject)=== ///
	#pragma region RootSignature作成(3DObject)
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	/// DescriptorRangeの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // から始まる
	descriptorRange[0].NumDescriptors = 1; //
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	/// RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0; // b0

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0; // b0

	/// descriptorRangeForInstancing作成
	//rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	//rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	//rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	//rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

	/// DescropterTable
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pParameters = rootParameters;				//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);	//配列の長さ
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	/// DirectionalLight
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	//CBV
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	//PixelShader
	rootParameters[3].Descriptor.ShaderRegister = 1; // b1


	/// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0; // s0
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;				//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);	//配列の長さ
	descriptionRootSignature.pStaticSamplers = staticSamplers;				//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);	//配列の長さ

	/// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr <ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr <ID3DBlob> errorBlob = nullptr;
	DXManager->SetHr(D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob));
	if (FAILED(DXManager->GetHr())) {
		Log(reinterpret_cast<char*>( errorBlob->GetBufferPointer() ));
		assert(false);
	}

	/// バイナリを元に生成
	Microsoft::WRL::ComPtr <ID3D12RootSignature> rootSignature = nullptr;
	DXManager->SetHr(DXManager->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));
	assert(SUCCEEDED(DXManager->GetHr()));
	#pragma endregion

	/// ===RootSignature作成(Particle)=== ///
	#pragma region RootSignature作成(Particle)
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignatureParticle{};
	descriptionRootSignatureParticle.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	///Particle用
	//TODO:
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0; // から始まる
	descriptorRangeForInstancing[0].NumDescriptors = 1; //
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	/// RootParameter作成
	D3D12_ROOT_PARAMETER rootParametersParticle[3] = {};
	rootParametersParticle[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParametersParticle[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParametersParticle[0].Descriptor.ShaderRegister = 0; // b0

	rootParametersParticle[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParametersParticle[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParametersParticle[1].Descriptor.ShaderRegister = 1; // b1

	rootParametersParticle[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParametersParticle[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParametersParticle[2].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootParametersParticle[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

	descriptionRootSignatureParticle.pParameters = rootParametersParticle;               // ルートパラメータ配列へのポインタ
	descriptionRootSignatureParticle.NumParameters = _countof(rootParametersParticle);   // 配列の長さ

	/// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplersParticle[1] = {};
	staticSamplersParticle[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplersParticle[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplersParticle[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplersParticle[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplersParticle[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplersParticle[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplersParticle[0].ShaderRegister = 0; // s0
	staticSamplersParticle[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignatureParticle.pStaticSamplers = staticSamplersParticle;               // ルートパラメータ配列へのポインタ
	descriptionRootSignatureParticle.NumStaticSamplers = _countof(staticSamplersParticle);   // 配列の長さ

	/// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlobParticle = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlobParticle = nullptr;
	DXManager->SetHr(D3D12SerializeRootSignature(&descriptionRootSignatureParticle,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlobParticle, &errorBlobParticle));
	if (FAILED(DXManager->GetHr())) {
		Log(reinterpret_cast<char*>( errorBlobParticle->GetBufferPointer() ));
		assert(false);
	}

	/// バイナリを元に生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureParticle = nullptr;
	DXManager->SetHr(DXManager->GetDevice()->CreateRootSignature(0, signatureBlobParticle->GetBufferPointer(),
		signatureBlobParticle->GetBufferSize(), IID_PPV_ARGS(&rootSignatureParticle)));
	assert(SUCCEEDED(DXManager->GetHr()));

	#pragma endregion




	/// ===InputLayoutの設定を行う=== ///
	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	//頂点データ
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	//画像座標データ
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	//法線データ
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);


	/// ===BlendStateの設定を行う=== ///
	///NormalBlend(ノーマル)
	D3D12_BLEND_DESC normalBlendDesc{};
	//すべての色要素を書き込む
	//TODO:CG3で追加。アルファ値が変更できる
	normalBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	normalBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	//Normal
	normalBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	normalBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	normalBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	normalBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	normalBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	normalBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	///AddBlend(加算合成)
	D3D12_BLEND_DESC addBlendDesc{};
	//すべての色要素を書き込む
	addBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	addBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	//Add
	addBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	addBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	addBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	addBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	addBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	addBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	///SubtractBlend(減算)
	D3D12_BLEND_DESC subtractBlendDesc{};
	//すべての色要素を書き込む
	subtractBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	subtractBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	//Subtract
	subtractBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	subtractBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_SUBTRACT;
	subtractBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	subtractBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	subtractBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	subtractBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	///MultiplyBlend(乗算)
	D3D12_BLEND_DESC multiplyBlendDesc{};
	//すべての色要素を書き込む
	multiplyBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	multiplyBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	//Multiply
	multiplyBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
	multiplyBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	multiplyBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
	multiplyBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	multiplyBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	multiplyBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	///ScreenBlend(乗算)
	D3D12_BLEND_DESC screenBlendDesc{};
	//すべての色要素を書き込む
	multiplyBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	multiplyBlendDesc.RenderTarget[0].BlendEnable = TRUE;
	//Screen
	multiplyBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
	multiplyBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	multiplyBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	multiplyBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	multiplyBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	multiplyBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;



	/// ===RasterrizerStateの設定を行う=== ///
	//RasterrizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面（時計回り）の表示をしない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	/// ===Shaderをcompileする=== ///
	Microsoft::WRL::ComPtr <IDxcBlob> vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl",
		L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr <IDxcBlob> pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl",
		L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderBlob != nullptr);

	/// ===PSOを生成する=== ///
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = normalBlendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//利用するトポロジのタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むかの設定(気にしない)
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	//DepthStencillStateの設定を行う
	D3D12_DEPTH_STENCIL_DESC depthStencillDesc{};
	//Depthの機能を有効化する
	depthStencillDesc.DepthEnable = true;
	//書き込み
	depthStencillDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencillDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//depthStencillの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencillDesc;
	//
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//実際に生成
	Microsoft::WRL::ComPtr <ID3D12PipelineState> graphicsPipelineState = nullptr;
	DXManager->SetHr(DXManager->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&graphicsPipelineState)));
	assert(SUCCEEDED(DXManager->GetHr()));


	///----------------------------------------///
	// VertexResourceを生成(球体)
	///----------------------------------------///

	//// 分割数
	//const uint32_t kSubDivision = 16; // 球体の分割数

	//// 頂点リソースの作成
	//const uint32_t kNumVertices = kSubDivision * kSubDivision * 6; // 全頂点数

	///// ===頂点リソースの作成=== ///
	//ID3D12Resource* vertexResource = CreateBufferResource(DXManager->GetDevice(), sizeof(VertexData) * kNumVertices); // 頂点リソースのバッファを作成

	///// ===VertexBufferViewを作成する=== ///
	//D3D12_VERTEX_BUFFER_VIEW vertexBufferView{}; // 頂点バッファビューの構造体
	//// リソースの先頭のアドレスから使う
	//vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress(); // GPU上のバッファの先頭アドレスを設定
	//// 使用するリソースサイズは頂点3つ分のサイズ
	//vertexBufferView.SizeInBytes = sizeof(VertexData) * kNumVertices; // バッファ全体のサイズを設定
	//// 1頂点あたりのサイズ
	//vertexBufferView.StrideInBytes = sizeof(VertexData); // 各頂点のサイズを設定

	///// ===リソースにデータを書き込む=== ///
	//VertexData* vertexData = nullptr; // 頂点データのポインタを初期化
	//D3D12_RANGE readRange = {}; // 読み取り範囲
	//vertexResource->Map(0, &readRange, reinterpret_cast<void**>( &vertexData )); // バッファのメモリをマッピングして書き込み用のポインタを取得

	//const float kLonEvery = float(M_PI) * 2.0f / float(kSubDivision); // 経度のステップサイズ
	//const float kLatEvery = float(M_PI) / float(kSubDivision); // 緯度のステップサイズ

	//for (uint32_t latIndex = 0; latIndex < kSubDivision; ++latIndex) {
	//	float lat = -float(M_PI) / 2.0f + latIndex * kLatEvery; // 現在の緯度

	//	for (uint32_t lonIndex = 0; lonIndex < kSubDivision; ++lonIndex) {
	//		float lon = lonIndex * kLonEvery; // 現在の経度
	//		uint32_t vertexIndex = ( latIndex * kSubDivision + lonIndex ) * 6; // 頂点インデックスを管理する変数

	//		/// ===1つ目の三角形(左下)=== ///
	//		// NOTE:position = 頂点データ,texcoord = 画像場所データ,normad = 法線データ
	//		// A
	//		vertexData[vertexIndex].position = { cos(lat) * cos(lon), sin(lat), cos(lat) * sin(lon), 1.0f };
	//		vertexData[vertexIndex].texCoord = { float(lonIndex) / kSubDivision, 1.0f - float(latIndex) / kSubDivision };
	//		vertexData[vertexIndex].normal.x = vertexData[vertexIndex].position.x;
	//		vertexData[vertexIndex].normal.y = vertexData[vertexIndex].position.y;
	//		vertexData[vertexIndex].normal.z = vertexData[vertexIndex].position.z;
	//		vertexIndex++;
	//		// B
	//		vertexData[vertexIndex].position = { cos(lat + kLatEvery) * cos(lon), sin(lat + kLatEvery), cos(lat + kLatEvery) * sin(lon), 1.0f };
	//		vertexData[vertexIndex].texCoord = { float(lonIndex) / kSubDivision, 1.0f - float(latIndex + 1) / kSubDivision };
	//		vertexData[vertexIndex].normal.x = vertexData[vertexIndex].position.x;
	//		vertexData[vertexIndex].normal.y = vertexData[vertexIndex].position.y;
	//		vertexData[vertexIndex].normal.z = vertexData[vertexIndex].position.z;
	//		vertexIndex++;
	//		// C
	//		vertexData[vertexIndex].position = { cos(lat) * cos(lon + kLonEvery), sin(lat), cos(lat) * sin(lon + kLonEvery), 1.0f };
	//		vertexData[vertexIndex].texCoord = { float(lonIndex + 1) / kSubDivision, 1.0f - float(latIndex) / kSubDivision };
	//		vertexData[vertexIndex].normal.x = vertexData[vertexIndex].position.x;
	//		vertexData[vertexIndex].normal.y = vertexData[vertexIndex].position.y;
	//		vertexData[vertexIndex].normal.z = vertexData[vertexIndex].position.z;
	//		vertexIndex++;

	//		/// ===2つ目の三角形(右上)=== ///
	//		// C
	//		vertexData[vertexIndex].position = { cos(lat) * cos(lon + kLonEvery), sin(lat), cos(lat) * sin(lon + kLonEvery), 1.0f };
	//		vertexData[vertexIndex].texCoord = { float(lonIndex + 1) / kSubDivision, 1.0f - float(latIndex) / kSubDivision };
	//		vertexData[vertexIndex].normal.x = vertexData[vertexIndex].position.x;
	//		vertexData[vertexIndex].normal.y = vertexData[vertexIndex].position.y;
	//		vertexData[vertexIndex].normal.z = vertexData[vertexIndex].position.z;
	//		vertexIndex++;
	//		// B
	//		vertexData[vertexIndex].position = { cos(lat + kLatEvery) * cos(lon), sin(lat + kLatEvery), cos(lat + kLatEvery) * sin(lon), 1.0f };
	//		vertexData[vertexIndex].texCoord = { float(lonIndex) / kSubDivision, 1.0f - float(latIndex + 1) / kSubDivision };
	//		vertexData[vertexIndex].normal.x = vertexData[vertexIndex].position.x;
	//		vertexData[vertexIndex].normal.y = vertexData[vertexIndex].position.y;
	//		vertexData[vertexIndex].normal.z = vertexData[vertexIndex].position.z;
	//		vertexIndex++;
	//		// D
	//		vertexData[vertexIndex].position = { cos(lat + kLatEvery) * cos(lon + kLonEvery), sin(lat + kLatEvery), cos(lat + kLatEvery) * sin(lon + kLonEvery), 1.0f };
	//		vertexData[vertexIndex].texCoord = { float(lonIndex + 1) / kSubDivision, 1.0f - float(latIndex + 1) / kSubDivision };
	//		vertexData[vertexIndex].normal.x = vertexData[vertexIndex].position.x;
	//		vertexData[vertexIndex].normal.y = vertexData[vertexIndex].position.y;
	//		vertexData[vertexIndex].normal.z = vertexData[vertexIndex].position.z;
	//		vertexIndex++;

	//	}
	//}

	///----------------------------------------///
	// IndexVertexResourceを生成(球体)
	///----------------------------------------///
	//// インデックスバッファの作成
	//const uint32_t kNumIndices = kSubDivision * kSubDivision * 6; // インデックスの数
	//ID3D12Resource* indexResource = CreateBufferResource(DXManager->GetDevice(), sizeof(uint32_t) * kNumIndices);

	//// インデックスバッファビューの作成
	//D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	//indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	//indexBufferView.SizeInBytes = sizeof(uint32_t) * kNumIndices;
	//indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	//// インデックスバッファにデータを書き込む
	//uint32_t* indexData = nullptr;
	//indexResource->Map(0, &readRange, reinterpret_cast<void**>( &indexData ));

	//uint32_t indexCounter = 0;
	//for (uint32_t latIndex = 0; latIndex < kSubDivision; ++latIndex) {
	//	for (uint32_t lonIndex = 0; lonIndex < kSubDivision; ++lonIndex) {
	//		uint32_t currentIndex = latIndex * kSubDivision + lonIndex;

	//		// 三角形1のインデックス
	//		indexData[indexCounter++] = currentIndex * 6 + 0; // A
	//		indexData[indexCounter++] = currentIndex * 6 + 1; // B
	//		indexData[indexCounter++] = currentIndex * 6 + 2; // C

	//		// 三角形2のインデックス
	//		indexData[indexCounter++] = currentIndex * 6 + 3; // C
	//		indexData[indexCounter++] = currentIndex * 6 + 4; // B
	//		indexData[indexCounter++] = currentIndex * 6 + 5; // D
	//	}
	//}

	///-------------------------------------------/// 
	///ModelResourceを生成
	///-------------------------------------------///
	/// ===モデルデータの読み込み=== ///
	ModelData modelData = LoadObjFile("resources/fence", "fence.obj");
	/// ===頂点リソースを作る=== ///
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResource = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(VertexData) * modelData.vertices.size());
	/// ===頂点バッファビューを作成する=== ///
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();				//リソースの先頭アドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());	//使用するリソースのサイズは頂点サイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);									//1頂点あたりのサイズ
	/// ===頂点リソースにデータを書き込む=== ///
	VertexData* vertexData = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>( &vertexData ));
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	///-------------------------------------------/// 
	///particleModelDataを生成
	///-------------------------------------------///
	const int instanceCount = 10;
	/// ===板モデルデータを作成===///
	ModelData modelDataParticle;
	modelDataParticle.vertices.push_back({ .position = {1.0f, 1.0f, 0.0f, 1.0f}, .texCoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左上
	modelDataParticle.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texCoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	modelDataParticle.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texCoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下
	modelDataParticle.vertices.push_back({ .position = {1.0f, -1.0f, 0.0f, 1.0f}, .texCoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下
	modelDataParticle.vertices.push_back({ .position = {-1.0f, 1.0f, 0.0f, 1.0f}, .texCoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	modelDataParticle.vertices.push_back({ .position = {-1.0f, -1.0f, 0.0f, 1.0f}, .texCoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右下
	modelDataParticle.material.textureFilePath = "resources/uvChecker.png";

	/// ===頂点リソースを作る=== ///
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceParticle = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(VertexData) * modelDataParticle.vertices.size());

	/// ===頂点バッファビューを作成する=== ///
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewParticle{};
	vertexBufferViewParticle.BufferLocation = vertexResourceParticle->GetGPUVirtualAddress(); //リソースの先頭アドレスから使う
	vertexBufferViewParticle.SizeInBytes = UINT(sizeof(VertexData) * modelDataParticle.vertices.size()); //使用するリソースのサイズは頂点サイズ
	vertexBufferViewParticle.StrideInBytes = sizeof(VertexData);

	/// ===頂点リソースにデータを書き込む=== ///
	VertexData* vertexDataParticle = nullptr;
	vertexResourceParticle->Map(0, nullptr, reinterpret_cast<void**>( &vertexDataParticle ));
	std::memcpy(vertexDataParticle, modelDataParticle.vertices.data(), sizeof(VertexData)* modelDataParticle.vertices.size());
	vertexResourceParticle->Unmap(0, nullptr);

	/// ===Instancing用のResourceWVP=== ///
	// wvp用のリソースを格納する配列
	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(TransformationMatrix) * instanceCount);
	// データを書き込むためのポインタを格納する配列
	TransformationMatrix* instancingData = nullptr;
	// 書き込むためのアドレスを取得
	instancingResource->Map(0, nullptr, reinterpret_cast<void**>( &instancingData ));

	for (int i = 0; i < instanceCount; ++i) {
		// 単位行列を書き込む
		instancingData[i].WVP = IdentityMatrix();
		instancingData[i].World = IdentityMatrix();
	}

	instancingResource->Unmap(0, nullptr);



	///----------------------------------------///
	//VettexResourceSpriteを生成(スプライト用)
	///----------------------------------------///
	/// ===頂点リソースの作成=== ///
	//NOTE:一般的にこれらのデータはオブジェクト事に必要である
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResouceSprite = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(VertexData) * 6);

	/// ===VettexBufferViewSpriteを作成する=== ///
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResouceSprite->GetGPUVirtualAddress();
	//使用するリソースサイズは頂点3つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6;
	//1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);

	/// ===リソースにデータを書き込む=== ///
	VertexData* vertexDataSprite = nullptr;
	//書き込むためのアドレス
	vertexResouceSprite->Map(0, nullptr,
		reinterpret_cast<void**>( &vertexDataSprite ));
	//左下
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f };
	vertexDataSprite[0].texCoord = { 0.0f,1.0f };
	vertexDataSprite[0].normal = { 0.0f,0.0f,-1.0f };
	//上
	vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f };
	vertexDataSprite[1].texCoord = { 0.0f,0.0f };
	vertexDataSprite[1].normal = { 0.0f,0.0f,-1.0f };
	//右下
	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f };
	vertexDataSprite[2].texCoord = { 1.0f,1.0f };
	vertexDataSprite[2].normal = { 0.0f,0.0f,-1.0f };
	///二番目の三角形
	//左下
	vertexDataSprite[3].position = { 0.0f,0.0f,0.0f,1.0f };
	vertexDataSprite[3].texCoord = { 0.0f,0.0f };
	vertexDataSprite[3].normal = { 0.0f,0.0f,-1.0f };
	//上
	vertexDataSprite[4].position = { 640.0f,0.0f,0.0f,1.0f };
	vertexDataSprite[4].texCoord = { 1.0f,0.0f };
	vertexDataSprite[4].normal = { 0.0f,0.0f,-1.0f };
	//右下
	vertexDataSprite[5].position = { 640.5f,360.0f,0.0f,1.0f };
	vertexDataSprite[5].texCoord = { 1.0f,1.0f };
	vertexDataSprite[5].normal = { 0.0f,0.0f,-1.0f };

	///----------------------------------------///
	//VettexResourceSpriteを生成(スプライト用)
	///----------------------------------------///
	/// ===頂点リソースの作成=== ///
	Microsoft::WRL::ComPtr <ID3D12Resource> indexResourceSprite = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(uint32_t) * 6);

	/// ===VettexBufferViewSpriteを作成する=== ///
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースサイズは頂点3つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//1頂点あたりのサイズ
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	/// ===リソースにデータを書き込む=== ///
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>( &indexDataSprite ));
	indexDataSprite[0] = 0; indexDataSprite[1] = 1; indexDataSprite[2] = 2;
	indexDataSprite[3] = 1; indexDataSprite[4] = 4; indexDataSprite[5] = 2;

	///----------------------------------------///
	//深度用のリソース
	///----------------------------------------///
	/// ===DepthStencilTextureをウィンドウのサイズで作成=== ///
	Microsoft::WRL::ComPtr <ID3D12Resource> depthStencilResource = CreateDepthStencilTextureResource(DXManager->GetDevice().Get(), win->GetWindowWidth(), win->GetWindowHeight());
	/// ===dsv用DescriptorHeap=== ///
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> dsvDescriptorHeap = CreateDescriptorHeap(DXManager->GetDevice().Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
	/// ===dsvの設定=== ///
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2dTexture
	//DSVHeapの先頭にDSVを作る
	DXManager->GetDevice()->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	///----------------------------------------///
	//並行光源用のリソース
	///----------------------------------------///
	Microsoft::WRL::ComPtr <ID3D12Resource> directionalLightResource = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(DirectionalLight));
	//並行光源リソースデータ
	DirectionalLight* directionalLightData = nullptr;
	//並行光源書き込み用データ
	DirectionalLight directionalLight{};
	//書き込むためのアドレス取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>( &directionalLightData ));

	/// ===リソースデータへの書き込み(初期設定)=== ///
	directionalLight.color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLight.direction = { 0.0f,-1.0f,0.0f };
	directionalLight.intensity = 1.0f;
	*directionalLightData = directionalLight;

	///----------------------------------------///
	//マテリアル用のリソース(3D)
	///----------------------------------------///
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResource = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(Material));
	//マテリアルデータ
	Material* materialData = nullptr;
	//マテリアルデータ書き込み用変数
	Material material = { {1.0f, 1.0f, 1.0f, 1.0f},true };
	//書き込むためのアドレス取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>( &materialData ));
	//今回は赤を書き込む
	*materialData = material;
	materialData->uvTransform = IdentityMatrix();

	///----------------------------------------///
	//マテリアル用のリソース(パーティクル)
	///----------------------------------------///
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResourceParticle = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(Material));
	//マテリアルデータ
	Material* materialDataParticle = nullptr;
	//マテリアルデータ書き込み用変数
	Material materialParticle = { {1.0f, 1.0f, 1.0f, 1.0f},true };
	//書き込むためのアドレス取得
	materialResourceParticle->Map(0, nullptr, reinterpret_cast<void**>( &materialDataParticle ));
	//色の書き込み
	*materialDataParticle = materialParticle;
	materialDataParticle->uvTransform = IdentityMatrix();

	///----------------------------------------///
	//マテリアル用のリソース(2D)
	///----------------------------------------///
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResourceSprite = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(Material));
	//マテリアルデータ
	Material* materialDataSprite = nullptr;
	//マテリアルデータ書き込み用変数
	Material materialSprite = { {1.0f, 1.0f, 1.0f, 1.0f},false };
	//書き込むためのアドレス取得
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>( &materialDataSprite ));
	//今回は赤を書き込む
	*materialDataSprite = materialSprite;
	//UVトランスフォーム
	materialDataSprite->uvTransform = IdentityMatrix();

	///----------------------------------------///
	//WVP用のリソース Matrix4x4 1つ分のサイズを用意
	///----------------------------------------///
	//wvp用のリソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> transformationMatrixResource = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* transformationMatrixData = nullptr;
	//書き込み用変数
	TransformationMatrix transformationMatrix;
	//書き込むためのアドレスを取得
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>( &transformationMatrixData ));
	//書き込み
	transformationMatrix.WVP = IdentityMatrix();
	//単位行列を書き込む
	*transformationMatrixData = transformationMatrix;

	///----------------------------------------///
	//Sprite用リソース Matrial4x4 1つ
	///----------------------------------------///
	//wvp用のリソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> transformationMatrixResourceSprite = CreateBufferResource(DXManager->GetDevice().Get(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	//書き込み用変数
	TransformationMatrix transformationMatrixSprite;
	//書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>( &transformationMatrixDataSprite ));
	//書き込み
	transformationMatrixSprite.WVP = IdentityMatrix();
	//単位行列を書き込む
	*transformationMatrixDataSprite = transformationMatrixSprite;


	///----------------------------------------///
	//textureResourceの読み込み
	///----------------------------------------///
	/// ===一枚目=== ///
	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr <ID3D12Resource> textureResource = CreateTextureResource(DXManager->GetDevice(), metadata);
	UploadTextureData(textureResource, mipImages);

	/// ===二枚目=== ///
	//Textureを読んで転送する
	//DirectX::ScratchImage mipImages2 = LoadTexture("resources/monsterBall.png");
	DirectX::ScratchImage mipImages2 = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr <ID3D12Resource> textureResource2 = CreateTextureResource(DXManager->GetDevice(), metadata2);
	UploadTextureData(textureResource2, mipImages2);

	/// ===三枚目=== ///
	DirectX::ScratchImage mipImagesParticle = LoadTexture(modelDataParticle.material.textureFilePath);
	const DirectX::TexMetadata& metadataParticle = mipImagesParticle.GetMetadata();
	Microsoft::WRL::ComPtr <ID3D12Resource> textureResourceParticle = CreateTextureResource(DXManager->GetDevice(), metadataParticle);
	UploadTextureData(textureResourceParticle, mipImagesParticle);


	///----------------------------------------///
	//実際にShaderResourceViewを作る
	///----------------------------------------///
	/// ===一枚目=== ///
	//metaDataを元にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	//SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHadleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHadleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 1);
	//SRVの生成
	DXManager->GetDevice().Get()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHadleCPU);

	/// ===二枚目=== ///
	//metaDataを元にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	//SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHadleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHadleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 2);
	//SRVの生成
	DXManager->GetDevice()->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHadleCPU2);

	/// ===三枚目=== ///
	//metaDataを元にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc3{};
	srvDesc3.Format = metadata.format;
	srvDesc3.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc3.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc3.Texture2D.MipLevels = UINT(metadataParticle.mipLevels);

	//SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHadleCPU3 = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHadleGPU3 = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 3);
	//SRVの生成
	DXManager->GetDevice()->CreateShaderResourceView(textureResourceParticle.Get(), &srvDesc3, textureSrvHadleCPU3);

	/// ===三枚目(Particle用)=== ///
	//TODO:
	D3D12_SHADER_RESOURCE_VIEW_DESC instancindSrvDesc{};
	instancindSrvDesc.Format = DXGI_FORMAT_UNKNOWN;;
	instancindSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancindSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;//2Dテクスチャ
	instancindSrvDesc.Buffer.FirstElement = 0;
	instancindSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancindSrvDesc.Buffer.NumElements = instanceCount;
	instancindSrvDesc.Buffer.StructureByteStride = sizeof(TransformationMatrix);
	//SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE instancindSrvHadleCPU = GetCPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4);
	D3D12_GPU_DESCRIPTOR_HANDLE instancindSrvHadleGPU = GetGPUDescriptorHandle(srvDescriptorHeap.Get(), descriptorSizeSRV, 4);
	//SRVの生成
	DXManager->GetDevice()->CreateShaderResourceView(instancingResource.Get(), &instancindSrvDesc, instancindSrvHadleCPU);


	///----------------------------------------///
	//ViewportとScissor
	///----------------------------------------///
	//ビューポート
	D3D12_VIEWPORT viewport{};
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = win->GetWindowWidth();
	viewport.Height = win->GetWindowHeight();
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1.0f;

	//シザー矩形
	D3D12_RECT scissorRect{};
	//基本的にビューポートと同じ矩形が構成されるようになる
	scissorRect.left = 0;
	scissorRect.right = win->GetWindowWidth();
	scissorRect.top = 0;
	scissorRect.bottom = win->GetWindowHeight();


	///----------------------------------------///
	//コマンドリストの決定
	///----------------------------------------///
	DXManager->CloseCommandList();


	///----------------------------------	------///
	//コマンドキック
	///----------------------------------------///
	DXManager->ExecuteCommandList();


	///----------------------------------------///
	//フェンス生成
	///----------------------------------------///
	DXManager->FenceGeneration();


	///----------------------------------------///
	//Imguiの初期化
	///----------------------------------------///
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(win->GetWindowHandle());
	ImGui_ImplDX12_Init(DXManager->GetDevice().Get(),
		DXManager->GetSwapChainDesc().BufferCount,
		DXManager->GetRtvDesc().Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());


	///----------------------------------------///
	//メインループ用変数
	///----------------------------------------///
	/// ===3Dオブジェクト用=== ///
	//Transform変数を作る
	Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	transform.rotate.y = -3.01f;
	transform.rotate.x = 0.5f;

	/// ===パーティクル用=== ///
	Transform transforms[instanceCount];
	for (int index = 0; index < instanceCount; ++index) {
		transforms[index].scale = { 1.0f,1.0f,1.0f };
		transforms[index].rotate = { 0.0f,-2.5,0.0f };
		transforms[index].translate = { index * 0.1f,index * 0.1f,index * 0.1f };
	}

	/// ===カメラの作成=== ///
	Transform cameraTransform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,-10.0f} };

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	//CPUで動かす用のTransform
	Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	//もう一枚のテクスチャの描画
	bool usaMonsterBall = true;

	Transform uvTransformSprite{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f},
	};

	Transform uvTransform{
	{1.0f,1.0f,1.0f},
	{0.0f,0.0f,0.0f},
	{0.0f,0.0f,0.0f},
	};

	/// ===ImGuiでブレンドモードを選択するための変数=== ///
	int currentBlendMode = 0;
	// 前回のブレンドモードを保持する変数
	int previousBlendMode = -1;
	const char* blendModeItems[] = { "Normal", "Add", "Subtract", "Multiply", "Screen" };
	enum blendMode {
		kBlendModeNormal,
		kBlendModeAdd,
		kBlendModeSubtract,
		kBlendModeMultiply,
		kBlendModeScreen,
	};

	///========================================///
	//メインループ
	///========================================///
	MSG msg{};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {

			///-------------------------------------------/// 
			///ImGui
			///-------------------------------------------///
			// ImGuiのフレーム開始
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			/// ===開発用UIの処理=== ///
			ImGui::Begin("Window");
			// カラーピッカーを表示
			ImGui::Text("3D Material Settings");
			ImGui::ColorPicker4("Color", &material.color.x, 0.01f);
			*materialData = material;
			// 空白とセパレータ
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::Combo("Blend Mode", &currentBlendMode, blendModeItems, IM_ARRAYSIZE(blendModeItems));
			// 現在のブレンドモードを表示
			ImGui::Text("Current Blend Mode: %s", blendModeItems[currentBlendMode]);
			ImGui::Separator();

			ImGui::Text("DirectionalLighting");
			ImGui::DragFloat3("Light direction", &directionalLight.direction.x, 0.01f);
			directionalLight.direction = Normalize(directionalLight.direction);
			*directionalLightData = directionalLight;


			// カメラのトランスフォーム設定
			ImGui::Text("Camera Transform");
			ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.01f);
			ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f);
			// 空白とセパレータ
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::Separator();

			// オブジェクトのトランスフォーム設定
			ImGui::Text("3D Object Transform");
			ImGui::DragFloat3("3D Rotate", &transform.rotate.x, 0.01f);
			ImGui::DragFloat3("3D Translate", &transform.translate.x, 0.01f);
			// 空白とセパレータ
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::Separator();

			// オブジェクトのトランスフォーム設定
			ImGui::Text("2D Object Transform");
			ImGui::DragFloat3("2D Translate", &transformSprite.translate.x, 1.0f);
			// テクスチャの切り替え設定
			ImGui::Text("Texture Settings");
			ImGui::Checkbox("Use Monster Ball", &usaMonsterBall);

			// 空白とセパレータ
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::Separator();

			ImGui::DragFloat2("UVTransform", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat2("UVTScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			ImGui::SliderAngle("UVTransform", &uvTransformSprite.translate.z);

			ImGui::End();


			///-------------------------------------------/// 
			///ゲーム処理
			///-------------------------------------------///
			/// ===ブレンドモードの設定=== /// 
			//ノーマル
			if (previousBlendMode != currentBlendMode) {
				if (currentBlendMode == blendMode::kBlendModeNormal) {
					graphicsPipelineStateDesc.BlendState = normalBlendDesc;
					//加算
				} else if (currentBlendMode == blendMode::kBlendModeAdd) {
					graphicsPipelineStateDesc.BlendState = addBlendDesc;
					//減算
				} else if (currentBlendMode == blendMode::kBlendModeSubtract) {
					graphicsPipelineStateDesc.BlendState = subtractBlendDesc;
					//乗算
				} else if (currentBlendMode == blendMode::kBlendModeMultiply) {
					graphicsPipelineStateDesc.BlendState = multiplyBlendDesc;
					//スクリーン
				} else if (currentBlendMode == blendMode::kBlendModeScreen) {
					graphicsPipelineStateDesc.BlendState = screenBlendDesc;
				}
			}
			/// ===カメラ処理=== ///
			Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
			transformationMatrix.World = worldMatrix;
			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = InverseMatrix(cameraMatrix);

			/// ===3Dオブジェクト処理=== ///
			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(win->GetWindowWidth()) / float(win->GetWindowHeight()), 0.1f, 100.0f);
			Matrix4x4 worldViewProjectionMatrix = MultiplyMatrix(worldMatrix, MultiplyMatrix(viewMatrix, projectionMatrix));
			transformationMatrix.WVP = worldViewProjectionMatrix;
			*transformationMatrixData = transformationMatrix;

			/// ===パーティクル処理=== ///
			//TODO:
			//for (int index = 0; index < instanceCount; ++index) {
			//	Matrix4x4 worldMatrixInstancing = MakeAffineMatrix(transforms[index].scale, transforms[index].rotate, transforms[index].translate);
			//	Matrix4x4 worldViewProjectionMatrix = MultiplyMatrix(worldMatrixInstancing, projectionMatrix);
			//	instancingData[index].WVP = worldViewProjectionMatrix;
			//	instancingData[index].World = worldMatrixInstancing;
			//}



			/// ===2Dオブジェクト処理=== ///
			//sprite用のWorldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			transformationMatrixSprite.World = worldMatrixSprite;
			Matrix4x4 viewMatrxSprite = IdentityMatrix();
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(win->GetWindowWidth()), float(win->GetWindowHeight()), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionMatrixSprite = MultiplyMatrix(worldMatrixSprite, MultiplyMatrix(viewMatrxSprite, projectionMatrixSprite));
			transformationMatrixSprite.WVP = worldViewProjectionMatrixSprite;
			*transformationMatrixDataSprite = transformationMatrixSprite;

			Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = MultiplyMatrix(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = MultiplyMatrix(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
			materialDataSprite->uvTransform = uvTransformMatrix;

			materialData->uvTransform = IdentityMatrix();


			///-------------------------------------------/// 
			///コマンド
			///-------------------------------------------///
			/// ====ImGuiの内部コマンド生成==== ///
			ImGui::Render();

			// バックバッファの決定
			DXManager->SettleCommandList();
			// バリア設定
			DXManager->SetupTransitionBarrier();

			// 描画ターゲットの設定とクリア
			DXManager->RenderTargetPreference(dsvHandle);

			// ImGuiの描画用DescriptorHeap設定
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap.Get() };
			DXManager->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);

			/// ====コマンドを積む==== ///
			// PSOの更新
			if (previousBlendMode != currentBlendMode) {
				DXManager->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
				// 前回のブレンドモードを更新
				previousBlendMode = currentBlendMode;
			}

			/// ===3D描画=== ///
			DXManager->GetCommandList()->RSSetViewports(1, &viewport);
			DXManager->GetCommandList()->RSSetScissorRects(1, &scissorRect);
			DXManager->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
			DXManager->GetCommandList()->SetPipelineState(graphicsPipelineState.Get());
			DXManager->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			DXManager->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());		// DirectionalLight用のCBV設定 (b1)

			///3Dモデル
			//DXManager->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);													//頂点データ
			//DXManager->GetCommandList()->IASetIndexBuffer(&indexBufferView);															//3D_インデックスバッファをバインド
			//DXManager->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());				//マテリアル
			//DXManager->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());	//WVP用リソース
			//DXManager->GetCommandList()->SetGraphicsRootDescriptorTable(2, usaMonsterBall ? textureSrvHadleGPU2 : textureSrvHadleGPU);//切り替え用テクスチャ
			//DXManager->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHadleGPU);										//汎用テクスチャ
			
			///パーティクル用
			DXManager->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());//今他人の借りてるぞ
			DXManager->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewParticle);										//頂点データ
			DXManager->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceParticle->GetGPUVirtualAddress());	//マテリアル
			DXManager->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHadleGPU3);									//テクスチャ

			// 描画コマンド
			//DXManager->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);										//3D_モデルデータ
			DXManager->GetCommandList()->DrawInstanced(UINT(modelDataParticle.vertices.size()), instanceCount, 0, 0);					//Particle_モデルデータ


			/// ===2D描画=== ///
			//NOTE:Material用のCBuffer(色)とSRV(Texture)は3Dの三角形と同じものを使用。無駄を省け。
			//NOTE:同じものを使用したな？気をつけろ、別々の描画をしたいときは個別のオブジェクトとして宣言し直せ。
			//Spriteの描画
			//DXManager->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
			//DXManager->GetCommandList()->IASetIndexBuffer(&indexBufferViewSprite);															//Index使用スプライト
			//DXManager->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());				//マテリアル
			//DXManager->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());	//transformationMatrixVBufferの場所を設定
			//DXManager->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHadleGPU);												//使用するテクスチャ
			//描画！(ドロ‐コール)
			//DXManager->GetCommandList()->DrawInstanced(6, 1, 0, 0);			//通常描画
			//DXManager->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);	//インデックス描画

			// ImGui描画
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), DXManager->GetCommandList().Get());



			/// ===コマンドリストのクローズと実行=== ///
			DXManager->CloseCommandList();
			DXManager->ExecuteCommandList();
		}
	}


	///----------------------------------------///
	//開放処理
	///----------------------------------------///

	/// ===ImGuiの終了処理=== ///
	//srvDescriptorHeap->Release();  // シェーダーリソースビュー用ディスクリプタヒープの解放
	ImGui_ImplDX12_Shutdown();  // ImGuiのDirectX12サポート終了
	ImGui_ImplWin32_Shutdown();  // ImGuiのWin32サポート終了
	ImGui::DestroyContext();  // ImGuiコンテキストの破棄

	/// ===ダイレクトX=== ///
	DXManager->ReleaseDirectX(win->GetWindowHandle());  // DirectXの解放処理
	delete DXManager;

	/// ===ウィンドウの終了=== ///
	win->CloseWindow();

	return 0;
}
