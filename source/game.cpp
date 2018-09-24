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
// components we use in our "game". these are all just simple structs with some data.


// 2D position: just x,y coordinates
struct PositionComponent
{
    float x, y;
};


// Sprite: color, sprite index (in the sprite atlas), and scale for rendering it
struct SpriteComponent
{
    float colorR, colorG, colorB;
    int spriteIndex;
    float scale;
};


// World bounds for our "game" logic: x,y minimum & maximum values
struct WorldBoundsComponent
{
    float xMin, xMax, yMin, yMax;
};


// Move around with constant velocity. When reached world bounds, reflect back from them.
struct MoveComponent
{
    float velx, vely;

    void Initialize(float minSpeed, float maxSpeed)
    {
        // random angle
        float angle = RandomFloat01() * 3.1415926f * 2;
        // random movement speed between given min & max
        float speed = RandomFloat(minSpeed, maxSpeed);
        // velocity x & y components
        velx = cosf(angle) * speed;
        vely = sinf(angle) * speed;
    }
};


// -------------------------------------------------------------------------------------------------
// super simple "game object system". each object has data for all possible components,
// as well as flags indicating which ones are actually present.

struct GameObject
{
    GameObject(const std::string&& name) : m_Name(name), m_HasPosition(0), m_HasSprite(0), m_HasWorldBounds(0), m_HasMove(0) { }
    ~GameObject() {}
    
    std::string m_Name;
    // data for all components
    PositionComponent m_Position;
    SpriteComponent m_Sprite;
    WorldBoundsComponent m_WorldBounds;
    MoveComponent m_Move;
    // flags for every component, indicating whether this object "has it"
    int m_HasPosition : 1;
    int m_HasSprite : 1;
    int m_HasWorldBounds : 1;
    int m_HasMove : 1;
};


// The "scene": array of game objects.
// "ID" of a game object is just an index into the scene array.
typedef size_t EntityID;
typedef std::vector<GameObject> GameObjectVector;
static GameObjectVector s_Objects;


// -------------------------------------------------------------------------------------------------
// "systems" that we have; they operate on components of game objects


struct MoveSystem
{
    EntityID boundsID; // ID if object with world bounds
    std::vector<EntityID> entities; // IDs of objects that should be moved

    void AddObjectToSystem(EntityID id)
    {
        entities.emplace_back(id);
    }

    void SetBounds(EntityID id)
    {
        boundsID = id;
    }
    
    void UpdateSystem(double time, float deltaTime)
    {
        const WorldBoundsComponent* bounds = &s_Objects[boundsID].m_WorldBounds;

        // go through all the objects
        for (size_t io = 0, no = entities.size(); io != no; ++io)
        {
            PositionComponent* pos = &s_Objects[io].m_Position;
            MoveComponent* move = &s_Objects[io].m_Move;
            
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



// "Avoidance system" works out interactions between objects that "avoid" and "should be avoided".
// Objects that avoid:
// - when they get closer to things that should be avoided than the given distance, they bounce back,
// - also they take sprite color from the object they just bumped into
struct AvoidanceSystem
{
    // things to be avoided: distances to them, and their IDs
    std::vector<float> avoidDistanceList;
    std::vector<EntityID> avoidList;
    
    // objects that avoid: their IDs
    std::vector<EntityID> objectList;

    void AddAvoidThisObjectToSystem(EntityID id, float distance)
    {
        avoidList.emplace_back(id);
        avoidDistanceList.emplace_back(distance);
    }
    
    void AddObjectToSystem(EntityID id)
    {
        objectList.emplace_back(id);
    }
    
    static float DistanceSq(const PositionComponent* a, const PositionComponent* b)
    {
        float dx = a->x - b->x;
        float dy = a->y - b->y;
        return dx * dx + dy * dy;
    }
    
    void ResolveCollision(GameObject& go, float deltaTime)
    {
        PositionComponent* pos = &go.m_Position;
        MoveComponent* move = &go.m_Move;

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
            GameObject& go = s_Objects[objectList[io]];
            PositionComponent* myposition = &go.m_Position;

            // check each thing in avoid list
            for (size_t ia = 0, na = avoidList.size(); ia != na; ++ia)
            {
                float avDistance = avoidDistanceList[ia];
                GameObject& avoid = s_Objects[avoidList[ia]];
                PositionComponent* avoidposition = &avoid.m_Position;
                
                // is our position closer to "thing to avoid" position than the avoid distance?
                if (DistanceSq(myposition, avoidposition) < avDistance * avDistance)
                {
                    ResolveCollision(go, deltaTime);
                    
                    // also make our sprite take the color of the thing we just bumped into
                    SpriteComponent* avoidSprite = &avoid.m_Sprite;
                    SpriteComponent* mySprite = &go.m_Sprite;
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
    s_Objects.reserve(1 + kObjectCount + kAvoidCount);
    
    // create "world bounds" object
    WorldBoundsComponent bounds;
    {
        GameObject go("bounds");
        go.m_WorldBounds.xMin = -80.0f;
        go.m_WorldBounds.xMax =  80.0f;
        go.m_WorldBounds.yMin = -50.0f;
        go.m_WorldBounds.yMax =  50.0f;
        bounds = go.m_WorldBounds;
        go.m_HasWorldBounds = 1;
        s_MoveSystem.SetBounds(s_Objects.size());
        s_Objects.emplace_back(go);
    }
    
    // create regular objects that move
    for (auto i = 0; i < kObjectCount; ++i)
    {
        GameObject go("object");

        // position it within world bounds
        go.m_Position.x = RandomFloat(bounds.xMin, bounds.xMax);
        go.m_Position.y = RandomFloat(bounds.yMin, bounds.yMax);
        go.m_HasPosition = 1;

        // setup a sprite for it (random sprite index from first 5), and initial white color
        go.m_Sprite.colorR = 1.0f;
        go.m_Sprite.colorG = 1.0f;
        go.m_Sprite.colorB = 1.0f;
        go.m_Sprite.spriteIndex = rand() % 5;
        go.m_Sprite.scale = 1.0f;
        go.m_HasSprite = 1;

        // make it move
        go.m_Move.Initialize(0.5f, 0.7f);
        go.m_HasMove = 1;
        s_MoveSystem.AddObjectToSystem(s_Objects.size());

        // make it avoid the bubble things, by adding to the avoidance system
        s_AvoidanceSystem.AddObjectToSystem(s_Objects.size());

        s_Objects.emplace_back(go);
    }

    // create objects that should be avoided
    for (auto i = 0; i < kAvoidCount; ++i)
    {
        GameObject go("toavoid");
        
        // position it in small area near center of world bounds
        go.m_Position.x = RandomFloat(bounds.xMin, bounds.xMax) * 0.2f;
        go.m_Position.y = RandomFloat(bounds.yMin, bounds.yMax) * 0.2f;
        go.m_HasPosition = 1;

        // setup a sprite for it (6th one), and a random color
        go.m_Sprite.colorR = RandomFloat(0.5f, 1.0f);
        go.m_Sprite.colorG = RandomFloat(0.5f, 1.0f);
        go.m_Sprite.colorB = RandomFloat(0.5f, 1.0f);
        go.m_Sprite.spriteIndex = 5;
        go.m_Sprite.scale = 2.0f;
        go.m_HasSprite = 1;
        
        // make it move, slowly
        go.m_Move.Initialize(0.1f, 0.2f);
        go.m_HasMove = 1;
        s_MoveSystem.AddObjectToSystem(s_Objects.size());

        // add to avoidance this as "Avoid This" object
        s_AvoidanceSystem.AddAvoidThisObjectToSystem(s_Objects.size(), 1.3f);
        
        s_Objects.emplace_back(go);
    }
}


extern "C" void game_destroy(void)
{
    s_Objects.clear();
}


extern "C" int game_update(sprite_data_t* data, double time, float deltaTime)
{
    int objectCount = 0;
    
    // update object systems
    s_MoveSystem.UpdateSystem(time, deltaTime);
    s_AvoidanceSystem.UpdateSystem(time, deltaTime);

    // go through all objects
    for (auto&& go : s_Objects)
    {
        // For objects that have a Position & Sprite on them: write out
        // their data into destination buffer that will be rendered later on.
        //
        // Using a smaller global scale "zooms out" the rendering, so to speak.
        float globalScale = 0.05f;
        if (go.m_HasPosition && go.m_HasSprite)
        {
            sprite_data_t& spr = data[objectCount++];
            spr.posX = go.m_Position.x * globalScale;
            spr.posY = go.m_Position.y * globalScale;
            spr.scale = go.m_Sprite.scale * globalScale;
            spr.colR = go.m_Sprite.colorR;
            spr.colG = go.m_Sprite.colorG;
            spr.colB = go.m_Sprite.colorB;
            spr.sprite = (float)go.m_Sprite.spriteIndex;
        }
    }
    return objectCount;
}

