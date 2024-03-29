#pragma once

#include <vector>
#include <map>

class Object;
class Component;
class TransformComponent;
enum COMPONENT_TYPE;

typedef std::map<const unsigned int, Object*> OBJECT_MAP;

enum ObjectType
{
	Default,
	MovableCamera,
	RotatingMonkey
};

class Object
{
public:
	Object(Object* _parent = nullptr);
	~Object();

	inline static ObjectType GetObjectType() { return ObjectType::Default; }

	virtual void Update(const float& _deltaTime);
	virtual void Render();

	inline void SetActive(const bool _active) { bActive = _active; }
	inline bool IsActive() const { return bActive; }

	void AddComponent(Component* _component);
	Component* FindComponentOfType(COMPONENT_TYPE _type);
	Component* operator[](COMPONENT_TYPE _type);

	inline TransformComponent* GetTransform() { return _Transform; }

	unsigned int GetObjectID();

	static OBJECT_MAP GetObjectList();

	inline Object* GetParent() const { return Parent; }

protected:
	bool bActive;

private:
	Object* Parent;

	unsigned int ObjectID;
	std::vector<Component*> Components;

	TransformComponent* _Transform;

	static unsigned int sObjectIDCount;
	static OBJECT_MAP sObjectList;
};

