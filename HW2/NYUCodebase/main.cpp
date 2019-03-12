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

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define PI 3.14159265359f

SDL_Window* displayWindow;

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
    SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
    glewInit();
#endif
    
    glViewport(0, 0, 640, 360);
    
    ShaderProgram program;
    program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
    
    glm::mat4 projectionMatrix = glm::mat4(1.0f);
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);
    program.SetProjectionMatrix(projectionMatrix);
    program.SetViewMatrix(viewMatrix);
    
    glUseProgram(program.programID);
    
    float lastFrameTicks = 0.0f;
    
    //ball properties
    
    glm::vec2 positionBall;
    glm::vec2 directionBall(std::cosf(45.0f*PI/180.0f), std::sinf(45.0f*PI/180.0f));
    float ballSpeed = 1.5f;
    
    //paddle properties
    
    float positionLPaddle = 0.0f;
    float positionRPaddle = 0.0f;
    float paddleSpeed = 3.0f;
    
    SDL_Event event;
    bool done = false;
    while (!done) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }
        }
        glClear(GL_COLOR_BUFFER_BIT);
        
        glEnableVertexAttribArray(program.positionAttribute);
        
        float ticks = (float)SDL_GetTicks()/1000.0f;
        float elapsed = ticks - lastFrameTicks;
        lastFrameTicks = ticks;
        
        //player movement
        
        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        if(keys[SDL_SCANCODE_W] && positionLPaddle < 0.8f) {
            positionLPaddle += elapsed * paddleSpeed;
        }
        if(keys[SDL_SCANCODE_S] && positionLPaddle > -0.8f) {
            positionLPaddle += elapsed * -paddleSpeed;
        }
        if(keys[SDL_SCANCODE_UP] && positionRPaddle < 0.8f) {
            positionRPaddle += elapsed * paddleSpeed;
        }
        if(keys[SDL_SCANCODE_DOWN] && positionRPaddle > -0.8f) {
            positionRPaddle += elapsed * -paddleSpeed;
        }
        
        //left paddle
        
        float vertices1[] = {-0.05f, 0.2f, -0.05f, -0.2f, 0.05f, 0.2f};
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.6f, positionLPaddle, 0.0f));
        program.SetModelMatrix(modelMatrix);
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices1);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        
        float vertices2[] = {-0.05f, -0.2f, 0.05f, -0.2f, 0.05f, 0.2f};
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.6f, positionLPaddle, 0.0f));
        program.SetModelMatrix(modelMatrix);
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices2);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        
        //right paddle
        
        float vertices3[] = {-0.05f, 0.2f, -0.05f, -0.2f, 0.05f, 0.2f};
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(1.6f, positionRPaddle, 0.0f));
        program.SetModelMatrix(modelMatrix);
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices3);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        
        float vertices4[] = {-0.05f, -0.2f, 0.05f, -0.2f, 0.05f, 0.2f};
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(1.6f, positionRPaddle, 0.0f));
        program.SetModelMatrix(modelMatrix);
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices4);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        
        //ball

        positionBall.x += directionBall.x * elapsed * ballSpeed;
        positionBall.y += directionBall.y * elapsed * ballSpeed;
        
        if (positionBall.y > 0.95f) {
            directionBall.y = -std::sinf(45.0f*PI/180.0f);
        }
        
        if (positionBall.y < -0.95f) {
            directionBall.y = std::sinf(45.0f*PI/180.0f);
        }
        
        if (positionBall.x > 1.777f || positionBall.x < -1.777f) {
            positionBall.x = 0;
            positionBall.y = 0;
            directionBall.x = std::cosf(45.0f*PI/180.0f);
            directionBall.y = std::sinf(45.0f*PI/180.0f);
        }
        
        float vertices5[] = {-0.05f, 0.05f, -0.05f, -0.05f, 0.05f, 0.05f};
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(positionBall.x, positionBall.y, 0.0f));
        program.SetModelMatrix(modelMatrix);
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices5);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        
        float vertices6[] = {-0.05f, -0.05f, 0.05f, -0.05f, 0.05f, 0.05f};
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(positionBall.x, positionBall.y, 0.0f));
        program.SetModelMatrix(modelMatrix);
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices6);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        
        //collision
        
        if ((abs(1.6f - positionBall.x) < 0.1f) && (abs(positionBall.y - positionRPaddle) < 0.25f)) {
            directionBall.x = -std::cosf(45.0f*PI/180.0f);
        }
        
        if ((abs(-1.6f - positionBall.x) < 0.1f) && (abs(positionBall.y - positionLPaddle) < 0.25f)) {
            directionBall.x = std::cosf(45.0f*PI/180.0f);
        }
        
        //clean up
        
        glDisableVertexAttribArray(program.positionAttribute);
        SDL_GL_SwapWindow(displayWindow);
        
    }
    
    SDL_Quit();
    return 0;
}
