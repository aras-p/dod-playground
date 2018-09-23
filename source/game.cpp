#include "game.h"
#include <vector>
#include <string>
#include <math.h>
#include <assert.h>

const int kObjectCount = 1000000;
const int kAvoidCount = 20;



static float RandomFloat01() { return (float)rand() / (float)RAND_MAX; }
static float RandomFloat(float from, float to) { return RandomFloat01() * (to - from) + from; }


// -------------------------------------------------------------------------------------------------
// super simple "component system"

class GameObject;
class Component;

typedef std::vector<Component*> ComponentVector;
typedef std::vector<GameObject*> GameObjectVector;

// C++ dynamic_cast is fairly slow, so roll our own super simple "type identification" system,
// where each component stores an enum for "what type I am?"
enum ComponentType
{
    kCompPosition,
    kCompSprite,
    kCompWorldBounds,
    kCompMove,
};

// Component base class. Knows about the parent game object, and has some virtual methods.
class Component
{
public:
    virtual ~Component() {}
    
    virtual void Start() {}

    const GameObject& GetGameObject() const { return *m_GameObject; }
    GameObject& GetGameObject() { return *m_GameObject; }
    void SetGameObject(GameObject& go) { m_GameObject = &go; }
    bool HasGameObject() const { return m_GameObject != nullptr; }
    
    ComponentType GetType() const { return m_Type; }

protected:
    Component(ComponentType type) : m_GameObject(nullptr), m_Type(type) {}

private:
    GameObject* m_GameObject;
    ComponentType m_Type;
};


// Game object class. Has an array of components.
class GameObject
{
public:
    GameObject(const std::string&& name) : m_Name(name) { }
    ~GameObject()
    {
        // game object owns the components; destroy them when deleting the game object
        for (auto c : m_Components) delete c;
    }

    // get a component of type T, or null if it does not exist on this game object
    template<typename T>
    T* GetComponent()
    {
        for (auto c : m_Components)
        {
            if (c->GetType() == T::kTypeId)
                return (T*)c;
        }
        return nullptr;
    }

    // add a new component to this game object
    void AddComponent(Component* c)
    {
        assert(!c->HasGameObject());
        c->SetGameObject(*this);
        m_Components.emplace_back(c);
    }
    
    void Start() { for (auto c : m_Components) c->Start(); }
    
private:
    std::string m_Name;
    ComponentVector m_Components;
};

// The "scene": array of game objects.
static GameObjectVector s_Objects;


// Finds all components of given type in the whole scene
template<typename T>
static ComponentVector FindAllComponentsOfType()
{
    ComponentVector res;
    for (auto go : s_Objects)
    {
        T* c = go->GetComponent<T>();
        if (c != nullptr)
            res.emplace_back(c);
    }
    return res;
}

// Find one component of given type in the scene (returns first found one)
template<typename T>
static T* FindOfType()
{
    for (auto go : s_Objects)
    {
        T* c = go->GetComponent<T>();
        if (c != nullptr)
            return c;
    }
    return nullptr;
}



// -------------------------------------------------------------------------------------------------
// components we use in our "game"


// 2D position: just x,y coordinates
struct PositionComponent : public Component
{
    enum { kTypeId = kCompPosition };
    PositionComponent() : Component((ComponentType)kTypeId) {}
    
    float x, y;
};


// Sprite: color, sprite index (in the sprite atlas), and scale for rendering it
struct SpriteComponent : public Component
{
    enum { kTypeId = kCompSprite };
    SpriteComponent() : Component((ComponentType)kTypeId) {}

    float colorR, colorG, colorB;
    int spriteIndex;
    float scale;
};


// World bounds for our "game" logic: x,y minimum & maximum values
struct WorldBoundsComponent : public Component
{
    enum { kTypeId = kCompWorldBounds };
    WorldBoundsComponent() : Component((ComponentType)kTypeId) {}
    
    float xMin, xMax, yMin, yMax;
};


// Move around with constant velocity. When reached world bounds, reflect back from them.
struct MoveComponent : public Component
{
    enum { kTypeId = kCompMove };
    
    float velx, vely;

    MoveComponent(float minSpeed, float maxSpeed) : Component((ComponentType)kTypeId)
    {
        // random angle
        float angle = RandomFloat01() * 3.1415926f * 2;
        // random movement speed between given min & max
        float speed = RandomFloat(minSpeed, maxSpeed);
        // velocity x & y components
        velx = cosf(angle) * speed;
        vely = sinf(angle) * speed;
    }
    
    virtual void Start() override;
};

struct MoveSystem
{
    WorldBoundsComponent* bounds;
    std::vector<PositionComponent*> positionList;
    std::vector<MoveComponent*> moveList;

    void AddObjectToSystem(MoveComponent* o)
    {
        positionList.emplace_back(o->GetGameObject().GetComponent<PositionComponent>());
        moveList.emplace_back(o);
    }

    void Initialize()
    {
        bounds = FindOfType<WorldBoundsComponent>();
    }
    
    void UpdateSystem(double time, float deltaTime)
    {
        // go through all the objects
        for (size_t io = 0, no = positionList.size(); io != no; ++io)
        {
            PositionComponent* pos = positionList[io];
            MoveComponent* move = moveList[io];
            
            // update position based on movement velocity & delta time
            pos->x += move->velx * deltaTime;
            pos->y += move->vely * deltaTime;
            
            // check against world bounds; put back onto bounds and mirror the velocity component to "bounce" back
            if (pos->x < bounds->xMin)
            {
                move->velx = -move->velx;
                pos->x = bounds->xMin;
            }
            if (pos->x > bounds->xMax)
            {
                move->velx = -move->velx;
                pos->x = bounds->xMax;
            }
            if (pos->y < bounds->yMin)
            {
                move->vely = -move->vely;
                pos->y = bounds->yMin;
            }
            if (pos->y > bounds->yMax)
            {
                move->vely = -move->vely;
                pos->y = bounds->yMax;
            }
        }
    }
};

static MoveSystem s_MoveSystem;

void MoveComponent::Start()
{
    s_MoveSystem.AddObjectToSystem(this);
}



// "Avoidance system" works out interactions between objects that have AvoidThis and Avoid
// components. Objects with Avoid component:
// - when they get closer to AvoidThis than AvoidThis::distance, they bounce back,
// - also they take sprite color from the object they just bumped into
struct AvoidanceSystem
{
    // things to be avoided: distances to them, and their position components
    std::vector<float> avoidDistanceList;
    std::vector<PositionComponent*> avoidPositionList;
    
    // objects that avoid: their position components
    std::vector<PositionComponent*> objectList;
    
    void AddAvoidThisObjectToSystem(PositionComponent* pos, float distance)
    {
        avoidDistanceList.emplace_back(distance);
        avoidPositionList.emplace_back(pos);
    }
    
    void AddObjectToSystem(PositionComponent* pos)
    {
        objectList.emplace_back(pos);
    }
    
    static float DistanceSq(const PositionComponent* a, const PositionComponent* b)
    {
        float dx = a->x - b->x;
        float dy = a->y - b->y;
        return dx * dx + dy * dy;
    }
    
    void ResolveCollision(PositionComponent* pos, float deltaTime)
    {
        MoveComponent* move = pos->GetGameObject().GetComponent<MoveComponent>();
        // flip velocity
        move->velx = -move->velx;
        move->vely = -move->vely;
        
        // move us out of collision, by moving just a tiny bit more than we'd normally move during a frame
        pos->x += move->velx * deltaTime * 1.1f;
        pos->y += move->vely * deltaTime * 1.1f;
    }
    
    void UpdateSystem(double time, float deltaTime)
    {
        // go through all the objects
        for (size_t io = 0, no = objectList.size(); io != no; ++io)
        {
            PositionComponent* myposition = objectList[io];

            // check each thing in avoid list
            for (size_t ia = 0, na = avoidPositionList.size(); ia != na; ++ia)
            {
                float avDistance = avoidDistanceList[ia];
                PositionComponent* avoidposition = avoidPositionList[ia];
                
                // is our position closer to "thing to avoid" position than the avoid distance?
                if (DistanceSq(myposition, avoidposition) < avDistance * avDistance)
                {
                    ResolveCollision(myposition, deltaTime);
                    
                    // also make our sprite take the color of the thing we just bumped into
                    SpriteComponent* avoidSprite = avoidposition->GetGameObject().GetComponent<SpriteComponent>();
                    SpriteComponent* mySprite = myposition->GetGameObject().GetComponent<SpriteComponent>();
                    mySprite->colorR = avoidSprite->colorR;
                    mySprite->colorG = avoidSprite->colorG;
                    mySprite->colorB = avoidSprite->colorB;
                }
            }
        }
    }
};

static AvoidanceSystem s_AvoidanceSystem;

// -------------------------------------------------------------------------------------------------
// "the game"


extern "C" void game_initialize(void)
{
    // create "world bounds" object
    {
        GameObject* go = new GameObject("bounds");
        WorldBoundsComponent* bounds = new WorldBoundsComponent();
        bounds->xMin = -80.0f;
        bounds->xMax =  80.0f;
        bounds->yMin = -50.0f;
        bounds->yMax =  50.0f;
        go->AddComponent(bounds);
        s_Objects.emplace_back(go);
    }
    WorldBoundsComponent* bounds = FindOfType<WorldBoundsComponent>();
    
    // create regular objects that move
    for (auto i = 0; i < kObjectCount; ++i)
    {
        GameObject* go = new GameObject("object");

        // position it within world bounds
        PositionComponent* pos = new PositionComponent();
        pos->x = RandomFloat(bounds->xMin, bounds->xMax);
        pos->y = RandomFloat(bounds->yMin, bounds->yMax);
        go->AddComponent(pos);

        // setup a sprite for it (random sprite index from first 5), and initial white color
        SpriteComponent* sprite = new SpriteComponent();
        sprite->colorR = 1.0f;
        sprite->colorG = 1.0f;
        sprite->colorB = 1.0f;
        sprite->spriteIndex = rand() % 5;
        sprite->scale = 1.0f;
        go->AddComponent(sprite);

        // make it move
        MoveComponent* move = new MoveComponent(0.5f, 0.7f);
        go->AddComponent(move);

        // make it avoid the bubble things, by adding to the avoidance system
        s_AvoidanceSystem.AddObjectToSystem(pos);

        s_Objects.emplace_back(go);
    }

    // create objects that should be avoided
    for (auto i = 0; i < kAvoidCount; ++i)
    {
        GameObject* go = new GameObject("toavoid");
        
        // position it in small area near center of world bounds
        PositionComponent* pos = new PositionComponent();
        pos->x = RandomFloat(bounds->xMin, bounds->xMax) * 0.2f;
        pos->y = RandomFloat(bounds->yMin, bounds->yMax) * 0.2f;
        go->AddComponent(pos);

        // setup a sprite for it (6th one), and a random color
        SpriteComponent* sprite = new SpriteComponent();
        sprite->colorR = RandomFloat(0.5f, 1.0f);
        sprite->colorG = RandomFloat(0.5f, 1.0f);
        sprite->colorB = RandomFloat(0.5f, 1.0f);
        sprite->spriteIndex = 5;
        sprite->scale = 2.0f;
        go->AddComponent(sprite);
        
        // make it move, slowly
        MoveComponent* move = new MoveComponent(0.1f, 0.2f);
        go->AddComponent(move);
        
        // add to avoidance this as "Avoid This" object
        s_AvoidanceSystem.AddAvoidThisObjectToSystem(pos, 1.3f);
        
        s_Objects.emplace_back(go);
    }
    
    // initialize systems
    s_MoveSystem.Initialize();

    // call Start on all objects/components once they are all created
    for (auto go : s_Objects)
    {
        go->Start();
    }
}


extern "C" void game_destroy(void)
{
    // just delete all objects/components
    for (auto go : s_Objects)
        delete go;
    s_Objects.clear();
}


extern "C" int game_update(sprite_data_t* data, double time, float deltaTime)
{
    int objectCount = 0;
    
    // update object systems
    s_MoveSystem.UpdateSystem(time, deltaTime);
    s_AvoidanceSystem.UpdateSystem(time, deltaTime);

    // go through all objects
    for (auto go : s_Objects)
    {
        // For objects that have a Position & Sprite on them: write out
        // their data into destination buffer that will be rendered later on.
        //
        // Using a smaller global scale "zooms out" the rendering, so to speak.
        float globalScale = 0.05f;
        PositionComponent* pos = go->GetComponent<PositionComponent>();
        SpriteComponent* sprite = go->GetComponent<SpriteComponent>();
        if (pos != nullptr && sprite != nullptr)
        {
            sprite_data_t& spr = data[objectCount++];
            spr.posX = pos->x * globalScale;
            spr.posY = pos->y * globalScale;
            spr.scale = sprite->scale * globalScale;
            spr.colR = sprite->colorR;
            spr.colG = sprite->colorG;
            spr.colB = sprite->colorB;
            spr.sprite = (float)sprite->spriteIndex;
        }
    }
    return objectCount;
}

