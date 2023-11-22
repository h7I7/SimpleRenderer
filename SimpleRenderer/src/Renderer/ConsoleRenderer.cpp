#include "Renderer/ConsoleRenderer.h"

#include "Tri.h"
#include "Mesh.h"
#include "Util.h"
#include "Components/TransformComponent.h"
#include "Objects/Camera.h"

#include <iostream>
#include <conio.h>
#include <limits>

#include <execution>

// Undefining these macros allows the use of std::numeric_limits<>::max() and std::min without errors
#undef max
#undef min

ConsoleRenderer::ConsoleRenderer(const unsigned int _width, const unsigned int _height)
	: RendererBase(_width, _height)
	, ConsoleBuffer(nullptr)
	, Console(INVALID_HANDLE_VALUE)
	, ThreadPool(std::thread::hardware_concurrency())
{
	CurrentScreenBufferIndex = 0;
	PreviousScreenBufferIndex = 0;

	for (unsigned int i = 0; i < SCREEN_BUFFER_COUNT; ++i)
	{
		ScreenBuffers[i] = new wchar_t[_width * _height];
	}
	DepthData = new float[_width * _height];

	ImageHorizontalIter.resize(_width);
	m_ImageVerticalIter.resize(_height);
	for (unsigned int i = 0; i < _width; ++i)
		ImageHorizontalIter[i] = i;
	for (unsigned int i = 0; i < _height; ++i)
		m_ImageVerticalIter[i] = i;

	Futures.resize(std::thread::hardware_concurrency());

	bRenderThreadContinue = true;
	bIsScreenBufferReady = false;

	bMultithreaded = true;

	ThreadPool.enqueue([this]()
	{
		while(bRenderThreadContinue)
		{
			while(!bIsScreenBufferReady)
			{
				Sleep(5);
			}
			PushToScreen(0.0f);
			bIsScreenBufferReady = false;
		}
	});

	InitialiseConsoleWindow();
}

ConsoleRenderer::~ConsoleRenderer()
{
	bRenderThreadContinue = false;

	for (std::map<const char*, Mesh*>::iterator itr = RegisteredModels.begin(); itr != RegisteredModels.end(); itr++)
	{
		delete itr->second;
	}

	for (unsigned int i = 0; i < SCREEN_BUFFER_COUNT; ++i)
	{
		delete[] ScreenBuffers[i];
	}
	delete[] DepthData;
}

bool ConsoleRenderer::InitialiseConsoleWindow()
{
	ShowConsole();

	// Set font size
	const HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdOut != NULL)
	{
		CONSOLE_FONT_INFOEX fontInfo;
		fontInfo.cbSize = sizeof(CONSOLE_FONT_INFOEX);
		if (GetCurrentConsoleFontEx(hStdOut, FALSE, &fontInfo))
		{
			// The console window seems to size itself correctly when the width and height are below the following values
			// so if our width or height is above these limits we need to manually size the window
			constexpr int MaxWidth = 240;
			constexpr int MaxHeight = 80;

			if (Width > MaxWidth || Height > MaxHeight)
			{
				const float aspectRatio = Width / Height;
				const int newHeight = aspectRatio * Height;
			
				float scale = 0.f;
				if (Width >= newHeight)
				{
					scale = 240.f/Width;
				}
				else
				{
					scale = 80.f/Height;
				}
			
				fontInfo.dwFontSize.X *= scale;
				fontInfo.dwFontSize.Y *= scale;
			}
			else
			{
				fontInfo.dwFontSize.X = 16;
				fontInfo.dwFontSize.Y = 16;
			}
	
			if (!SetCurrentConsoleFontEx(hStdOut, FALSE, &fontInfo))
			{
				std::cerr << "CreateConsoleScreenBuffer win error: " << GetLastError() << '\n';
				return false;
			}
		}
	}

	// Set terminal rows/columns
	std::string sizeCommand = "mode con cols=" + std::to_string(Width) + " lines=" + std::to_string(Height);
	system(sizeCommand.c_str());

	// Create Screen Buffer
	ConsoleBuffer = new wchar_t[Width * Height];
	std::fill_n(ConsoleBuffer, Width * Height, L'a');

	Console = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	if (Console == INVALID_HANDLE_VALUE)
	{
		std::cerr << "CreateConsoleScreenBuffer win error: " << GetLastError() << '\n';
		return false;
	}
	if (!SetConsoleActiveScreenBuffer(Console))
	{
		std::cerr << "CreateConsoleScreenBuffer win error: " << GetLastError() << '\n';
		return false;
	}
	COORD screenBufferSize = {(SHORT)Width, (SHORT)Height};
	if (!SetConsoleScreenBufferSize(Console, screenBufferSize))
	{
		std::cerr << "SetConsoleScreenBufferSize win error: " << GetLastError() << '\n';
		return false;
	}

	WindowHandle = GetConsoleWindow();

	return true;
}

void ConsoleRenderer::Render(const float& _deltaTime)
{ 
	const TransformComponent* cameraTransform = Camera::Main->GetTransform();
	if (!cameraTransform)
	{
		return;
	}

	wchar_t* currentImageData = GetCurrentScreenBuffer();

	// Reset depth texture
	std::fill_n(currentImageData, Width * Height, 0);
	std::fill_n(DepthData, Width * Height, std::numeric_limits<float>::max());

	if (bMultithreaded)
	{
		std::vector<unsigned int> m_ObjectIter;
		m_ObjectIter.resize(ObjectsToRender.size());
		for (unsigned int i = 0; i < ObjectsToRender.size(); ++i)
			m_ObjectIter[i] = i;

		std::for_each(std::execution::par, m_ObjectIter.begin(), m_ObjectIter.end(),
			[this](unsigned int meshIndex)
			{
				Mesh* mesh = RegisteredModels[ObjectsToRender[meshIndex].first];
				TransformComponent* transform = ObjectsToRender[meshIndex].second;

				std::vector<unsigned int> m_TriIter;
				m_TriIter.resize(mesh->m_Triangles.size());
				for (unsigned int i = 0; i < mesh->m_Triangles.size(); ++i)
					m_TriIter[i] = i;

				std::for_each(std::execution::par, m_TriIter.begin(), m_TriIter.end(),
					[this, mesh, transform](unsigned int triIndex)
					{
						RasterizeTri(mesh->m_Triangles[triIndex], transform);
					});
			});
	}
	// Single-threaded
	else
	{
		for (unsigned int meshIndex = 0; meshIndex < ObjectsToRender.size(); ++meshIndex)
		{
			Mesh* mesh = RegisteredModels[ObjectsToRender[meshIndex].first];
			TransformComponent* transform = ObjectsToRender[meshIndex].second;
		
			for (unsigned int triIndex = 0; triIndex < mesh->m_Triangles.size(); ++triIndex)
			{
				RasterizeTri(mesh->m_Triangles[triIndex], transform);
			}
		}
	}

	ObjectsToRender.clear();

	if(!bIsScreenBufferReady)
	{
		bIsScreenBufferReady = true;
		NextScreenBuffer();
	}
}

void ConsoleRenderer::PushToScreen(const float& _deltaTime)
{
	DWORD dwBytesWritten;
	WriteConsoleOutputCharacter(Console, GetPreviousScreenBuffer(), Width * Height, { 0,0 }, &dwBytesWritten);
}

void ConsoleRenderer::RasterizeTri(const Tri& _tri, TransformComponent* const _transform)
{
	if (Camera* mainCamera = Camera::Main)
	{
		Tri transformedTri = mainCamera->TriangleToScreenSpace(_tri, _transform);
		if (transformedTri.Discard)
		{
			transformedTri.Discard = false;
			return;
		}
		
		DrawTriangleToScreen(_tri, transformedTri, _transform);
	}
}

// https://youtu.be/PahbNFypubE?si=WROMBcqVb14EpsfC
void ConsoleRenderer::DrawTriangleToScreen(const Tri& _worldTri, const Tri& _screenSpaceTri, TransformComponent* const _transform)
{
	wchar_t* currentImageData = GetCurrentScreenBuffer();

	// Calculate lighting
	Tri worldSpaceTri = _worldTri * _transform->GetTransformation();
	worldSpaceTri.RecalculateSurfaceNormal();
	glm::vec3 avgPos = (worldSpaceTri.v1 + worldSpaceTri.v2 + worldSpaceTri.v3) / 3.f;
	glm::vec3 lightPos{0.f, 10.f, 0.f};
	glm::vec3 lightDir = glm::normalize(avgPos - lightPos);
	glm::vec3 camToTri = glm::normalize(Camera::Main->GetTransform()->GetPosition() - avgPos);
	glm::vec3 bounce = glm::reflect(camToTri, worldSpaceTri.GetSurfaceNormal());
	float nDotL = glm::dot(lightDir, bounce);

	const wchar_t CharacterToDraw = LightIntensityToAsciiCharacter(nDotL);

	glm::vec4 pos1 = _screenSpaceTri.v1;
	glm::vec4 pos2 = _screenSpaceTri.v2;
	glm::vec4 pos3 = _screenSpaceTri.v3;

	// Calculate lines and triangle direction
	Util::OrderPointsByYThenX(pos2, pos1);
	Util::OrderPointsByYThenX(pos3, pos2);
	Util::OrderPointsByYThenX(pos2, pos1);

	// Early out if triangle has no area
	if (pos1.y == pos3.y)
		return;

	bool facingLeft = pos2.x > pos3.x;

	auto GetSlopePoint = [](const glm::vec3& start, const glm::vec3& end, const int y) -> glm::vec3
	{
		float lowerX = start.x > end.x ? end.x : start.x;
		float higherX = start.x > end.x ? start.x : end.x;
		float lowerZ = start.z > end.z ? end.z : start.z;
		float higherZ = start.z > end.z ? start.z : end.z;
		float x = std::clamp(start.x + (end.x - start.x) * (y - start.y) / (end.y - start.y), lowerX, higherX);
		float z = std::clamp(start.z + (end.z - start.z) * (y - start.y) / (end.y - start.y), lowerZ, higherZ);
		return glm::vec3(x, y, z);
	};

	const int maxCapacity = std::floor(pos1.y) - std::floor(pos3.y) + 1;
	std::vector<std::pair<glm::vec3, glm::vec3>> scanlines(maxCapacity);

	// Get scanlines for top half of triangle
	if (std::floor(pos1.y) != std::floor(pos2.y))
	{
		for (float y = pos1.y; y >= pos2.y; --y)
		{
			glm::vec3 start = GetSlopePoint(pos2, pos1, std::floor(y));
			glm::vec3 end = GetSlopePoint(pos3, pos1, std::floor(y));

			if (start.x < end.x)
				std::swap(start, end);

			scanlines.emplace_back(std::make_pair(std::move(start), std::move(end)));
		}
	}
	
	// Get scanlines for bottom half of triangle
	if (std::floor(pos2.y) != std::floor(pos3.y))
	{
		for (float y = pos2.y; y >= pos3.y; --y)
		{
			glm::vec3 start = GetSlopePoint(pos3, pos2, std::floor(y));
			glm::vec3 end = GetSlopePoint(pos3, pos1,std::floor(y));

			if (start.x < end.x)
				std::swap(start, end);

			scanlines.emplace_back(std::make_pair(std::move(start), std::move(end)));
		}
	}
	
	// Draw scanlines
	for (std::pair<glm::vec3, glm::vec3> scanline : scanlines)
	{
		float distance = scanline.first.x - scanline.second.x;

		float startDepth = scanline.first.z;
		float endDepth = scanline.second.z;

		if (facingLeft)
			std::swap(startDepth, endDepth);
		
		float depthStep = (startDepth - endDepth) / distance;

		int stepCount = 0;
		for (float i = scanline.first.x; i >= scanline.first.x - distance; i -= 1.f)
		{
			int x = std::floor(i);
			int y = std::floor(scanline.first.y);

			if (x < 0 || x > Width - 1 || y < 0 || y > Height - 1)
				continue;
	
			int drawIndex = x + (y * Width);
			float z = scanline.first.z + (depthStep * stepCount);
			if (DepthData[drawIndex] > z)
			{
				currentImageData[drawIndex] = CharacterToDraw;
				DepthData[drawIndex] = z;
			}
			stepCount++;
		}
	}
}

wchar_t ConsoleRenderer::LightIntensityToAsciiCharacter(const float _lightIntensity) const
{
	int index = static_cast<int>(-_lightIntensity * CharacterMapLength);

	if (index < 0)
		index = 0;

	return CharacterMap[index];
}