#include "Components/MeshComponent.h"

#include "Renderer/RendererBase.h"

MeshComponent::MeshComponent(Object* _owner, RendererBase* _renderer, const std::string& meshID)
	: Component(_owner)
	, bVisible(true)
	, _Renderer(_renderer)
{
	Type = MESH;

	if (_Renderer)
	{
		MeshID = _Renderer->RegisterMesh(meshID);
	}
	else
	{
		bVisible = false;
	}
}

MeshComponent::~MeshComponent()
{}

void MeshComponent::Update(const float& _deltaTime)
{}

void MeshComponent::Render()
{
	if (!bVisible)
	{
		return;
	}

	TransformComponent* t = GetTransform();
	if (!t)
	{
		return;
	}

	_Renderer->DrawMesh(MeshID, t);
}
