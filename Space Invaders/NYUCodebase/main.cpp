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

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define PI 3.14159265359f
#define MAX_BULLETS 30

SDL_Window* displayWindow;
ShaderProgram program;
ShaderProgram program2;
float lastFrameTicks = 0.0f;
SDL_Event event;
bool done = false;
glm::mat4 modelMatrix = glm::mat4(1.0f);
int bulletIndex = 0;
enum GameMode { STATE_MAIN_MENU, STATE_GAME_LEVEL, STATE_GAME_OVER, STATE_VICTORY};
GameMode mode;

const int moveAnimation[] = {48, 49, 50};
const int numFrames = 3;
float animationElapsed = 0.0f;
float framesPerSecond = 0.5f;
float framesPerSecondMenu = 1.0f;
int currentIndex = 0;

class SheetSprite {
public:
    SheetSprite() {};
    SheetSprite(unsigned int textureID) : textureID(textureID) {};
    SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size):
    textureID(textureID), u(u), v(v), width(width), height(height), size(size) {};
    
    void Draw(ShaderProgram &program, glm::vec3 position) const;
    
    void DrawAnimated(ShaderProgram &program, int index, int spriteCountX, int spriteCountY, glm::vec3 position) const;
    
    float size;
    unsigned int textureID;
    float u;
    float v;
    float width;
    float height;
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

void SheetSprite::DrawAnimated(ShaderProgram &program, int index, int spriteCountX, int spriteCountY, glm::vec3 position) const {
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

//GLuint fontSheet = LoadTexture(RESOURCE_FOLDER"font1.png");

class Entity {
public:
    void Draw(float elapsed) const {
        if (isEnemy) {
            animationElapsed += elapsed;
            if (animationElapsed > 1.0/framesPerSecond) {
                currentIndex++;
                animationElapsed = 0.0;
                if (currentIndex > numFrames-1) {
                    currentIndex = 0;
                }
            }
            sprite.DrawAnimated(program, moveAnimation[currentIndex], 12, 8, position);
        }
        else {
            sprite.Draw(program, position);
        }
    }
    void Update(float elapsed) {
        position.x += velocity.x * elapsed;
        position.y += velocity.y * elapsed;
    }
    
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 size;
    
    SheetSprite sprite;
    
    bool isEnemy;
};

struct GameState {
    Entity player;
    Entity enemies[24];
    Entity bullets[30];
    int score;
};

GameState state;

void shootBullet(int &bulletIndex) {
    state.bullets[bulletIndex].position.x = state.player.position.x;
    state.bullets[bulletIndex].position.y = state.player.position.y + (state.player.size.x / 2.0f);
    bulletIndex++;
    if(bulletIndex > MAX_BULLETS-1) {
        bulletIndex = 0;
    }
}

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
        program2.SetModelMatrix(modelMatrix);
        glVertexAttribPointer(program2.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
        glVertexAttribPointer(program2.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
        glDrawArrays(GL_TRIANGLES, 0, text.size() * 6.0);
    }
}

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
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);
    program.SetProjectionMatrix(projectionMatrix);
    program.SetViewMatrix(viewMatrix);
    program2.SetProjectionMatrix(projectionMatrix);
    program2.SetViewMatrix(viewMatrix);
    
    glUseProgram(program2.programID);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    GLuint playerSpriteSheet = LoadTexture(RESOURCE_FOLDER"sheet.png");
    GLuint enemySpriteSheet = LoadTextureNearest(RESOURCE_FOLDER"characters_1.png");
    
    state.player.sprite = SheetSprite(playerSpriteSheet, 211.0f/1024.0f, 941.0f/1024.0f, 99.0f/1024.0f, 75.0f/1024.0f, 0.2f);
    state.player.position.y = -0.75f;
    state.player.size.x = 99.0f / 360.0f;
    state.player.size.y = 75.0f / 360.0f;
    state.player.isEnemy = false;
    
    float firstEnemyPosX = -1.4f;
    float firstEnemyPosY = 0.75f;
    int counter = 0;
    
    for (Entity &ent : state.enemies) {
        ent.sprite = SheetSprite(enemySpriteSheet);
        ent.position.y = firstEnemyPosY;
        ent.position.x = firstEnemyPosX;
        ent.size.x = 75.0f / 360.0f;
        ent.size.y = 50.0f / 360.0f;
        ent.velocity.x = 0.2f;
        ent.velocity.y = -0.05f;
        ent.isEnemy = true;
        if (counter == 7) {
            firstEnemyPosY -= 0.4f;
            firstEnemyPosX = -1.4f;
            counter = -1;
        }
        else {
            firstEnemyPosX += 0.4f;
        }
        counter++;
    }
    
    for (Entity &ent : state.bullets) {
        ent.sprite = SheetSprite(playerSpriteSheet, 856.0f/1024.0f, 421.0f/1024.0f, 9.0f/1024.0f, 54.0f/1024.0f, 0.2f);
        ent.position.x = 100.0f;
        ent.position.y = 100.0f;
        ent.size.x = 9.0f / 360.0f;
        ent.size.y = 54.0f / 360.0f;
        ent.isEnemy = false;
        ent.velocity.y = 4.0f;
    }
    
    mode = STATE_MAIN_MENU;
}

void RenderGame(const GameState &state, float elapsed) {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(program.positionAttribute);
    glEnableVertexAttribArray(program2.positionAttribute);
    glEnableVertexAttribArray(program2.texCoordAttribute);
    state.player.Draw(elapsed);
    for (const Entity &ent : state.enemies) {
        ent.Draw(elapsed);
    }
    for (const Entity &ent : state.bullets) {
        ent.Draw(elapsed);
    }
    GLuint fontSheet = LoadTextureNearest(RESOURCE_FOLDER"pixel_font.png");
    std::string score = "Score: " + std::to_string(state.score);
    DrawText(program, fontSheet, score, 0.1f, 0.0f, glm::vec3(-1.67f, 0.9f, 0.0f));
}

void UpdateGame(GameState &state, float elapsed) {
    state.player.Update(elapsed);
    for (Entity &ent : state.enemies) {
        ent.Update(elapsed);
    }
    if (state.enemies[7].position.x > 1.777f - (state.enemies[7].size.x / 2.0f)) {
        for (Entity &ent : state.enemies) {
            ent.velocity.x = -0.2f;
        }
    }
    if (state.enemies[0].position.x < -1.777f + (state.enemies[7].size.x / 2.0f)) {
        for (Entity &ent : state.enemies) {
            ent.velocity.x = 0.2f;
        }
    }
    for (Entity &ent : state.bullets) {
        ent.Update(elapsed);
    }
    for (Entity &ent : state.bullets) {
        for (Entity &ent2 : state.enemies) {
            if ((abs(ent.position.x - ent2.position.x) < (ent.size.x/2.0f) + (ent2.size.x/2.0f))
                && (abs(ent.position.y - ent2.position.y) < (ent.size.y/2.0f) + (ent2.size.y/2.0f))) {
                ent2.position.y = 1000.0f;
                ent.position.x = 100.0f;
                ent.position.y = 100.0f;
                state.score += 10;
            }
        }
    }
    for (Entity &ent : state.enemies) {
        if ((abs(ent.position.x - state.player.position.x) < (ent.size.x/2.0f) + (state.player.size.x/2.0f))
            && (abs(ent.position.y - state.player.position.y) < (ent.size.y/2.0f) + (state.player.size.y/2.0f))) {
            mode = STATE_GAME_OVER;
        }
        if (ent.position.y < -1.0f) {
            mode = STATE_GAME_OVER;
        }
    }
    if (state.score == 240) {
        mode = STATE_VICTORY;
    }
}

void ProcessInputGame(GameState &state) {
    state.player.velocity.x = 0.0f;
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    if(keys[SDL_SCANCODE_D] && state.player.position.x < 1.777f - (state.player.size.x / 2.0f)) {
        state.player.velocity.x = 2.0f;
    }
    if(keys[SDL_SCANCODE_A] && state.player.position.x > -1.777f + (state.player.size.x / 2.0f)) {
        state.player.velocity.x = -2.0f;
    }
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            done = true;
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                shootBullet(bulletIndex);
            }
        }
    }
}

void RenderMenu(float elapsed) {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(program.positionAttribute);
    glEnableVertexAttribArray(program2.positionAttribute);
    glEnableVertexAttribArray(program2.texCoordAttribute);
    GLuint fontSheet = LoadTextureNearest(RESOURCE_FOLDER"pixel_font.png");
    std::string menu = "SPACE INVADERS";
    DrawText(program, fontSheet, menu, 0.2f, 0.0f, glm::vec3(-1.3f, 0.8f, 0.0f));
    menu = "How to play: Don't let the aliens touch your ship or get through!";
    DrawText(program, fontSheet, menu, 0.05f, 0.0f, glm::vec3(-1.67f, 0.2f, 0.0f));
    menu = "Controls: Use A and D to move, SPACEBAR to shoot";
    DrawText(program, fontSheet, menu, 0.05f, 0.0f, glm::vec3(-1.67f, 0.1f, 0.0f));
    menu = "Click anywhere to start";
    animationElapsed += elapsed;
    if (animationElapsed > 1.0/framesPerSecondMenu) {
        DrawText(program, fontSheet, menu, 0.1f, 0.0f, glm::vec3(-1.2f, -0.5f, 0.0f));
        if (animationElapsed > 2.0/framesPerSecondMenu) {
            animationElapsed = 0.0;
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

void RenderGameOver() {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(program.positionAttribute);
    glEnableVertexAttribArray(program2.positionAttribute);
    glEnableVertexAttribArray(program2.texCoordAttribute);
    GLuint fontSheet = LoadTextureNearest(RESOURCE_FOLDER"pixel_font.png");
    std::string menu = "GAME OVER";
    DrawText(program, fontSheet, menu, 0.3f, 0.0f, glm::vec3(-1.2f, 0.0f, 0.0f));
}

void ProcessInputGameOver() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            done = true;
        }
    }
}

void RenderVictory() {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableVertexAttribArray(program.positionAttribute);
    glEnableVertexAttribArray(program2.positionAttribute);
    glEnableVertexAttribArray(program2.texCoordAttribute);
    GLuint fontSheet = LoadTextureNearest(RESOURCE_FOLDER"pixel_font.png");
    std::string menu = "VICTORY!";
    DrawText(program, fontSheet, menu, 0.3f, 0.0f, glm::vec3(-1.1f, 0.0f, 0.0f));
    menu = "You saved humanity!";
    DrawText(program, fontSheet, menu, 0.15f, 0.0f, glm::vec3(-1.4f, -0.25f, 0.0f));
}

void ProcessInputVictory() {
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
            done = true;
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
            RenderGameOver();
        }
        break;
        case STATE_VICTORY: {
            RenderVictory();
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
        ProcessInput();
        Update(elapsed);
        Render(elapsed);
        Cleanup();
    }
    SDL_Quit();
    return 0;
}
