#include <graphics.h>
#include <conio.h>
#include <Windows.h>
#include <iostream>
#include <cmath>
#include <ctime>
#include <vector>    
#include <algorithm>
#include <random>
#include <string>

using namespace std;
struct Square {
    POINT center;       
    POINT spawnPoint;   
    POINT targetPos;    
    float moveRadius;   
    float moveSpeed;    
    float viewAngle;    
    float viewRadius;   
    float faceAngle;    
    bool isChasing;     
    int size;
    COLORREF color;
    enum State { 
        PATROLLING,   
        RETURNING,    
        PAUSED       
    } state;
    float rotationPauseTimer = 0;
    float pauseTimer;   
    float returnProgress;
    COLORREF baseColor;  
};
const int WIDTH = 1000;
const int HEIGHT = 800;
const int RADIUS = 20;
const float MOVE_SPEED = 6.0f;  
const int SQUARE_SIZE = 40;         

const float ENEMY_MOVE_RADIUS = 200.0f;  
const float ENEMY_SPEED = 2.0f;         
const float ENEMY_VIEW_RADIUS = 400.0f; 
const float ENEMY_VIEW_ANGLE = 60.0f;   
const float CHASE_SPEED = 4.0f;    
const float RETURN_DURATION = 2.0f; 
const float MIN_PAUSE_TIME = 0.5f; 
const float MAX_PAUSE_TIME = 1.5f;
int health = 5;
DWORD lastFrameTime = 0;          
float currentFPS = 0.0f;
vector<Square> squares;             
DWORD lastSpawnTime = 0;
int SPAWN_INTERVAL = 2000;  //生成时间 
int sumScore = 0; 
float playerX = WIDTH/2.0f;
float playerY = HEIGHT/2.0f;
float distance(POINT a, POINT b) 
{
    return sqrtf(powf(a.x - b.x, 2) + powf(a.y - b.y, 2));
}
float toDegrees(float radians) { return radians * 180.0f / 3.1415926f; }
float toRadians(float degrees) { return degrees * 3.1415926f / 180.0f; }
bool w = false, a = false, s = false, d = false;
float randomBetween(float min, float max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(min, max);
    return dis(gen);
}
void initGame()
{
    initgraph(WIDTH, HEIGHT);
    setbkcolor(WHITE);
    cleardevice();
}
bool isPointInPolygon(POINT point, const POINT* polygon, int numPoints) {
    bool inside = false;
    for (int i = 0, j = numPoints-1; i < numPoints; j = i++) {
        if (((polygon[i].y > point.y) != (polygon[j].y > point.y)) &&
            (point.x < (polygon[j].x - polygon[i].x) * (point.y - polygon[i].y) 
                      / (polygon[j].y - polygon[i].y) + polygon[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}
void generateNewPatrolTarget(Square& enemy) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    float angle = toRadians(gen() % 360);
    float radius = ENEMY_MOVE_RADIUS * sqrt(dis(gen));

    enemy.targetPos.x = enemy.spawnPoint.x + radius * cos(angle);
    enemy.targetPos.y = enemy.spawnPoint.y + radius * sin(angle);

    // 边界约束
    enemy.targetPos.x = fmax(SQUARE_SIZE/2, fmin(WIDTH - SQUARE_SIZE/2, (LONG)enemy.targetPos.x));
    enemy.targetPos.y = fmax(SQUARE_SIZE/2, fmin(HEIGHT - SQUARE_SIZE/2, (LONG)enemy.targetPos.y));

    // 强制移动半径约束
    POINT vecToSpawn = { enemy.targetPos.x - enemy.spawnPoint.x, 
                        enemy.targetPos.y - enemy.spawnPoint.y };
    float distToSpawn = sqrt(vecToSpawn.x*vecToSpawn.x + vecToSpawn.y*vecToSpawn.y);
    
    if (distToSpawn > ENEMY_MOVE_RADIUS) {
        float scale = ENEMY_MOVE_RADIUS / distToSpawn;
        enemy.targetPos.x = enemy.spawnPoint.x + vecToSpawn.x * scale;
        enemy.targetPos.y = enemy.spawnPoint.y + vecToSpawn.y * scale;
    }

    POINT vecToTarget = { enemy.targetPos.x - enemy.center.x,
                         enemy.targetPos.y - enemy.center.y };
    enemy.faceAngle = toDegrees(atan2f(vecToTarget.y, vecToTarget.x));
}
void drawPlayer()
{
    setfillcolor(BLUE);
    setlinecolor(BLACK);
	fillcircle(static_cast<int>(playerX), static_cast<int>(playerY), RADIUS);
}
void spawnRandomSquare() {
    Square newSquare;
    
    newSquare.spawnPoint.x = rand() % (WIDTH - SQUARE_SIZE) + SQUARE_SIZE/2;
    newSquare.spawnPoint.y = rand() % (HEIGHT - SQUARE_SIZE) + SQUARE_SIZE/2;
    newSquare.center = newSquare.spawnPoint; 
    newSquare.moveRadius = ENEMY_MOVE_RADIUS;
    newSquare.moveSpeed = ENEMY_SPEED;
    newSquare.viewRadius = ENEMY_VIEW_RADIUS;
    newSquare.viewAngle = ENEMY_VIEW_ANGLE;
    newSquare.faceAngle = 0.0f;
    newSquare.isChasing = false;
    
    newSquare.size = SQUARE_SIZE;
    newSquare.color = RED;
    
    squares.push_back(newSquare);
}
bool checkCollision(POINT circlePos, int radius, const POINT* rectPoints, int size) {
    
    POINT rectCenter = {
        (rectPoints[0].x + rectPoints[1].x + rectPoints[2].x + rectPoints[3].x) / 4,
        (rectPoints[0].y + rectPoints[1].y + rectPoints[2].y + rectPoints[3].y) / 4
    };

    POINT edge = {rectPoints[1].x - rectPoints[0].x, rectPoints[1].y - rectPoints[0].y};
    float angle = atan2f(edge.y, edge.x);
    float cosTheta = cosf(-angle);
    float sinTheta = sinf(-angle);

    float dx = circlePos.x - rectCenter.x;
    float dy = circlePos.y - rectCenter.y;
    float localX = dx * cosTheta - dy * sinTheta;
    float localY = dx * sinTheta + dy * cosTheta;

    float halfSize = size / 2.0f;

    float closestX = fmaxf(-halfSize, fminf(localX, halfSize));
    float closestY = fmaxf(-halfSize, fminf(localY, halfSize));

    float distanceX = localX - closestX;
    float distanceY = localY - closestY;
    float distanceSq = distanceX*distanceX + distanceY*distanceY;

    return distanceSq < (radius * radius);
}
void updateEnemies(float deltaTime) {
    POINT playerPos = { static_cast<int>(playerX), static_cast<int>(playerY) };

    for (auto it = squares.begin(); it != squares.end();) {
        Square& enemy = *it;
        bool wasChasing = enemy.isChasing;
        
        POINT vecToPlayer = { 
            playerPos.x - enemy.center.x, 
            playerPos.y - enemy.center.y 
        };
        float distToPlayer = distance(enemy.center, playerPos);
        float angleToPlayer = toDegrees(atan2f(vecToPlayer.y, vecToPlayer.x));
        
        float angleDiff = fabs(angleToPlayer - enemy.faceAngle);
        angleDiff = fmin(angleDiff, 360 - angleDiff);
        
        enemy.isChasing = (distToPlayer < enemy.viewRadius) && 
                         (angleDiff < enemy.viewAngle / 2);

        //追逐状态
        if (enemy.isChasing) {
            
            enemy.faceAngle = angleToPlayer;
            
            if (enemy.state == Square::PAUSED) {
                enemy.state = Square::PATROLLING;
                enemy.rotationPauseTimer = 0;
            }
        }

        if (wasChasing && !enemy.isChasing) {
            enemy.state = Square::RETURNING;
            enemy.targetPos = enemy.spawnPoint;
            
            // 计算返回路径方向
            POINT vecToTarget = { 
                enemy.targetPos.x - enemy.center.x,
                enemy.targetPos.y - enemy.center.y 
            };
            enemy.faceAngle = toDegrees(atan2f(vecToTarget.y, vecToTarget.x));
        }

        //碰撞检测 
        POINT points[4];
        float angle = toRadians(enemy.faceAngle);
        const POINT basePoints[4] = {
            { -enemy.size/2, -enemy.size/2 },
            {  enemy.size/2, -enemy.size/2 },
            {  enemy.size/2,  enemy.size/2 },
            { -enemy.size/2,  enemy.size/2 }
        };
        
        for (int i = 0; i < 4; i++) {
            float x = basePoints[i].x;
            float y = basePoints[i].y;
            points[i].x = enemy.center.x + x*cos(angle) - y*sin(angle);
            points[i].y = enemy.center.y + x*sin(angle) + y*cos(angle);
        }

        if (checkCollision(playerPos, RADIUS, points, enemy.size)) {
            health = max(0, health-1);
            it = squares.erase(it);
            continue;
        }

        //非追逐状态
        if (!enemy.isChasing) {
            switch (enemy.state) {
                case Square::PATROLLING:
                    if (distance(enemy.center, enemy.targetPos) < 5) {
                        enemy.state = Square::PAUSED;
                        enemy.rotationPauseTimer = randomBetween(1.0f, 2.0f);
                    }
                    break;
                    
                case Square::PAUSED:
                    enemy.rotationPauseTimer -= deltaTime;
                    if (enemy.rotationPauseTimer <= 0) {
                        generateNewPatrolTarget(enemy);
                        enemy.state = Square::PATROLLING;
                    }
                    break;
                    
                case Square::RETURNING:
                    
                    if (distance(enemy.center, enemy.targetPos) < 5) {
                        enemy.state = Square::PAUSED;
                        enemy.rotationPauseTimer = randomBetween(1.0f, 2.0f);
                    }
                    break;
            }
        }

        POINT moveVec = {0, 0};
        if (enemy.isChasing) {
            moveVec = vecToPlayer; 
        } else {
            if (enemy.state == Square::PATROLLING || enemy.state == Square::RETURNING) {
                moveVec.x = enemy.targetPos.x - enemy.center.x;
                moveVec.y = enemy.targetPos.y - enemy.center.y;
            }
        }

        float moveLength = sqrtf(moveVec.x*moveVec.x + moveVec.y*moveVec.y);
        if (moveLength > 0 && enemy.state != Square::PAUSED) {
            float speed = enemy.isChasing ? CHASE_SPEED : enemy.moveSpeed;
            enemy.center.x += (moveVec.x / moveLength) * speed;
            enemy.center.y += (moveVec.y / moveLength) * speed;
        }

        if (!enemy.isChasing) {
            POINT currentVec = { 
                enemy.center.x - enemy.spawnPoint.x,
                enemy.center.y - enemy.spawnPoint.y 
            };
            float currentDist = sqrt(currentVec.x*currentVec.x + currentVec.y*currentVec.y);
            
            if (currentDist > ENEMY_MOVE_RADIUS) {
                float scale = ENEMY_MOVE_RADIUS / currentDist;
                enemy.center.x = enemy.spawnPoint.x + currentVec.x * scale;
                enemy.center.y = enemy.spawnPoint.y + currentVec.y * scale;
                enemy.targetPos = enemy.center;
            }
        }

        ++it;
    }
}
void drawEnemy() {
    for (auto& enemy : squares) {
        setfillcolor(enemy.color);
        setlinecolor(BLACK);
        
        POINT points[4];
        float angle = toRadians(enemy.faceAngle);
        
        POINT basePoints[4] = {
            { -enemy.size/2, -enemy.size/2 },
            {  enemy.size/2, -enemy.size/2 },
            {  enemy.size/2,  enemy.size/2 },
            { -enemy.size/2,  enemy.size/2 }
        };
        
        for (int i = 0; i < 4; i++) {
            float x = basePoints[i].x;
            float y = basePoints[i].y;
            points[i].x = enemy.center.x + x*cos(angle) - y*sin(angle);
            points[i].y = enemy.center.y + x*sin(angle) + y*cos(angle);
        }
        
        fillpolygon(points, 4);
        
        
        
    }
}
void drawUI() {
    // 绘制FPS
    settextcolor(BLACK);
    setbkmode(TRANSPARENT);
    char fpsText[32];
    sprintf(fpsText, "FPS: %.1f", currentFPS);
    outtextxy(10, 10, fpsText);
	
    // 绘制生命值
    for (int i = 0; i < health; i++) {
        setfillcolor(LIGHTRED);
        fillcircle(WIDTH - 40 - i*40, 30, 10);
    }
    //绘制分数
    char scoretext[32];
	sprintf(scoretext,"分数: %d",sumScore);
	outtextxy(920,45,scoretext); 
	
	//绘制生成时间 
	char spawntime[32];
	sprintf(spawntime,"生成间隔: %d 毫秒",SPAWN_INTERVAL);
	outtextxy(850,65,spawntime);
}
void processInput(ExMessage& msg)
{
    switch(msg.message)
    {
    case WM_KEYDOWN:
        switch(msg.vkcode)
        {
        case 'W': w = true; break;
        case 'A': a = true; break;
        case 'S': s = true; break;
        case 'D': d = true; break;
        case VK_ESCAPE: closegraph(); exit(0); break;
        }
        break;
    
    case WM_KEYUP:
        switch(msg.vkcode)
        {
        case 'W': w = false; break;
        case 'A': a = false; break;
        case 'S': s = false; break;
        case 'D': d = false; break;
        }
        break;
    case WM_LBUTTONDOWN: {
    POINT clickPos = {msg.x, msg.y};
    
    for (auto it = squares.rbegin(); it != squares.rend();) {
        // 生成当前敌人的多边形顶点
        POINT points[4];
        float angle = toRadians(it->faceAngle);
        POINT basePoints[4] = {
            { -it->size/2, -it->size/2 },
            {  it->size/2, -it->size/2 },
            {  it->size/2,  it->size/2 },
            { -it->size/2,  it->size/2 }
        };
        
        for (int i = 0; i < 4; i++) {
            float x = basePoints[i].x;
            float y = basePoints[i].y;
            points[i].x = it->center.x + x*cos(angle) - y*sin(angle);
            points[i].y = it->center.y + x*sin(angle) + y*cos(angle);
        }
        
        if (isPointInPolygon(clickPos, points, 4)) {
            // 正向迭代器
            auto forward_it = it.base();
            forward_it--;
            it = std::vector<Square>::reverse_iterator(
                squares.erase(forward_it)
            );
            sumScore+=100;
            if(SPAWN_INTERVAL >=50)
            	SPAWN_INTERVAL-=50;
            else
            	break;
            
            break;  
        } else {
            ++it;
        }
    }
    break;
}
    
    }
}

void updatePlayer()
{
    float dx = 0.0f, dy = 0.0f;
    if(w) dy -= 1.0f;
    if(s) dy += 1.0f;
    if(a) dx -= 1.0f;
    if(d) dx += 1.0f;

    float length = sqrtf(dx*dx + dy*dy);
    if(length > 0.0f)
    {
        dx /= length;
        dy /= length;
    }

    playerX += dx * MOVE_SPEED;
    playerY += dy * MOVE_SPEED;

    playerX = fmaxf(RADIUS, fminf(WIDTH - RADIUS, playerX));
    playerY = fmaxf(RADIUS, fminf(HEIGHT - RADIUS, playerY));
}

int main()
{
    initGame();
    ExMessage msg;
	lastSpawnTime = GetTickCount();
	DWORD lastFrameTime = GetTickCount();
    while(true)
    {
        
		DWORD currentTime = GetTickCount();
        float deltaTime = (currentTime - lastFrameTime) / 1000.0f;
        lastFrameTime = currentTime;
		while(peekmessage(&msg, EX_KEY | EX_MOUSE | EX_WINDOW))
        {
            processInput(msg);
        }
        
        if (currentTime - lastSpawnTime > SPAWN_INTERVAL) {
            spawnRandomSquare();
            lastSpawnTime = currentTime;
        }
        updatePlayer();
        updateEnemies(deltaTime);
        BeginBatchDraw();
        cleardevice();
        drawEnemy();
        drawPlayer();
        drawUI();
        EndBatchDraw();
        FlushBatchDraw();
		Sleep(1);
		
    }

    closegraph();
    return 0;
}
