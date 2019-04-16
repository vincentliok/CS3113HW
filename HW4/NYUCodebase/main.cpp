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

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define PI 3.14159265359f
#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6
#define TILE_SIZE 0.2f

SDL_Window* displayWindow;
ShaderProgram program;
ShaderProgram program2;
float lastFrameTicks = 0.0f;
SDL_Event event;
bool done = false;
glm::mat4 modelMatrix = glm::mat4(1.0f);
glm::mat4 viewMatrix = glm::mat4(1.0f);
float accumulator = 0.0f;
glm::vec3 friction = glm::vec3(4.0f, 0.0f, 0.0f);
glm::vec3 gravity = glm::vec3(0.0f, -4.0f, 0.0f);

class SheetSprite {
public:
    SheetSprite() {};
    SheetSprite(unsigned int textureID) : textureID(textureID) {};
    SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size):
    textureID(textureID), u(u), v(v), width(width), height(height), size(size) {};
    SheetSprite(unsigned int textureID, int index, int spriteCountX, int spriteCountY):
    textureID(textureID), index(index), spriteCountX(spriteCountX), spriteCountY(spriteCountY) {};
    
    void Draw(ShaderProgram &program, glm::vec3 position) const;
    void DrawUniform(ShaderProgram &program, glm::vec3 position) const;
    
    float size;
    unsigned int textureID;
    float u;
    float v;
    float width;
    float height;
    int index;
    int spriteCountX;
    int spriteCountY;
};

void SheetSprite::Draw(ShaderProgram &program, glm::vec3 position) const {
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
    
    modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    program2.SetModelMatrix(modelMatrix);
    glVertexAttribPointer(program2.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
    glVertexAttribPointer(program2.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void SheetSprite::DrawUniform(ShaderProgram &program, glm::vec3 position) const {
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
    float vertices[] = {-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f,  -0.5f,
        -0.5f, 0.5f, -0.5f};
    
    modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    modelMatrix = glm::scale(modelMatrix, glm::vec3(0.25f, 0.25f, 1.0f));
    program2.SetModelMatrix(modelMatrix);
    glVertexAttribPointer(program2.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
    glVertexAttribPointer(program2.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

GLuint LoadTexture(const char *filePath) {
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(image);
    return retTexture;
}

GLuint LoadTextureNearest(const char *filePath) {
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    stbi_image_free(image);
    return retTexture;
}

float lerp(float v0, float v1, float t) {
    return (1.0-t)*v0 + t*v1;
}

void worldToTileCoordinates(float worldX, float worldY, int &gridX, int &gridY) {
    gridX = (int)(worldX / TILE_SIZE);
    gridY = (int)(worldY / -TILE_SIZE);
}

FlareMap level;

class Entity {
public:
    void Draw(float elapsed) const {
        sprite.DrawUniform(program, position);
    }
    void Update(float elapsed) {
        velocity.x = lerp(velocity.x, 0.0f, elapsed * friction.x);
        velocity.y = lerp(velocity.y, 0.0f, elapsed * friction.y);
        velocity.x += acceleration.x * elapsed;
        velocity.y += gravity.y * elapsed;
        position.x += velocity.x * elapsed;
        position.y += velocity.y * elapsed;
        
        int gridposx = 0;
        int gridposy = 0;
        collidedBottom = false;
        worldToTileCoordinates(position.x, position.y - (size.y/2.0), gridposx, gridposy);
        if (((gridposx > 0 && gridposy > 0) && (gridposx < 50 && gridposy < 50)) && level.mapData[gridposy][gridposx] == 3) {
            position.y += fabs((-TILE_SIZE * gridposy)-(position.y - (size.y/2.0))) + 0.00001;
            velocity.y = 0;
            collidedBottom = true;
        }
        worldToTileCoordinates(position.x, position.y + (size.y/6.0), gridposx, gridposy);
        if (((gridposx > 0 && gridposy > 0) && (gridposx < 50 && gridposy < 50)) && level.mapData[gridposy][gridposx] == 3) {
            position.y -= fabs(((-TILE_SIZE * gridposy)-TILE_SIZE)-(position.y + (size.y/6.0))) - 0.00001;
            velocity.y = 0;
        }
        worldToTileCoordinates(position.x - (size.x/2.0), position.y, gridposx, gridposy);
        if (((gridposx > 0 && gridposy > 0) && (gridposx < 50 && gridposy < 50)) && level.mapData[gridposy][gridposx] == 3) {
            position.x += fabs(((TILE_SIZE * gridposx)+TILE_SIZE)-(position.x - (size.x/2.0))) + 0.00001;
            velocity.x = 0;
        }
        worldToTileCoordinates(position.x + (size.x/2.0), position.y, gridposx, gridposy);
        if (((gridposx > 0 && gridposy > 0) && (gridposx < 50 && gridposy < 50)) && level.mapData[gridposy][gridposx] == 3) {
            position.x -= fabs(((TILE_SIZE * gridposx))-(position.x + (size.x/2.0))) + 0.00001;
            velocity.x = 0;
        }
    }
    
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    glm::vec3 size;
    
    SheetSprite sprite;
    
    bool isEnemy;
    bool collidedTop;
    bool collidedBottom;
    bool collidedLeft;
    bool collidedRight;
};

struct GameState {
    Entity player;
    Entity item;
};

GameState state;

void Setup() {
    SDL_Init(SDL_INIT_VIDEO);
    displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
    SDL_GL_MakeCurrent(displayWindow, context);
    
#ifdef _WINDOWS
    glewInit();
#endif
    
    glViewport(0, 0, 640, 360);

    program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
    program2.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
    
    glm::mat4 projectionMatrix = glm::mat4(1.0f);
    projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);
    program.SetProjectionMatrix(projectionMatrix);
    program.SetViewMatrix(viewMatrix);
    program2.SetProjectionMatrix(projectionMatrix);
    program2.SetViewMatrix(viewMatrix);
    
    glUseProgram(program2.programID);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    GLuint playerSpriteSheet = LoadTextureNearest(RESOURCE_FOLDER"characters_3.png");
    state.player.sprite = SheetSprite(playerSpriteSheet, 10, 8, 4);
    state.player.size.x = 0.1;
    state.player.size.y = 0.25;
    state.player.position.x = 4.0;
    state.player.position.y = -3.0;
    level.Load(RESOURCE_FOLDER"PlatformerLevel.txt");
    
    GLuint itemSpriteSheet = LoadTextureNearest(RESOURCE_FOLDER"arne_sprites.png");
    state.item.sprite = SheetSprite(itemSpriteSheet, 86, 16, 8);
    state.item.size.x = 0.1;
    state.item.size.y = 0.15;
    state.item.position.x = level.entities[0].x * TILE_SIZE;
    //std::cout << level.entities[0].x * TILE_SIZE << std::endl;
    state.item.position.y = level.entities[0].y * -TILE_SIZE + 5;
    //std::cout << level.entities[0].y * -TILE_SIZE << std::endl;
}

void RenderGame(const GameState &state, float elapsed) {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(program.positionAttribute);
    glEnableVertexAttribArray(program2.positionAttribute);
    glEnableVertexAttribArray(program2.texCoordAttribute);
    state.player.Draw(elapsed);
    state.item.Draw(elapsed);
    
    GLuint mapSprites = LoadTextureNearest(RESOURCE_FOLDER"arne_sprites.png");
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
        program2.SetModelMatrix(modelMatrix);
        glVertexAttribPointer(program2.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
        glVertexAttribPointer(program2.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
        glDrawArrays(GL_TRIANGLES, 0, numVertices);
    }
    
    viewMatrix = glm::mat4(1.0f);
    viewMatrix = glm::translate(viewMatrix, -state.player.position);
    program.SetViewMatrix(viewMatrix);
    program2.SetViewMatrix(viewMatrix);
}

void UpdateGame(GameState &state, float elapsed) {
    state.player.Update(elapsed);
    state.item.Update(elapsed);
    
    if ((abs(state.item.position.x - state.player.position.x) < (state.player.size.x/2.0f) + (state.item.size.x/2.0f))
        && (abs(state.item.position.y - state.player.position.y) < (state.item.size.y/2.0f) + (state.player.size.y/2.0f))) {
        state.item.position.x = 1000.0f;
    }
}

void ProcessInputGame(GameState &state) {
    state.player.acceleration.x = 0.0f;
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if(keys[SDL_SCANCODE_D]) {
        state.player.acceleration.x = 5.0f;
    }
    if(keys[SDL_SCANCODE_A]) {
        state.player.acceleration.x = -5.0f;
    }
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            done = true;
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode == SDL_SCANCODE_SPACE && state.player.collidedBottom) {
                state.player.velocity.y = 3;
            }
        }
        
    }
}

void Cleanup() {
    glDisableVertexAttribArray(program.positionAttribute);
    SDL_GL_SwapWindow(displayWindow);
}

int main(int argc, char *argv[])
{
    Setup();
    while (!done) {
        float ticks = (float)SDL_GetTicks()/1000.0f;
        float elapsed = ticks - lastFrameTicks;
        lastFrameTicks = ticks;
        ProcessInputGame(state);
        elapsed += accumulator;
        if(elapsed < FIXED_TIMESTEP) {
            accumulator = elapsed;
            continue; }
        while(elapsed >= FIXED_TIMESTEP) {
            UpdateGame(state, FIXED_TIMESTEP);
            elapsed -= FIXED_TIMESTEP;
        }
        accumulator = elapsed;
        RenderGame(state, FIXED_TIMESTEP);
        Cleanup();
    }
    SDL_Quit();
    return 0;
}
