#include "game.h"
#include <vector>

// -------------------------------------------------------------------------------------------------
// super simple "component system"

class GameObject;


// Component base class. Knows about the parent game object.
class Component
{
public:
    Component() : m_GameObject(nullptr) {}
    virtual ~Component() {}

    const GameObject& GetGameObject() const { return *m_GameObject; }
    GameObject& GetGameObject() { return *m_GameObject; }
    void SetGameObject(GameObject& go) { m_GameObject = &go; }
    bool HasGameObject() const { return m_GameObject != nullptr; }

private:
    GameObject* m_GameObject;
};


// Game object class. Has an array of components.
class GameObject
{
public:
    GameObject()
    {
    }
    ~GameObject()
    {
        for (auto c : m_Components)
            delete c;
    }
    
    template<typename T>
    T* GetComponent()
    {
        for (auto i : m_Components)
        {
            T* c = dynamic_cast<T*>(i);
            if (c != nullptr)
                return c;
        }
        return nullptr;
    }
    
    void AddComponent(Component* c)
    {
        assert(!c->HasGameObject());
        c->SetGameObject(*this);
        m_Components.emplace_back(c);
    }
    
private:
    typedef std::vector<Component*> ComponentVector;
    ComponentVector m_Components;
};

// The "scene": array of game objects.
typedef std::vector<GameObject*> GameObjectVector;
static GameObjectVector s_Objects;


// -------------------------------------------------------------------------------------------------
// components we use in our "game"

struct PositionComponent : public Component
{
    float x, y;
};

struct SpriteComponent : public Component
{
    float color[4];
};

struct WorldBoundsComponent : public Component
{
    float xMin, xMax, yMin, yMax;
};


// -------------------------------------------------------------------------------------------------
// "the game"


const int kObjectCount = 100;

static float RandomFloat01()
{
    return (float)rand() / (float)RAND_MAX;
}

extern "C" void game_initialize(void)
{
    for (auto i = 0; i < kObjectCount; ++i)
    {
        GameObject* go = new GameObject();
        
        PositionComponent* pos = new PositionComponent();
        pos->x = RandomFloat01();
        pos->y = RandomFloat01();
        go->AddComponent(pos);
        
        SpriteComponent* sprite = new SpriteComponent();
        sprite->color[0] = 1.0f;
        sprite->color[1] = 1.0f;
        sprite->color[2] = 1.0f;
        sprite->color[3] = 1.0f;
        go->AddComponent(sprite);

        s_Objects.emplace_back(go);
    }
}

extern "C" void game_destroy(void)
{
    for (auto go : s_Objects)
        delete go;
    s_Objects.clear();
}

extern "C" int game_update(sprite_data_t* data, double time, float deltaTime)
{
    int objectCount = 0;
    for (auto go : s_Objects)
    {
        PositionComponent* pos = go->GetComponent<PositionComponent>();
        SpriteComponent* sprite = go->GetComponent<SpriteComponent>();
        if (pos != nullptr && sprite != nullptr)
        {
            sprite_data_t& spr = data[objectCount++];
            spr.posX = pos->x;
            spr.posY = pos->y;
            spr.scale = 0.1f;
            spr.color[0] = sprite->color[0];
            spr.color[1] = sprite->color[1];
            spr.color[2] = sprite->color[2];
            spr.color[3] = sprite->color[3];
        }
    }
    return objectCount;
}

