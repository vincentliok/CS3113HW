#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengl.h>
#include <SDL_image.h>
#include "ShaderProgram.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <vector>
#include "FlareMap.h"
#include <algorithm>
#include <SDL_mixer.h>

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define PI 3.14159265359f
#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6
#define TILE_SIZE 0.2f

//global variables

//physics

glm::vec3 friction = glm::vec3(4.0f, 0.0f, 0.0f);
glm::vec3 gravity = glm::vec3(0.0f, -4.0f, 0.0f);

//level

std::vector<int> solidTiles{3};
std::vector<int> hazardTiles{100, 101};
FlareMap level;
int levelNum = 1;

//miscellaneous

bool landed;
bool lookingLeft;

//game mode

enum GameMode {STATE_MAIN_MENU, STATE_GAME_LEVEL, STATE_GAME_OVER, STATE_VICTORY, STATE_PAUSE};
GameMode mode;

//time

float lastFrameTicks = 0.0f;
float accumulator = 0.0f;
float framesPerSecond = 10.0f;
float framesPerSecondMenu = 1.0f;
float currentTimeLevelText = 0.0f;
float maxTimeLevelText = 3.0f;

//player animation

float animationElapsedPlayer = 0.0f;
const int moveAnimationPlayer[] = {8, 9, 10, 11};
const int numFramesPlayer = 4;
int currentIndexPlayer = 0;

//setup

SDL_Window* displayWindow;
ShaderProgram programNonTextured;
ShaderProgram programTextured;
SDL_Event event;
bool done = false;
glm::mat4 modelMatrix = glm::mat4(1.0f);
glm::mat4 viewMatrix = glm::mat4(1.0f);

//helper functions

float lerp(float v0, float v1, float t) {
    return (1.0-t)*v0 + t*v1;
}

void worldToTileCoordinates(float worldX, float worldY, int &gridX, int &gridY) {
    gridX = (int)(worldX / TILE_SIZE);
    gridY = (int)(worldY / -TILE_SIZE);
}

float RandomFloat(float a, float b) {
    float random = ((float) rand()) / (float) RAND_MAX;
    float diff = b - a;
    float r = random * diff;
    return a + r;
}

//drawing stuff

class SheetSprite {
public:
    SheetSprite() {};
    SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size):
    textureID(textureID), u(u), v(v), width(width), height(height), size(size) {};
    
    void Draw(ShaderProgram &program) const;
    
    float size;
    unsigned int textureID;
    float u;
    float v;
    float width;
    float height;
};

void SheetSprite::Draw(ShaderProgram &program) const {
    glBindTexture(GL_TEXTURE_2D, textureID);
    GLfloat texCoords[] = {
        u, v+height,
        u+width, v,
        u, v,
        u+width, v,
        u, v+height,
        u+width, v+height
    };
    float aspect = width / height;
    float vertices[] = {
        -0.5f * size * aspect, -0.5f * size,
        0.5f * size * aspect, 0.5f * size,
        -0.5f * size * aspect, 0.5f * size,
        0.5f * size * aspect, 0.5f * size,
        -0.5f * size * aspect, -0.5f * size ,
        0.5f * size * aspect, -0.5f * size};
    
    glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
    glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

class SheetSpriteUniform {
public:
    SheetSpriteUniform() {};
    SheetSpriteUniform(unsigned int textureID, int index, int spriteCountX, int spriteCountY, float size,
                       float offsetX, float offsetY):
    textureID(textureID), index(index), spriteCountX(spriteCountX), spriteCountY(spriteCountY), size(size),
    offsetX(offsetX), offsetY(offsetY) {};
    
    void Draw(ShaderProgram &program) const;
    
    float size;
    unsigned int textureID;
    int index;
    int spriteCountX;
    int spriteCountY;
    float offsetX;
    float offsetY;
};

void SheetSpriteUniform::Draw(ShaderProgram &program) const {
    glBindTexture(GL_TEXTURE_2D, textureID);
    float u = (float)(((int)index) % spriteCountX) / (float) spriteCountX;
    float v = (float)(((int)index) / spriteCountX) / (float) spriteCountY;
    float spriteWidth = 1.0/(float)spriteCountX;
    float spriteHeight = 1.0/(float)spriteCountY;
    float texCoords[] = {
        u, v+spriteHeight,
        u+spriteWidth, v,
        u, v,
        u+spriteWidth, v,
        u, v+spriteHeight,
        u+spriteWidth, v+spriteHeight
    };
    float vertices[] = {
        -0.5f * size + offsetX, -0.5f * size + offsetY,
        0.5f * size + offsetX, 0.5f * size + offsetY,
        -0.5f * size + offsetX, 0.5f * size + offsetY,
        0.5f * size + offsetX, 0.5f * size + offsetY,
        -0.5f * size + offsetX, -0.5f * size + offsetY,
        0.5f * size + offsetX, -0.5f * size + offsetY};
    
    glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
    glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

GLuint LoadTexture(const char *filePath, bool useNearest) {
    int w,h,comp;
    unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);
    if(image == NULL) {
        std::cout << "Unable to load image. Make sure the path is correct\n";
        assert(false);
    }
    GLuint retTexture;
    glGenTextures(1, &retTexture);
    glBindTexture(GL_TEXTURE_2D, retTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    if (useNearest) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    stbi_image_free(image);
    return retTexture;
}

//entity stuff

enum EntityType {ENTITY_PLAYER, ENTITY_ENEMY, ENTITY_JUMPER};

class Entity {
public:
    void Draw(float elapsed) {
        
        //uniform
        
        if (usesUniform) {
            
            //player
            
            if (entitytype == ENTITY_PLAYER) {
                
                //jump sprite
                
                if (velocity.y > 0) {
                    modelMatrix = glm::mat4(1.0f);
                    modelMatrix = glm::translate(modelMatrix, position);
                    if (lookingLeft) {
                        modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
                    }
                    programTextured.SetModelMatrix(modelMatrix);
                    spriteUniform.index = 13;
                    spriteUniform.Draw(programTextured);
                }
                
                //fall sprite
                
                else if (velocity.y < 0) {
                    modelMatrix = glm::mat4(1.0f);
                    modelMatrix = glm::translate(modelMatrix, position);
                    if (lookingLeft) {
                        modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
                    }
                    programTextured.SetModelMatrix(modelMatrix);
                    spriteUniform.index = 14;
                    spriteUniform.Draw(programTextured);
                }
                
                //walking animation
                
                else if (acceleration.x != 0) {
                    animationElapsedPlayer += elapsed;
                    if (animationElapsedPlayer > 1.0/framesPerSecond) {
                        currentIndexPlayer++;
                        animationElapsedPlayer = 0.0;
                        if (currentIndexPlayer > numFramesPlayer-1) {
                            currentIndexPlayer = 0;
                        }
                    }
                    modelMatrix = glm::mat4(1.0f);
                    modelMatrix = glm::translate(modelMatrix, position);
                    if (acceleration.x < 0) {
                        modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
                    }
                    programTextured.SetModelMatrix(modelMatrix);
                    spriteUniform.index = moveAnimationPlayer[currentIndexPlayer];
                    spriteUniform.Draw(programTextured);
                }
                
                //idle sprite
                
                else {
                    modelMatrix = glm::mat4(1.0f);
                    modelMatrix = glm::translate(modelMatrix, position);
                    if (lookingLeft) {
                        modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
                    }
                    programTextured.SetModelMatrix(modelMatrix);
                    spriteUniform.index = 10;
                    spriteUniform.Draw(programTextured);
                }
            }
            
            //enemies
            
            if (entitytype == ENTITY_ENEMY) {
                
                //walking animation
                
                animationElapsedEnemy += elapsed;
                if (animationElapsedEnemy > 1.0/framesPerSecond) {
                    currentIndexEnemy++;
                    animationElapsedEnemy = 0;
                    if (currentIndexEnemy > numFramesEnemy-1) {
                        currentIndexEnemy = 0;
                    }
                }
                modelMatrix = glm::mat4(1.0f);
                modelMatrix = glm::translate(modelMatrix, position);
                if (acceleration.x < 0) {
                    modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
                }
                programTextured.SetModelMatrix(modelMatrix);
                spriteUniform.index = moveAnimationEnemy[currentIndexEnemy];
                spriteUniform.Draw(programTextured);
            }
            
            //jumping animation
            
            if (entitytype == ENTITY_JUMPER) {
                modelMatrix = glm::mat4(1.0f);
                modelMatrix = glm::translate(modelMatrix, position);
                modelMatrix = glm::scale(modelMatrix, glm::vec3(-1.0f, 1.0f, 1.0f));
                programTextured.SetModelMatrix(modelMatrix);
                if (velocity.y > 0) {
                    spriteUniform.index = 81;
                }
                else {
                    spriteUniform.index = 80;
                }
                spriteUniform.Draw(programTextured);
            }
        }
        
        //non-uniform
        
        else {
            modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, position);
            programTextured.SetModelMatrix(modelMatrix);
            sprite.Draw(programTextured);
        }
    }
    
    void Update(float elapsed) {
        velocity.x = lerp(velocity.x, 0.0f, elapsed * friction.x);
        velocity.y = lerp(velocity.y, 0.0f, elapsed * friction.y);
        velocity.x += acceleration.x * elapsed;
        velocity.y += gravity.y * elapsed;
        position.x += velocity.x * elapsed;
        position.y += velocity.y * elapsed;
        
        tileCollision();
        hazardCollision();
        
        //enemy think function
        
        if (entitytype == ENTITY_ENEMY) {
            int gridposx = 0;
            int gridposy = 0;
            
            //check bottom right tile
            
            worldToTileCoordinates(position.x + size.x, position.y - size.y, gridposx, gridposy);
            if (((gridposx > 0 && gridposy > 0) && (gridposx < level.mapWidth && gridposy < level.mapHeight)) &&
                (!(std::find(solidTiles.begin(), solidTiles.end(), level.mapData[gridposy][gridposx]) != solidTiles.end()) ||
                 (std::find(hazardTiles.begin(), hazardTiles.end(), level.mapData[gridposy][gridposx]) != hazardTiles.end()))) {
                acceleration.x = -abs(acceleration.x);
            }
            
            //check bottom left tile
            
            worldToTileCoordinates(position.x - size.x, position.y - size.y, gridposx, gridposy);
            if (((gridposx > 0 && gridposy > 0) && (gridposx < level.mapWidth && gridposy < level.mapHeight)) &&
                (!(std::find(solidTiles.begin(), solidTiles.end(), level.mapData[gridposy][gridposx]) != solidTiles.end()) ||
                 (std::find(hazardTiles.begin(), hazardTiles.end(), level.mapData[gridposy][gridposx]) != hazardTiles.end()))) {
                acceleration.x = abs(acceleration.x);
            }
            
            //check left and right tiles
            
            if (collidedRight == true || collidedLeft == true) {
                acceleration.x = -acceleration.x;
            }
            
            //check right tile for hazards
            
            worldToTileCoordinates(position.x + size.x, position.y, gridposx, gridposy);
            if (((gridposx > 0 && gridposy > 0) && (gridposx < level.mapWidth && gridposy < level.mapHeight)) &&
                (std::find(hazardTiles.begin(), hazardTiles.end(), level.mapData[gridposy][gridposx]) != hazardTiles.end())) {
                acceleration.x = -abs(acceleration.x);
            }
            
            //check left tile for hazards
            
            worldToTileCoordinates(position.x - size.x, position.y, gridposx, gridposy);
            if (((gridposx > 0 && gridposy > 0) && (gridposx < level.mapWidth && gridposy < level.mapHeight)) &&
                (std::find(hazardTiles.begin(), hazardTiles.end(), level.mapData[gridposy][gridposx]) != hazardTiles.end())) {
                acceleration.x = abs(acceleration.x);
            }
        }
        
        if (entitytype == ENTITY_JUMPER) {
            if (collidedBottom) {
                velocity.y = jumpVelocity;
            }
        }
    }
    
    void tileCollision() {
        int gridposx = 0;
        int gridposy = 0;
        collidedBottom = false;
        collidedTop = false;
        collidedLeft = false;
        collidedRight = false;
        
        //bottom collision flag
        
        worldToTileCoordinates(position.x, position.y - (size.y/2.0), gridposx, gridposy);
        if (((gridposx > 0 && gridposy > 0) && (gridposx < level.mapWidth && gridposy < level.mapHeight)) &&
            std::find(solidTiles.begin(), solidTiles.end(), level.mapData[gridposy][gridposx]) != solidTiles.end()) {
            position.y += fabs((-TILE_SIZE * gridposy)-(position.y - (size.y/2.0))) + 0.00001;
            velocity.y = 0;
            collidedBottom = true;
        }
        
        //top collision flag
        
        worldToTileCoordinates(position.x, position.y + (size.y/2.0), gridposx, gridposy);
        if (((gridposx > 0 && gridposy > 0) && (gridposx < level.mapWidth && gridposy < level.mapHeight)) &&
            std::find(solidTiles.begin(), solidTiles.end(), level.mapData[gridposy][gridposx]) != solidTiles.end()) {
            position.y -= fabs(((-TILE_SIZE * gridposy)-TILE_SIZE)-(position.y + (size.y/2.0))) - 0.00001;
            velocity.y = 0;
            collidedTop = true;
        }
        
        //left collision flag
        
        worldToTileCoordinates(position.x - (size.x/2.0), position.y, gridposx, gridposy);
        if (((gridposx > 0 && gridposy > 0) && (gridposx < level.mapWidth && gridposy < level.mapHeight)) &&
            std::find(solidTiles.begin(), solidTiles.end(), level.mapData[gridposy][gridposx]) != solidTiles.end()) {
            position.x += fabs(((TILE_SIZE * gridposx)+TILE_SIZE)-(position.x - (size.x/2.0))) + 0.00001;
            velocity.x = 0;
            collidedLeft = true;
        }
        
        //right collision flag
        
        worldToTileCoordinates(position.x + (size.x/2.0), position.y, gridposx, gridposy);
        if (((gridposx > 0 && gridposy > 0) && (gridposx < level.mapWidth && gridposy < level.mapHeight)) &&
            std::find(solidTiles.begin(), solidTiles.end(), level.mapData[gridposy][gridposx]) != solidTiles.end()) {
            position.x -= fabs(((TILE_SIZE * gridposx))-(position.x + (size.x/2.0))) + 0.00001;
            velocity.x = 0;
            collidedRight = true;
        }
    }
    
    void hazardCollision() {
        int gridposx = 0;
        int gridposy = 0;
        
        //check spikes
        
        worldToTileCoordinates(position.x, position.y, gridposx, gridposy);
        if (((gridposx > 0 && gridposy > 0) && (gridposx < level.mapWidth && gridposy < level.mapHeight)) &&
            std::find(hazardTiles.begin(), hazardTiles.end(), level.mapData[gridposy][gridposx]) != hazardTiles.end()) {
            if (entitytype == ENTITY_PLAYER) {
                mode = STATE_GAME_OVER;
            }
        }
        
        //check fall
        
        if (gridposy >= level.mapHeight) {
            if (entitytype == ENTITY_PLAYER) {
                mode = STATE_GAME_OVER;
            }
        }
    }
    
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    glm::vec3 size;
    float jumpVelocity;
    
    SheetSprite sprite;
    SheetSpriteUniform spriteUniform;
    
    bool usesUniform;
    
    bool collidedTop;
    bool collidedBottom;
    bool collidedLeft;
    bool collidedRight;
    
    EntityType entitytype;
    
    float animationElapsedEnemy = 0.0f;
    int currentIndexEnemy = 0;
    const std::vector<int> moveAnimationEnemy{80, 81};
    const int numFramesEnemy = 2;
};

//particle stuff

class Particle {
public:
    Particle() {};
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime;
    glm::vec4 colorDeviation;
    float sizeDeviation;
    float rotation;
};

class ParticleEmitter {
public:
    ParticleEmitter(unsigned int particleCount) {};
    ParticleEmitter() {};
    ~ParticleEmitter() {};
    
    void Update(float elapsed);
    void Render();
    void Trigger();
    
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 velocityDeviation;
    glm::vec3 gravity;
    
    glm::vec4 startColor;
    glm::vec4 endColor;
    glm::vec4 colorDeviation;
    
    float startSize;
    float endSize;
    float sizeDeviation;
    
    float maxLifetime;
    bool isVisible = false;
    std::vector<Particle> particles;
};

void ParticleEmitter::Render() {
    if (isVisible) {
        std::vector<float> vertices;
        std::vector<float> particleColors;
        for(int i=0; i < particles.size(); i++) {
            float m = (particles[i].lifetime/maxLifetime);
            float size = lerp(startSize, endSize, m) + particles[i].sizeDeviation;
            float cosTheta = cosf(particles[i].rotation);
            float sinTheta = sinf(particles[i].rotation);
            float TL_x = cosTheta * -size - sinTheta * size;
            float TL_y = sinTheta * -size + cosTheta * size;
            float BL_x = cosTheta * -size - sinTheta * -size;
            float BL_y = sinTheta * -size + cosTheta * -size;
            float BR_x = cosTheta * size - sinTheta * -size;
            float BR_y = sinTheta * size + cosTheta * -size;
            float TR_x = cosTheta * size - sinTheta * size;
            float TR_y = sinTheta * size + cosTheta * size;
            vertices.insert(vertices.end(), {
                particles[i].position.x + TL_x, particles[i].position.y + TL_y,
                particles[i].position.x + BL_x, particles[i].position.y + BL_y,
                particles[i].position.x + TR_x, particles[i].position.y + TR_y,
                particles[i].position.x + TR_x, particles[i].position.y + TR_y,
                particles[i].position.x + BL_x, particles[i].position.y + BL_y,
                particles[i].position.x + BR_x, particles[i].position.y + BR_y
            });
            for(int j=0; j < 6; j++) {
                particleColors.push_back(lerp(startColor.r, endColor.r, m) + particles[i].colorDeviation.r);
                particleColors.push_back(lerp(startColor.g, endColor.g, m) + particles[i].colorDeviation.g);
                particleColors.push_back(lerp(startColor.b, endColor.b, m) + particles[i].colorDeviation.b);
                particleColors.push_back(lerp(startColor.a, endColor.a, m) + particles[i].colorDeviation.a);
            }
            particles[i].rotation += 0.1;
        }
        glUseProgram(programNonTextured.programID);
        glVertexAttribPointer(programNonTextured.positionAttribute, 2, GL_FLOAT, false, 0, vertices.data());
        glEnableVertexAttribArray(programNonTextured.positionAttribute);
        GLuint colorAttribute = glGetAttribLocation(programNonTextured.programID, "color");
        glVertexAttribPointer(colorAttribute, 4, GL_FLOAT, false, 0, particleColors.data());
        glEnableVertexAttribArray(colorAttribute);
        modelMatrix = glm::mat4(1.0f);
        programNonTextured.SetModelMatrix(modelMatrix);
        glDrawArrays(GL_TRIANGLES, 0, vertices.size()/2);
        glUseProgram(programTextured.programID);
    }
}

void ParticleEmitter::Update(float elapsed) {
    for (Particle &particle : particles) {
        particle.velocity.x += gravity.x * elapsed;
        particle.velocity.y += gravity.y * elapsed;
        particle.position.x += particle.velocity.x * elapsed;
        particle.position.y += particle.velocity.y * elapsed;
        if (particle.lifetime > maxLifetime) {
            isVisible = false;
        }
        particle.lifetime += elapsed;
    }
}

void ParticleEmitter::Trigger() {
    isVisible = true;
    for (Particle &particle : particles) {
        particle.position = position;
        particle.lifetime = 0;
        particle.velocity.x = velocity.x + RandomFloat(-velocityDeviation.x, velocityDeviation.x);
        particle.velocity.y = velocity.y + RandomFloat(-velocityDeviation.y, velocityDeviation.y);
        particle.colorDeviation.r = (-colorDeviation.r * 0.5) + (colorDeviation.r * ((float)rand() / (float)RAND_MAX));
        particle.colorDeviation.g = (-colorDeviation.g * 0.5) + (colorDeviation.g * ((float)rand() / (float)RAND_MAX));
        particle.colorDeviation.b = (-colorDeviation.b * 0.5) + (colorDeviation.b * ((float)rand() / (float)RAND_MAX));
        particle.colorDeviation.a = (-colorDeviation.a * 0.5) + (colorDeviation.a * ((float)rand() / (float)RAND_MAX));
    }
}

//text stuff

void DrawText(ShaderProgram &program, int fontTexture, std::string text, float size, float spacing, glm::vec3 position) {
    float character_size = 1.0/16.0f;
    std::vector<float> vertexData;
    std::vector<float> texCoordData;
    for(int i=0; i < text.size(); i++) {
        int spriteIndex = (int)text[i];
        float texture_x = (float)(spriteIndex % 16) / 16.0f;
        float texture_y = (float)(spriteIndex / 16) / 16.0f;
        vertexData.insert(vertexData.end(), {
            ((size+spacing) * i) + (-0.5f * size), 0.5f * size,
            ((size+spacing) * i) + (-0.5f * size), -0.5f * size,
            ((size+spacing) * i) + (0.5f * size), 0.5f * size,
            ((size+spacing) * i) + (0.5f * size), -0.5f * size,
            ((size+spacing) * i) + (0.5f * size), 0.5f * size,
            ((size+spacing) * i) + (-0.5f * size), -0.5f * size,
        });
        texCoordData.insert(texCoordData.end(), {
            texture_x, texture_y,
            texture_x, texture_y + character_size,
            texture_x + character_size, texture_y,
            texture_x + character_size, texture_y + character_size,
            texture_x + character_size, texture_y,
            texture_x, texture_y + character_size,
        }); }
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    
    for (int i = 0; i < vertexData.size(); i++) {
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, position);
        programTextured.SetModelMatrix(modelMatrix);
        glVertexAttribPointer(programTextured.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
        glVertexAttribPointer(programTextured.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
        glDrawArrays(GL_TRIANGLES, 0, text.size() * 6.0);
    }
}

void collidesWith(Entity &first, Entity &second) {
    if ((abs(first.position.x - second.position.x) < (first.size.x/3.0f) + (second.size.x/3.0f)) &&
        (abs(first.position.y - second.position.y) < (first.size.y/3.0f) + (second.size.y/3.0f))) {
        
        //player-enemy collision
        
        if (first.entitytype == ENTITY_PLAYER && (second.entitytype == ENTITY_ENEMY || second.entitytype == ENTITY_JUMPER)) {
            mode = STATE_GAME_OVER;
        }
    }
}

//gamestate stuff

struct GameState {
    Entity player;
    std::vector<Entity> enemies;
    ParticleEmitter particleSources[2];
};

GameState state;

void Setup() {
    SDL_Init(SDL_INIT_VIDEO);
    displayWindow = SDL_CreateWindow("Pixel Jumper", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
    SDL_GL_MakeCurrent(displayWindow, context);
    
#ifdef _WINDOWS
    glewInit();
#endif
    
    glViewport(0, 0, 640, 360);

    programNonTextured.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
    programTextured.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
    
    glm::mat4 projectionMatrix = glm::mat4(1.0f);
    projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);
    programNonTextured.SetProjectionMatrix(projectionMatrix);
    programNonTextured.SetViewMatrix(viewMatrix);
    programTextured.SetProjectionMatrix(projectionMatrix);
    programTextured.SetViewMatrix(viewMatrix);
    
    glUseProgram(programTextured.programID);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    //music
    
    Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 );
    Mix_Music *music;
    music = Mix_LoadMUS("dootdoot.wav");
    Mix_PlayMusic(music, -1);
    
    //load the level
    
    level.Load(RESOURCE_FOLDER"Level1.txt");
    
    //spawn the player
    
    GLuint playerSpriteSheet = LoadTexture(RESOURCE_FOLDER"characters_3.png", true);
    state.player.spriteUniform = SheetSpriteUniform(playerSpriteSheet, 10, 8, 4, TILE_SIZE + 0.1, 0.0f, 0.05f);
    state.player.size.x = TILE_SIZE;
    state.player.size.y = TILE_SIZE;
    state.player.position.x = level.entities[0].x * TILE_SIZE;
    state.player.position.y = level.entities[0].y * -TILE_SIZE;
    state.player.entitytype = ENTITY_PLAYER;
    state.player.usesUniform = true;
    
    //setup particle emitters
    
    state.particleSources[0].gravity = {-2.0f, 0.0f, 0.0f};
    state.particleSources[0].maxLifetime = 0.5;
    state.particleSources[0].velocity = {1.0f, 0.0f, 0.0f};
    state.particleSources[0].velocityDeviation = {0.0f, 0.2f, 0.0f};
    state.particleSources[0].startColor = {1.0f, 1.0f, 1.0f, 1.0f};
    state.particleSources[0].endColor = {1.0f, 1.0f, 1.0f, 0.0f};
    state.particleSources[0].colorDeviation = {0.0f, 0.0f, 0.0f, 0.0f};
    state.particleSources[0].sizeDeviation = 0.01;
    state.particleSources[0].startSize = 0.01;
    state.particleSources[0].endSize = 0.05;
    for (int i = 0; i < 5; i++) {
        Particle p;
        p.lifetime = RandomFloat(0.0, state.particleSources[0].maxLifetime);
        p.sizeDeviation = RandomFloat(-state.particleSources[0].sizeDeviation, state.particleSources[0].sizeDeviation);
        state.particleSources[0].particles.push_back(p);
    }
    
    state.particleSources[1].gravity = {2.0f, 0.0f, 0.0f};
    state.particleSources[1].maxLifetime = 0.5;
    state.particleSources[1].velocity = {-1.0f, 0.0f, 0.0f};
    state.particleSources[1].velocityDeviation = {0.0f, 0.2f, 0.0f};
    state.particleSources[1].startColor = {1.0f, 1.0f, 1.0f, 1.0f};
    state.particleSources[1].endColor = {1.0f, 1.0f, 1.0f, 0.0f};
    state.particleSources[1].colorDeviation = {0.0f, 0.0f, 0.0f, 0.0f};
    state.particleSources[1].sizeDeviation = 0.01;
    state.particleSources[1].startSize = 0.01;
    state.particleSources[1].endSize = 0.05;
    for (int i = 0; i < 5; i++) {
        Particle p;
        p.lifetime = RandomFloat(0.0, state.particleSources[1].maxLifetime);
        p.sizeDeviation = RandomFloat(-state.particleSources[1].sizeDeviation, state.particleSources[1].sizeDeviation);
        state.particleSources[1].particles.push_back(p);
    }
    
    //set starting mode
    
    mode = STATE_MAIN_MENU;
}

void RenderGame(GameState &state, float elapsed) {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(programNonTextured.positionAttribute);
    glEnableVertexAttribArray(programTextured.positionAttribute);
    glEnableVertexAttribArray(programTextured.texCoordAttribute);
    glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
    
    //draw the map
    
    GLuint mapSprites = LoadTexture(RESOURCE_FOLDER"arne_sprites.png", true);
    std::vector<float> vertexData;
    std::vector<float> texCoordData;
    int numVertices = 0;
    for(int y=0; y < level.mapHeight; y++) {
        for(int x=0; x < level.mapWidth; x++) {
            if (level.mapData[y][x] != 0) {
                float u = (float)(((int)level.mapData[y][x]) % 16) / (float) 16;
                float v = (float)(((int)level.mapData[y][x]) / 16) / (float) 8;
                float spriteWidth = 1.0f/(float)16;
                float spriteHeight = 1.0f/(float)8;
                vertexData.insert(vertexData.end(), {
                    TILE_SIZE * x, -TILE_SIZE * y,
                    TILE_SIZE * x, (-TILE_SIZE * y)-TILE_SIZE,
                    (TILE_SIZE * x)+TILE_SIZE, (-TILE_SIZE * y)-TILE_SIZE,
                    TILE_SIZE * x, -TILE_SIZE * y,
                    (TILE_SIZE * x)+TILE_SIZE, (-TILE_SIZE * y)-TILE_SIZE,
                    (TILE_SIZE * x)+TILE_SIZE, -TILE_SIZE * y
                });
                texCoordData.insert(texCoordData.end(), {
                    u, v,
                    u, v+(spriteHeight),
                    u+spriteWidth, v+(spriteHeight),
                    u, v,
                    u+spriteWidth, v+(spriteHeight),
                    u+spriteWidth, v
                });
                numVertices += 6;
            }
        }
    }
    glBindTexture(GL_TEXTURE_2D, mapSprites);
    for (int i = 0; i < vertexData.size(); i++) {
        modelMatrix = glm::mat4(1.0f);
        programTextured.SetModelMatrix(modelMatrix);
        glVertexAttribPointer(programTextured.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
        glVertexAttribPointer(programTextured.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
        glDrawArrays(GL_TRIANGLES, 0, numVertices);
    }
    
    //draw the player
    
    state.player.Draw(elapsed);
    
    //draw enemies
    
    for (Entity &ent : state.enemies) {
        ent.Draw(elapsed);
    }
    
    //render particle emitters
    
    state.particleSources[0].Render();
    state.particleSources[1].Render();
    
    //make the camera follow the player
    
    viewMatrix = glm::mat4(1.0f);
    viewMatrix = glm::translate(viewMatrix, -state.player.position);
    programNonTextured.SetViewMatrix(viewMatrix);
    programTextured.SetViewMatrix(viewMatrix);
    
    //render level text
    
    currentTimeLevelText += elapsed;
    if (currentTimeLevelText < maxTimeLevelText) {
        GLuint fontSheet = LoadTexture(RESOURCE_FOLDER"pixel_font.png", true);
        std::string menu;
        if (levelNum == 1) {
            menu = "Level 1";
        }
        else if (levelNum == 2) {
            menu = "Level 2";
        }
        else {
            menu = "Level 3";
        }
        DrawText(programTextured, fontSheet, menu, 0.2f, 0.0f, state.player.position + glm::vec3(-0.5,0.5,0));
    }
}

void UpdateGame(GameState &state, float elapsed) {
    
    //update the player
    
    state.player.Update(elapsed);
    
    //update enemies
    
    for (Entity &ent : state.enemies) {
        ent.Update(elapsed);
    }
    
    //update particle emitters
    
    state.particleSources[0].position = state.player.position;
    state.particleSources[0].position.y -= state.player.size.y / 2.0;
    state.particleSources[0].Update(elapsed);
    
    state.particleSources[1].position = state.player.position;
    state.particleSources[1].position.y -= state.player.size.y / 2.0;
    state.particleSources[1].Update(elapsed);
    
    //trigger particle after player lands
    
    if (state.player.collidedBottom == false) {
        landed = false;
    }
    else if (state.player.collidedBottom && landed == false) {
        state.particleSources[0].Trigger();
        state.particleSources[1].Trigger();
        landed = true;
    }
    
    //check if player reached end of level
    
    if (state.player.position.x >= level.entities[1].x * TILE_SIZE) {
        if (levelNum == 1) {
            
            //setup level
            
            levelNum++;
            level.entities.clear();
            level.Load(RESOURCE_FOLDER"Level2.txt");
            
            //setup player
            
            state.player.position.x = level.entities[0].x * TILE_SIZE;
            state.player.position.y = level.entities[0].y * -TILE_SIZE;
            state.player.velocity.x = 0;
            state.player.velocity.y = 0;
            
            //setup enemies
            
            state.enemies.clear();
            
            for (int i = 0; i < 3 ; i++) {
                state.enemies.push_back(Entity());
            }
            
            GLuint enemySpriteSheet = LoadTexture(RESOURCE_FOLDER"arne_sprites.png", true);
            state.enemies[0].spriteUniform = SheetSpriteUniform(enemySpriteSheet, 80, 16, 8, TILE_SIZE, 0.0f, 0.0f);
            state.enemies[0].size.x = TILE_SIZE;
            state.enemies[0].size.y = TILE_SIZE;
            state.enemies[0].position.x = level.entities[2].x * TILE_SIZE;
            state.enemies[0].position.y = level.entities[2].y * -TILE_SIZE;
            state.enemies[0].usesUniform = true;
            state.enemies[0].entitytype = ENTITY_ENEMY;
            state.enemies[0].acceleration.x = 3.0f;
            
            state.enemies[1].spriteUniform = SheetSpriteUniform(enemySpriteSheet, 80, 16, 8, TILE_SIZE, 0.0f, 0.0f);
            state.enemies[1].size.x = TILE_SIZE;
            state.enemies[1].size.y = TILE_SIZE;
            state.enemies[1].position.x = level.entities[3].x * TILE_SIZE;
            state.enemies[1].position.y = level.entities[3].y * -TILE_SIZE;
            state.enemies[1].usesUniform = true;
            state.enemies[1].entitytype = ENTITY_ENEMY;
            state.enemies[1].acceleration.x = 3.0f;
            
            state.enemies[2].spriteUniform = SheetSpriteUniform(enemySpriteSheet, 80, 16, 8, TILE_SIZE, 0.0f, 0.0f);
            state.enemies[2].size.x = TILE_SIZE;
            state.enemies[2].size.y = TILE_SIZE;
            state.enemies[2].position.x = level.entities[4].x * TILE_SIZE;
            state.enemies[2].position.y = level.entities[4].y * -TILE_SIZE;
            state.enemies[2].usesUniform = true;
            state.enemies[2].entitytype = ENTITY_JUMPER;
            state.enemies[2].acceleration.x = 0.0f;
            state.enemies[2].jumpVelocity = 2.5f;
        }
        else if (levelNum == 2) {
            
            //setup level
            
            levelNum++;
            level.entities.clear();
            level.Load(RESOURCE_FOLDER"Level3.txt");
            
            //setup player
            
            state.player.position.x = level.entities[0].x * TILE_SIZE;
            state.player.position.y = level.entities[0].y * -TILE_SIZE;
            state.player.velocity.x = 0;
            state.player.velocity.y = 0;
            
            //setup enemies
            
            state.enemies.clear();
            for (int i = 0; i < 5 ; i++) {
                state.enemies.push_back(Entity());
            }
            
            GLuint enemySpriteSheet = LoadTexture(RESOURCE_FOLDER"arne_sprites.png", true);
            state.enemies[0].spriteUniform = SheetSpriteUniform(enemySpriteSheet, 80, 16, 8, TILE_SIZE, 0.0f, 0.0f);
            state.enemies[0].size.x = TILE_SIZE;
            state.enemies[0].size.y = TILE_SIZE;
            state.enemies[0].position.x = level.entities[2].x * TILE_SIZE;
            state.enemies[0].position.y = level.entities[2].y * -TILE_SIZE;
            state.enemies[0].usesUniform = true;
            state.enemies[0].entitytype = ENTITY_ENEMY;
            state.enemies[0].acceleration.x = 3.0f;
            
            state.enemies[1].spriteUniform = SheetSpriteUniform(enemySpriteSheet, 80, 16, 8, TILE_SIZE, 0.0f, 0.0f);
            state.enemies[1].size.x = TILE_SIZE;
            state.enemies[1].size.y = TILE_SIZE;
            state.enemies[1].position.x = level.entities[3].x * TILE_SIZE;
            state.enemies[1].position.y = level.entities[3].y * -TILE_SIZE;
            state.enemies[1].usesUniform = true;
            state.enemies[1].entitytype = ENTITY_ENEMY;
            state.enemies[1].acceleration.x = 3.0f;
            
            state.enemies[2].spriteUniform = SheetSpriteUniform(enemySpriteSheet, 80, 16, 8, TILE_SIZE, 0.0f, 0.0f);
            state.enemies[2].size.x = TILE_SIZE;
            state.enemies[2].size.y = TILE_SIZE;
            state.enemies[2].position.x = level.entities[4].x * TILE_SIZE;
            state.enemies[2].position.y = level.entities[4].y * -TILE_SIZE;
            state.enemies[2].usesUniform = true;
            state.enemies[2].entitytype = ENTITY_ENEMY;
            state.enemies[2].acceleration.x = 3.0f;
            
            state.enemies[3].spriteUniform = SheetSpriteUniform(enemySpriteSheet, 80, 16, 8, TILE_SIZE, 0.0f, 0.0f);
            state.enemies[3].size.x = TILE_SIZE;
            state.enemies[3].size.y = TILE_SIZE;
            state.enemies[3].position.x = level.entities[5].x * TILE_SIZE;
            state.enemies[3].position.y = level.entities[5].y * -TILE_SIZE;
            state.enemies[3].usesUniform = true;
            state.enemies[3].entitytype = ENTITY_ENEMY;
            state.enemies[3].acceleration.x = 3.0f;

            state.enemies[4].spriteUniform = SheetSpriteUniform(enemySpriteSheet, 80, 16, 8, TILE_SIZE, 0.0f, 0.0f);
            state.enemies[4].size.x = TILE_SIZE;
            state.enemies[4].size.y = TILE_SIZE;
            state.enemies[4].position.x = level.entities[6].x * TILE_SIZE + (TILE_SIZE/2.0f);
            state.enemies[4].position.y = level.entities[6].y * -TILE_SIZE;
            state.enemies[4].usesUniform = true;
            state.enemies[4].entitytype = ENTITY_JUMPER;
            state.enemies[4].acceleration.x = 0.0f;
            state.enemies[4].jumpVelocity = 5.0f;
        }
        else {
            mode = STATE_VICTORY;
        }
        
        //reset level text
        
        currentTimeLevelText = 0;
        
    }
    
    //check collision between player and enemies
    
    for (Entity &enemy : state.enemies) {
        collidesWith(state.player, enemy);
    }
}

void ProcessInputGame(GameState &state) {
    
    //move left and move right
    
    state.player.acceleration.x = 0.0f;
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if(keys[SDL_SCANCODE_D]) {
        state.player.acceleration.x = 5.0f;
        lookingLeft = false;
    }
    if(keys[SDL_SCANCODE_A]) {
        state.player.acceleration.x = -5.0f;
        lookingLeft = true;
    }
    
    //jump
    
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            done = true;
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode == SDL_SCANCODE_SPACE && state.player.collidedBottom) {
                state.player.velocity.y = 2.5;
                Mix_Chunk *jumpSound;
                jumpSound = Mix_LoadWAV("Jump.wav");
                Mix_VolumeChunk(jumpSound, 50);
                Mix_PlayChannel(-1, jumpSound, 0);
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                mode = STATE_PAUSE;
            }
        }
    }
}

void RenderMenu(float elapsed) {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(programNonTextured.positionAttribute);
    glEnableVertexAttribArray(programTextured.positionAttribute);
    glEnableVertexAttribArray(programTextured.texCoordAttribute);
    GLuint fontSheet = LoadTexture(RESOURCE_FOLDER"pixel_font.png", true);
    std::string menu = "PIXEL JUMPER";
    DrawText(programTextured, fontSheet, menu, 0.2f, 0.0f, glm::vec3(-1.2f, 0.8f, 0.0f));
    menu = "How to play: Reach the end of every level";
    DrawText(programTextured, fontSheet, menu, 0.07f, 0.0f, glm::vec3(-1.67f, 0.2f, 0.0f));
    menu = "Controls: Use A and D to move, SPACEBAR to jump,";
    DrawText(programTextured, fontSheet, menu, 0.07f, 0.0f, glm::vec3(-1.67f, 0.1f, 0.0f));
    menu = "ESC to pause";
    DrawText(programTextured, fontSheet, menu, 0.07f, 0.0f, glm::vec3(-0.95f, 0.0f, 0.0f));
    menu = "Click anywhere to start";
    animationElapsedPlayer += elapsed;
    if (animationElapsedPlayer > 1.0/framesPerSecondMenu) {
        DrawText(programTextured, fontSheet, menu, 0.1f, 0.0f, glm::vec3(-1.2f, -0.5f, 0.0f));
        if (animationElapsedPlayer > 2.0/framesPerSecondMenu) {
            animationElapsedPlayer = 0.0;
        }
    }
}

void ProcessInputMenu() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            done = true;
        } else if(event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == 1) {
                mode = STATE_GAME_LEVEL;
            }
        }
    }
}

void RenderGameOver(float elapsed) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    viewMatrix = glm::mat4(1.0f);
    programNonTextured.SetViewMatrix(viewMatrix);
    programTextured.SetViewMatrix(viewMatrix);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(programNonTextured.positionAttribute);
    glEnableVertexAttribArray(programTextured.positionAttribute);
    glEnableVertexAttribArray(programTextured.texCoordAttribute);
    GLuint fontSheet = LoadTexture(RESOURCE_FOLDER"pixel_font.png", true);
    std::string menu = "GAME OVER";
    DrawText(programTextured, fontSheet, menu, 0.3f, 0.0f, glm::vec3(-1.2f, 0.5f, 0.0f));
    menu = "Click anywhere to retry";
    animationElapsedPlayer += elapsed;
    if (animationElapsedPlayer > 1.0/framesPerSecondMenu) {
        DrawText(programTextured, fontSheet, menu, 0.1f, 0.0f, glm::vec3(-1.15f, -0.5f, 0.0f));
        if (animationElapsedPlayer > 2.0/framesPerSecondMenu) {
            animationElapsedPlayer = 0.0;
        }
    }
    Mix_HaltMusic();
}

void ProcessInputGameOver() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            done = true;
        } else if(event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == 1) {
                
                //reset player position and velocity
                
                state.player.position.x = level.entities[0].x * TILE_SIZE;
                state.player.position.y = level.entities[0].y * -TILE_SIZE;
                state.player.velocity.x = 0;
                state.player.velocity.y = 0;
                mode = STATE_GAME_LEVEL;
                
                //reset level text
                
                currentTimeLevelText = 0;
                
                //play music
                
                Mix_Music *music;
                music = Mix_LoadMUS("dootdoot.wav");
                Mix_PlayMusic(music, -1);
            }
        }
    }
}

void RenderVictory(float elapsed) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    viewMatrix = glm::mat4(1.0f);
    programNonTextured.SetViewMatrix(viewMatrix);
    programTextured.SetViewMatrix(viewMatrix);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(programNonTextured.positionAttribute);
    glEnableVertexAttribArray(programTextured.positionAttribute);
    glEnableVertexAttribArray(programTextured.texCoordAttribute);
    GLuint fontSheet = LoadTexture(RESOURCE_FOLDER"pixel_font.png", true);
    std::string menu = "VICTORY!";
    DrawText(programTextured, fontSheet, menu, 0.3f, 0.0f, glm::vec3(-1.1f, 0.5f, 0.0f));
    menu = "Click anywhere to quit";
    animationElapsedPlayer += elapsed;
    if (animationElapsedPlayer > 1.0/framesPerSecondMenu) {
        DrawText(programTextured, fontSheet, menu, 0.1f, 0.0f, glm::vec3(-1.15f, -0.5f, 0.0f));
        if (animationElapsedPlayer > 2.0/framesPerSecondMenu) {
            animationElapsedPlayer = 0.0;
        }
    }
}

void ProcessInputVictory() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            done = true;
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == 1) {
                done = true;
            }
        }
    }
}

void RenderPause() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    viewMatrix = glm::mat4(1.0f);
    programNonTextured.SetViewMatrix(viewMatrix);
    programTextured.SetViewMatrix(viewMatrix);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(programNonTextured.positionAttribute);
    glEnableVertexAttribArray(programTextured.positionAttribute);
    glEnableVertexAttribArray(programTextured.texCoordAttribute);
    
    //"PAUSED" text
    
    GLuint fontSheet = LoadTexture(RESOURCE_FOLDER"pixel_font.png", true);
    std::string menu = "PAUSED";
    DrawText(programTextured, fontSheet, menu, 0.3f, 0.0f, glm::vec3(-0.8f, 0.5f, 0.0f));
    
    //resume button
    
    float vertices[] = {-0.6f, -0.15f, 0.6f, 0.15f, -0.6f, 0.15f,
                        0.6f, 0.15f, -0.6f, -0.15f, 0.6f, -0.15f
    };
    std::vector<float> particleColors;
    for (int i = 0 ; i < 24 ; i++) {
        particleColors.push_back(0.5f);
    }
    glUseProgram(programNonTextured.programID);
    modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.8f, -0.25f, 0.0f));
    programNonTextured.SetModelMatrix(modelMatrix);
    glVertexAttribPointer(programNonTextured.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
    GLuint colorAttribute = glGetAttribLocation(programNonTextured.programID, "color");
    glVertexAttribPointer(colorAttribute, 4, GL_FLOAT, false, 0, particleColors.data());
    glEnableVertexAttribArray(colorAttribute);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    //quit button
    
    float vertices1[] = {-0.6f, -0.15f, 0.6f, 0.15f, -0.6f, 0.15f,
                        0.6f, 0.15f, -0.6f, -0.15f, 0.6f, -0.15f
    };
    particleColors.clear();
    for (int i = 0 ; i < 24 ; i++) {
        particleColors.push_back(0.5f);
    }
    glUseProgram(programNonTextured.programID);
    modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, glm::vec3(0.8f, -0.25f, 0.0f));
    programNonTextured.SetModelMatrix(modelMatrix);
    glVertexAttribPointer(programNonTextured.positionAttribute, 2, GL_FLOAT, false, 0, vertices1);
    glVertexAttribPointer(colorAttribute, 4, GL_FLOAT, false, 0, particleColors.data());
    glEnableVertexAttribArray(colorAttribute);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    //"Resume" text
    
    glUseProgram(programTextured.programID);
    menu = "Resume";
    DrawText(programTextured, fontSheet, menu, 0.2f, 0.0f, glm::vec3(-1.3f, -0.25f, 0.0f));
    
    //"Quit" text
    
    menu = "Quit";
    DrawText(programTextured, fontSheet, menu, 0.2f, 0.0f, glm::vec3(0.5f, -0.25f, 0.0f));
    
    Mix_HaltMusic();
}

void ProcessInputPause() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            done = true;
        } else if(event.type == SDL_MOUSEBUTTONDOWN) {
            float clickX = (((float)event.button.x / 640.0f) * 3.554f ) - 1.777f;
            float clickY = (((float)(360-event.button.y) / 360.0f) * 2.0f ) - 1.0f;
            
            //resume
            
            if (event.button.button == 1 && clickX > -1.4f && clickX < -0.2f
                                         && clickY > -0.4f && clickY < -0.1) {
                Mix_Music *music;
                music = Mix_LoadMUS("dootdoot.wav");
                Mix_PlayMusic(music, -1);
                mode = STATE_GAME_LEVEL;
            }
            
            //quit
            
            if (event.button.button == 1 && clickX < 1.4f && clickX > 0.2f
                                         && clickY > -0.4f && clickY < -0.1) {
                done = true;
            }
        }
    }
}

void Render(float elapsed) {
    switch (mode) {
        case STATE_GAME_LEVEL: {
            RenderGame(state, elapsed);
        }
            break;
        case STATE_MAIN_MENU: {
            RenderMenu(elapsed);
        }
            break;
        case STATE_GAME_OVER: {
            RenderGameOver(elapsed);
        }
            break;
        case STATE_VICTORY: {
            RenderVictory(elapsed);
        }
            break;
        case STATE_PAUSE: {
            RenderPause();
        }
            break;
    }
}

void Update(float elapsed) {
    switch (mode) {
        case STATE_GAME_LEVEL: {
            UpdateGame(state, elapsed);
        }
            break;
        case STATE_MAIN_MENU: {
        }
            break;
        case STATE_GAME_OVER: {
        }
            break;
        case STATE_VICTORY: {
        }
            break;
        case STATE_PAUSE: {
        }
            break;
    }
}

void ProcessInput() {
    switch (mode) {
        case STATE_GAME_LEVEL: {
            ProcessInputGame(state);
        }
            break;
        case STATE_MAIN_MENU: {
            ProcessInputMenu();
        }
            break;
        case STATE_GAME_OVER: {
            ProcessInputGameOver();
        }
            break;
        case STATE_VICTORY: {
            ProcessInputVictory();
        }
            break;
        case STATE_PAUSE: {
            ProcessInputPause();
        }
            break;
    }
}

void Cleanup() {
    glDisableVertexAttribArray(programNonTextured.positionAttribute);
    SDL_GL_SwapWindow(displayWindow);
}

int main(int argc, char *argv[])
{
    Setup();
    while (!done) {
        float ticks = (float)SDL_GetTicks()/1000.0f;
        float elapsed = ticks - lastFrameTicks;
        lastFrameTicks = ticks;
        ProcessInput();
        elapsed += accumulator;
        if(elapsed < FIXED_TIMESTEP) {
            accumulator = elapsed;
            continue; }
        while(elapsed >= FIXED_TIMESTEP) {
            Update(FIXED_TIMESTEP);
            elapsed -= FIXED_TIMESTEP;
        }
        accumulator = elapsed;
        Render(FIXED_TIMESTEP);
        Cleanup();
    }
    SDL_Quit();
    return 0;
}
