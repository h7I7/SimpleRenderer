#pragma once

#include "Renderer/Renderer.h"
#include "CharTexture.h"

#include <glm.hpp>
#include <Windows.h>
#include <future>
#include <mutex>

enum RENDER_MODE
{
	SINGLE,
	MULTI
};

class ConsoleRenderer : public Renderer
{
public:
	ConsoleRenderer(const unsigned int& _width, const unsigned int& _height);
	virtual ~ConsoleRenderer() override;

	virtual bool Initialise() override;
	virtual const char* RegisterMesh(const char* _meshPath) override;
	virtual void DrawMesh(const char* _modelReference, const glm::mat4& _modelMatrix) override;
	virtual void Flush(const float& _deltaTime) override;

	void Render_Single(const Mesh* _mesh, const glm::mat4& _modelMatrix);
	void Render_Async(const Mesh* _mesh, const glm::mat4& _modelMatrix);
	void RenderRow(const Mesh* _mesh, unsigned int _row, const glm::mat4& _modelMatrix);
	void RenderPixel(const Mesh* _mesh, unsigned int _column, unsigned int _row, const glm::mat4& _modelMatrix);

	void WriteToScreen(int _row, int _col, wchar_t _char);
	void WriteToScreen(int _row, int _col, const std::wstring& _s);

protected:
	std::vector<std::future<void>> m_Futures;
	std::mutex m_Lock;

	// Render parameters
	RENDER_MODE m_Mode = RENDER_MODE::MULTI;
	const int m_Width, m_Height;
	CharTexture m_RenderTex;
	bool m_VSync = false;
	float m_FPS = 60.f;

	wchar_t* m_ScreenBuffer;
	HANDLE m_Console;
};