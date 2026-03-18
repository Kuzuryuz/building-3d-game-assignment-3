#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/model.h>

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct GameObject
{
    glm::vec3 position{};
    glm::vec3 rotationDeg{};
    glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
};

struct Player
{
    glm::vec3 position{};
    float yawDeg = 0.0f;
    float radius = 0.32f;
    float moveSpeed = 5.0f;
    float turnSpeed = 120.0f;
};

static const float TILE_SIZE = 2.0f;
static const float FLOOR_Y = -0.02f;
static const float PLAYER_Y = 0.80f;
static const float WALL_COLLISION_PADDING = 0.0f;

static const glm::vec3 FLOOR_SCALE(0.5f, 0.5f, 0.5f);
static const glm::vec3 WALL_SCALE(0.5f, 0.5f, 0.5f);
static const glm::vec3 WALL_CORNER_SCALE(0.5f, 0.5f, 0.5f);
static const glm::vec3 PLAYER_SCALE(0.8f);
static const glm::vec3 KEY_SCALE(0.0035f);
static const glm::vec3 DOOR_SCALE(0.8f, 0.8f, 0.8f);

static const float CAMERA_HEIGHT = 5.2f;
static const float CAMERA_DISTANCE = 5.6f;

std::vector<std::string> level1Map = {
    "2HHHHHdHHHHH1",
    "VP....V.....V",
    "V.HH1.V.HH1.V",
    "V...V.V...V.V",
    "rHH.V.3H1.V.V",
    "V...V...V.V.V",
    "V.HHuH1.V.V.V",
    "V.....V...V.V",
    "V.2HH.3HHHl.V",
    "V.V.......V.V",
    "V.3HdHHHH.V.V",
    "V...V..K..VDV",
    "3HHHuHHHHHuH4"
};

std::vector<std::string> level2Map = {
    "2HHHHHHHHHHHdHHHHH1",
    "VP..........V.....V",
    "V.HHdHHHdH1.rH1.V.V",
    "V...V...V.3HlDV.V.V",
    "V.V.V.2H4...V.V.3Hl",
    "V.V...V...V.V.V...V",
    "rH4.2HuHHH4.V.3H1.V",
    "V...V.......V...V.V",
    "V.HH4.2HHH1.V.HH4.V",
    "V.....V...V.......V",
    "V.2HHHl.VLrHHH1.V.V",
    "V.V...V.V.V...V.3Hl",
    "V.V.HH4.3HuHH.V...V",
    "V.V.............V.V",
    "V.3HHHdHHHdHHHHH4.V",
    "V.....V...V.......V",
    "rHHH1.V.HH4.HHHHHHl",
    "V..KV.V...........V",
    "V.HH4.3H1.HdHHH.V.V",
    "V.......V..V....V.V",
    "V.2HHHH.rHHuHH.HuHl",
    "V.V.....V.........V",
    "V.3HHH1.V.V.HHHdH.V",
    "V.....V...V...MV..V",
    "3HHHHHuHHHuHHHHuHH4"
};

std::vector<std::string>* currentMap = nullptr;

Player player;

glm::vec3 keyPosition(0.0f);

static const int MAX_KEYS = 3;
glm::vec3 keyPositions[MAX_KEYS];
bool keyCollected[MAX_KEYS] = { false, false, false };
int totalKeysNeeded = 1;
int keysCollected = 0;

glm::vec3 doorPosition(0.0f);
bool hasKey = false;
bool gameWon = false;
bool levelComplete = false;
int currentLevel = 1;

enum PieceType
{
    PIECE_WALL,
    PIECE_CORNER,
    PIECE_TSPLIT
};

struct LevelPiece
{
    PieceType type;
    GameObject obj;
};

std::vector<GameObject> floorTiles;
std::vector<LevelPiece> levelPieces;

float mapOriginX = 0.0f;
float mapOriginZ = 0.0f;

static glm::vec3 gridToWorld(int gx, int gz, float y)
{
    return glm::vec3(mapOriginX + gx * TILE_SIZE, y, mapOriginZ + gz * TILE_SIZE);
}

static bool isWallTile(int gx, int gz)
{
    if (!currentMap) return true;
    const auto& map = *currentMap;
    if (gz < 0 || gz >= static_cast<int>(map.size())) return true;
    if (gx < 0 || gx >= static_cast<int>(map[gz].size())) return true;

    char c = map[gz][gx];
    return c == 'H' || c == 'V' || c == '1' || c == '2' || c == '3' || c == '4'
        || c == 'l' || c == 'r' || c == 'u' || c == 'd';
}

static bool collidesWithWalls(const glm::vec3& candidatePos)
{
    float r = player.radius + WALL_COLLISION_PADDING;

    std::vector<glm::vec2> samplePoints = {
        {candidatePos.x - r, candidatePos.z - r},
        {candidatePos.x + r, candidatePos.z - r},
        {candidatePos.x - r, candidatePos.z + r},
        {candidatePos.x + r, candidatePos.z + r},
        {candidatePos.x,     candidatePos.z},
        {candidatePos.x - r, candidatePos.z},
        {candidatePos.x + r, candidatePos.z},
        {candidatePos.x,     candidatePos.z - r},
        {candidatePos.x,     candidatePos.z + r}
    };

    for (const glm::vec2& pt : samplePoints)
    {
        int gx = static_cast<int>(std::round((pt.x - mapOriginX) / TILE_SIZE));
        int gz = static_cast<int>(std::round((pt.y - mapOriginZ) / TILE_SIZE));
        if (isWallTile(gx, gz))
            return true;
    }
    return false;
}

static float distanceXZ(const glm::vec3& a, const glm::vec3& b)
{
    return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
}

static glm::mat4 buildModelMatrix(const GameObject& obj)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, obj.position);
    model = glm::rotate(model, glm::radians(obj.rotationDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(obj.rotationDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(obj.rotationDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, obj.scale);
    return model;
}

static void drawInstances(Shader& shader, Model& model, const std::vector<GameObject>& objects)
{
    for (const GameObject& obj : objects)
    {
        shader.setMat4("model", buildModelMatrix(obj));
        model.Draw(shader);
    }
}

static glm::vec3 getPlayerForward()
{
    float yaw = glm::radians(player.yawDeg);
    return glm::normalize(glm::vec3(std::sin(yaw), 0.0f, -std::cos(yaw)));
}

static void rebuildLevel()
{
    floorTiles.clear();
    levelPieces.clear();

    keysCollected = 0;
    hasKey = false;
    for (int i = 0; i < MAX_KEYS; ++i)
    {
        keyPositions[i] = glm::vec3(0.0f);
        keyCollected[i] = false;
    }

    if (currentLevel == 1)
    {
        currentMap = &level1Map;
        totalKeysNeeded = 1;
    }
    else
    {
        currentMap = &level2Map;
        totalKeysNeeded = 3;
    }

    const auto& map = *currentMap;
    int rows = static_cast<int>(map.size());
    int cols = static_cast<int>(map[0].size());

    mapOriginX = -((cols - 1) * TILE_SIZE) * 0.5f;
    mapOriginZ = -((rows - 1) * TILE_SIZE) * 0.5f;

    int keyIndex = 0;

    for (int z = 0; z < rows; ++z)
    {
        for (int x = 0; x < cols; ++x)
        {
            char cell = map[z][x];
            glm::vec3 worldFloor = gridToWorld(x, z, FLOOR_Y);
            floorTiles.push_back({ worldFloor, glm::vec3(0.0f), FLOOR_SCALE });

            if (cell == 'H')
            {
                LevelPiece p;
                p.type = PIECE_WALL;
                p.obj.position = gridToWorld(x, z, 0.0f);
                p.obj.rotationDeg = glm::vec3(0.0f, 0.0f, 0.0f);
                p.obj.scale = WALL_SCALE;
                levelPieces.push_back(p);
            }
            else if (cell == 'V')
            {
                LevelPiece p;
                p.type = PIECE_WALL;
                p.obj.position = gridToWorld(x, z, 0.0f);
                p.obj.rotationDeg = glm::vec3(0.0f, 90.0f, 0.0f);
                p.obj.scale = WALL_SCALE;
                levelPieces.push_back(p);
            }
            else if (cell == '1')
            {
                LevelPiece p;
                p.type = PIECE_CORNER;
                p.obj.position = gridToWorld(x, z, 0.0f);
                p.obj.rotationDeg = glm::vec3(0.0f, 0.0f, 0.0f);
                p.obj.scale = WALL_CORNER_SCALE;
                levelPieces.push_back(p);
            }
            else if (cell == '2')
            {
                LevelPiece p;
                p.type = PIECE_CORNER;
                p.obj.position = gridToWorld(x, z, 0.0f);
                p.obj.rotationDeg = glm::vec3(0.0f, 90.0f, 0.0f);
                p.obj.scale = WALL_CORNER_SCALE;
                levelPieces.push_back(p);
            }
            else if (cell == '3')
            {
                LevelPiece p;
                p.type = PIECE_CORNER;
                p.obj.position = gridToWorld(x, z, 0.0f);
                p.obj.rotationDeg = glm::vec3(0.0f, 180.0f, 0.0f);
                p.obj.scale = WALL_CORNER_SCALE;
                levelPieces.push_back(p);
            }
            else if (cell == '4')
            {
                LevelPiece p;
                p.type = PIECE_CORNER;
                p.obj.position = gridToWorld(x, z, 0.0f);
                p.obj.rotationDeg = glm::vec3(0.0f, 270.0f, 0.0f);
                p.obj.scale = WALL_CORNER_SCALE;
                levelPieces.push_back(p);
            }
            else if (cell == 'l')
            {
                LevelPiece p;
                p.type = PIECE_TSPLIT;
                p.obj.position = gridToWorld(x, z, 0.0f);
                p.obj.rotationDeg = glm::vec3(0.0f, -90.0f, 0.0f);
                p.obj.scale = WALL_SCALE;
                levelPieces.push_back(p);
            }
            else if (cell == 'r')
            {
                LevelPiece p;
                p.type = PIECE_TSPLIT;
                p.obj.position = gridToWorld(x, z, 0.0f);
                p.obj.rotationDeg = glm::vec3(0.0f, 90.0f, 0.0f);
                p.obj.scale = WALL_SCALE;
                levelPieces.push_back(p);
            }
            else if (cell == 'u')
            {
                LevelPiece p;
                p.type = PIECE_TSPLIT;
                p.obj.position = gridToWorld(x, z, 0.0f);
                p.obj.rotationDeg = glm::vec3(0.0f, 180.0f, 0.0f);
                p.obj.scale = WALL_SCALE;
                levelPieces.push_back(p);
            }
            else if (cell == 'd')
            {
                LevelPiece p;
                p.type = PIECE_TSPLIT;
                p.obj.position = gridToWorld(x, z, 0.0f);
                p.obj.rotationDeg = glm::vec3(0.0f, 0.0f, 0.0f);
                p.obj.scale = WALL_SCALE;
                levelPieces.push_back(p);
            }
            else if (cell == 'P')
            {
                player.position = gridToWorld(x, z, PLAYER_Y);
                player.yawDeg = 0.0f;
            }
            else if (cell == 'K')
            {
                if (currentLevel == 1)
                    keyPosition = gridToWorld(x, z, 0.45f);
                else if (keyIndex < MAX_KEYS)
                {
                    keyPositions[keyIndex] = gridToWorld(x, z, 0.45f);
                    keyIndex++;
                }
            }
            else if (cell == 'L')
            {
                if (keyIndex < MAX_KEYS)
                {
                    keyPositions[keyIndex] = gridToWorld(x, z, 0.45f);
                    keyIndex++;
                }
            }
            else if (cell == 'M')
            {
                if (keyIndex < MAX_KEYS)
                {
                    keyPositions[keyIndex] = gridToWorld(x, z, 0.45f);
                    keyIndex++;
                }
            }
            else if (cell == 'D')
            {
                doorPosition = gridToWorld(x, z, 0.0f);
            }
        }
    }
}

static void showPopup(const std::string& message)
{
#ifdef _WIN32
    MessageBoxA(NULL, message.c_str(), "Dungeon Key Escape", MB_OK | MB_ICONINFORMATION);
#endif
}

static void updateWindowTitle(GLFWwindow* window)
{
    std::ostringstream oss;
    oss << "Dungeon Key Escape | Level " << currentLevel;

    if (gameWon)
        oss << " | You escaped! Press ESC to quit";
    else if (hasKey)
        oss << " | All keys collected - Go to the exit door!";
    else if (currentLevel == 2)
        oss << " | Keys: " << keysCollected << "/" << totalKeysNeeded << " - Find all keys!";
    else
        oss << " | Find the key, then escape";

    glfwSetWindowTitle(window, oss.str().c_str());
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Dungeon Key Escape", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    stbi_set_flip_vertically_on_load(false);
    glEnable(GL_DEPTH_TEST);

    Shader shader("1.model_loading.vs", "1.model_loading.fs");

    Model floorModel(FileSystem::getPath("resources/objects/maze/floor_tile_large.obj"));
    Model wallModel(FileSystem::getPath("resources/objects/maze/wall.obj"));
    Model wallCornerModel(FileSystem::getPath("resources/objects/maze/wall_corner.obj"));
    Model wallTSplitModel(FileSystem::getPath("resources/objects/maze/wall_Tsplit.obj"));
    Model playerModel(FileSystem::getPath("resources/objects/maze/model.obj"));
    Model keyModel(FileSystem::getPath("resources/objects/maze/key.obj"));
    Model closedDoorModel(FileSystem::getPath("resources/objects/maze/gate-door.obj"));
    Model openDoorModel(FileSystem::getPath("resources/objects/maze/gate.obj"));

    currentLevel = 1;
    rebuildLevel();
    showPopup("Level 1\nFind the key and escape through the door!");
    updateWindowTitle(window);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        if (currentLevel == 1)
        {
            if (!hasKey && distanceXZ(player.position, keyPosition) < 0.75f)
            {
                hasKey = true;
                keysCollected = 1;
                showPopup("Key collected!\nGo to the exit door.");
                updateWindowTitle(window);
            }
        }
        else
        {
            for (int i = 0; i < totalKeysNeeded; ++i)
            {
                if (!keyCollected[i] && distanceXZ(player.position, keyPositions[i]) < 0.75f)
                {
                    keyCollected[i] = true;
                    keysCollected++;

                    if (keysCollected >= totalKeysNeeded)
                    {
                        hasKey = true;
                        showPopup("All 3 keys collected!\nGo to the exit door!");
                    }
                    else
                    {
                        std::ostringstream msg;
                        msg << "Key " << keysCollected << "/" << totalKeysNeeded << " collected!\nFind the remaining keys.";
                        showPopup(msg.str());
                    }
                    updateWindowTitle(window);
                }
            }
        }

        if (!gameWon && hasKey && distanceXZ(player.position, doorPosition) < 0.95f)
        {
            if (currentLevel == 1)
            {
                currentLevel = 2;
                gameWon = false;
                hasKey = false;
                levelComplete = false;
                rebuildLevel();
                showPopup("Level 1 Complete!\n\nWelcome to Level 2!\nFind all 3 keys to escape!");
                updateWindowTitle(window);
            }
            else
            {
                gameWon = true;
                showPopup("Congratulations!\nYou escaped the dungeon!\n\nPress ESC to quit.");
                updateWindowTitle(window);
            }
        }

        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        glm::vec3 playerForward = getPlayerForward();
        glm::vec3 camPos = player.position + playerForward * CAMERA_DISTANCE + glm::vec3(0.0f, CAMERA_HEIGHT, 0.0f);
        glm::vec3 camTarget = player.position + glm::vec3(0.0f, 0.8f, 0.0f);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT), 0.1f, 200.0f);
        glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0.0f, 1.0f, 0.0f));
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        drawInstances(shader, floorModel, floorTiles);

        for (const auto& piece : levelPieces)
        {
            shader.setMat4("model", buildModelMatrix(piece.obj));

            if (piece.type == PIECE_WALL)
                wallModel.Draw(shader);
            else if (piece.type == PIECE_CORNER)
                wallCornerModel.Draw(shader);
            else if (piece.type == PIECE_TSPLIT)
                wallTSplitModel.Draw(shader);
        }

        if (currentLevel == 1)
        {
            if (!hasKey)
            {
                GameObject keyObj;
                keyObj.position = keyPosition + glm::vec3(0.0f, 0.15f * std::sin(currentFrame * 3.0f), 0.0f);
                keyObj.rotationDeg = glm::vec3(0.0f, currentFrame * 90.0f, 0.0f);
                keyObj.scale = KEY_SCALE;
                shader.setMat4("model", buildModelMatrix(keyObj));
                keyModel.Draw(shader);
            }
        }
        else
        {
            for (int i = 0; i < totalKeysNeeded; ++i)
            {
                if (!keyCollected[i])
                {
                    GameObject keyObj;
                    keyObj.position = keyPositions[i] + glm::vec3(0.0f, 0.15f * std::sin(currentFrame * 3.0f + i * 1.0f), 0.0f);
                    keyObj.rotationDeg = glm::vec3(0.0f, currentFrame * 90.0f + i * 120.0f, 0.0f);
                    keyObj.scale = KEY_SCALE;
                    shader.setMat4("model", buildModelMatrix(keyObj));
                    keyModel.Draw(shader);
                }
            }
        }

        {
            GameObject doorObj;
            doorObj.position = doorPosition;
            doorObj.rotationDeg = glm::vec3(0.0f, 0.0f, 0.0f);
            doorObj.scale = DOOR_SCALE;
            shader.setMat4("model", buildModelMatrix(doorObj));

            if (hasKey)
                openDoorModel.Draw(shader);
            else
                closedDoorModel.Draw(shader);
        }

        {
            GameObject playerObj;
            playerObj.position = player.position;
            playerObj.rotationDeg = glm::vec3(0.0f, -player.yawDeg, 0.0f);
            playerObj.scale = PLAYER_SCALE;
            shader.setMat4("model", buildModelMatrix(playerObj));
            playerModel.Draw(shader);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (gameWon)
        return;

    float turnAmount = player.turnSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        player.yawDeg -= turnAmount;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        player.yawDeg += turnAmount;

    glm::vec3 forward = getPlayerForward();
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec3 movement(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        movement -= forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        movement += forward;

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        movement -= right;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        movement += right;

    if (glm::length(movement) > 0.0001f)
    {
        movement = glm::normalize(movement) * player.moveSpeed * deltaTime;

        glm::vec3 candidate = player.position;

        candidate.x += movement.x;
        if (!collidesWithWalls(candidate))
            player.position.x = candidate.x;

        candidate = player.position;
        candidate.z += movement.z;
        if (!collidesWithWalls(candidate))
            player.position.z = candidate.z;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}