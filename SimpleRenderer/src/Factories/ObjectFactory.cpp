#include "Factories/ObjectFactory.h"

#include "Renderer/RendererBase.h"

#include "Objects/Object.h"
#include "Objects/Camera.h"
#include "Objects/RotatingModel.h"

#include "Components/MeshComponent.h"

ObjectFactory::ObjectFactory(RendererBase* _renderer, std::vector<Object*>& _objectList)
	: _Renderer(_renderer)
	, ObjectList(_objectList)
{}

Object* ObjectFactory::NewObject(ObjectType type, const ObjectDefinition& _definition)
{
	if (!_Renderer)
	{
		return nullptr;
	}

	Object* obj = nullptr;
	switch(type)
	{
		case ObjectType::MovableCamera:
		{
			obj = new Camera();

			break;
		}
		case ObjectType::RotatingMonkey:
		{
			obj = new RotatingModel(_definition.Parent);

			const std::string& meshID = _Renderer->RegisterMesh(_definition.ModelPath);
			obj->AddComponent(new MeshComponent(obj, _Renderer, meshID));

			break;
		}
	}

	assert("Object type need a factory implementation!", (obj == nullptr));

	ObjectList.push_back(obj);
	return obj;
}