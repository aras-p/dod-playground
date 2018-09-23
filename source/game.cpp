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


// Component base class. Knows about the parent game object, and has some virtual methods.
class Component
{
public:
    Component() : m_GameObject(nullptr) {}
    virtual ~Component() {}
    
    virtual void Start() {}
    virtual void Update(double time, float deltaTime) {}

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
        for (auto i : m_Components)
        {
            T* c = dynamic_cast<T*>(i);
            if (c != nullptr)
                return c;
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
    void Update(double time, float deltaTime) { for (auto c : m_Components) c->Update(time, deltaTime); }
    
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
    float x, y;
};


// Sprite: color, sprite index (in the sprite atlas), and scale for rendering it
struct SpriteComponent : public Component
{
    float colorR, colorG, colorB;
    int spriteIndex;
    float scale;
};


// World bounds for our "game" logic: x,y minimum & maximum values
struct WorldBoundsComponent : public Component
{
    float xMin, xMax, yMin, yMax;
};


// Move around with constant velocity. When reached world bounds, reflect back from them.
struct MoveComponent : public Component
{
    float velx, vely;
    WorldBoundsComponent* bounds;
    PositionComponent* pos;

    MoveComponent(float minSpeed, float maxSpeed)
    {
        // random angle
        float angle = RandomFloat01() * 3.1415926f * 2;
        // random movement speed between given min & max
        float speed = RandomFloat(minSpeed, maxSpeed);
        // velocity x & y components
        velx = cosf(angle) * speed;
        vely = sinf(angle) * speed;
    }

    virtual void Start() override
    {
        bounds = FindOfType<WorldBoundsComponent>();
        // get Position component on our game object
        pos = GetGameObject().GetComponent<PositionComponent>();
    }
    
    virtual void Update(double time, float deltaTime) override
    {
        // update position based on movement velocity & delta time
        pos->x += velx * deltaTime;
        pos->y += vely * deltaTime;
        
        // check against world bounds; put back onto bounds and mirror the velocity component to "bounce" back
        if (pos->x < bounds->xMin)
        {
            velx = -velx;
            pos->x = bounds->xMin;
        }
        if (pos->x > bounds->xMax)
        {
            velx = -velx;
            pos->x = bounds->xMax;
        }
        if (pos->y < bounds->yMin)
        {
            vely = -vely;
            pos->y = bounds->yMin;
        }
        if (pos->y > bounds->yMax)
        {
            vely = -vely;
            pos->y = bounds->yMax;
        }
    }
};


// When present, tells things that have Avoid component to avoid this object
struct AvoidThisComponent : public Component
{
    float distance;
};


// Objects with this component "avoid" objects with AvoidThis component:
// - when they get closer to them than Avoid::distance, they bounce back,
// - also they take sprite color from the object they just bumped into
struct AvoidComponent : public Component
{
    static ComponentVector avoidList;
    
    PositionComponent* myposition;

    virtual void Start() override
    {
        myposition = GetGameObject().GetComponent<PositionComponent>();

        // fetch list of objects we'll be avoiding, if we haven't done that yet
        if (avoidList.empty())
            avoidList = FindAllComponentsOfType<AvoidThisComponent>();
    }
    
    static float DistanceSq(const PositionComponent* a, const PositionComponent* b)
    {
        float dx = a->x - b->x;
        float dy = a->y - b->y;
        return dx * dx + dy * dy;
    }
    
    void ResolveCollision(float deltaTime)
    {
        MoveComponent* move = GetGameObject().GetComponent<MoveComponent>();
        // flip velocity
        move->velx = -move->velx;
        move->vely = -move->vely;
        
        // move us out of collision, by moving just a tiny bit more than we'd normally move during a frame
        PositionComponent* pos = GetGameObject().GetComponent<PositionComponent>();
        pos->x += move->velx * deltaTime * 1.1f;
        pos->y += move->vely * deltaTime * 1.1f;
    }

    virtual void Update(double time, float deltaTime) override
    {
        // check each thing in avoid list
        for (auto avc : avoidList)
        {
            AvoidThisComponent* av = (AvoidThisComponent*)avc;

            PositionComponent* avoidposition = av->GetGameObject().GetComponent<PositionComponent>();
            // is our position closer to "thing to avoid" position than the avoid distance?
            if (DistanceSq(myposition, avoidposition) < av->distance * av->distance)
            {
                ResolveCollision(deltaTime);

                // also make our sprite take the color of the thing we just bumped into
                SpriteComponent* avoidSprite = av->GetGameObject().GetComponent<SpriteComponent>();
                SpriteComponent* mySprite = GetGameObject().GetComponent<SpriteComponent>();
                mySprite->colorR = avoidSprite->colorR;
                mySprite->colorG = avoidSprite->colorG;
                mySprite->colorB = avoidSprite->colorB;
            }
        }
    }
};

ComponentVector AvoidComponent::avoidList;


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

        // make it avoid the bubble things
        AvoidComponent* avoid = new AvoidComponent();
        go->AddComponent(avoid);

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
        
        // setup an "avoid this" component
        AvoidThisComponent* avoid = new AvoidThisComponent();
        avoid->distance = 1.3f;
        go->AddComponent(avoid);
        
        s_Objects.emplace_back(go);
    }

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
    // go through all objects
    for (auto go : s_Objects)
    {
        // Update all their components
        go->Update(time, deltaTime);

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

