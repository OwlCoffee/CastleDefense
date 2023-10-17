/*
* Copyright (c) 2006-2007 Erin Catto http://www.gphysics.com
*
* Permission to use, copy, modify, distribute and sell this software
* and its documentation for any purpose is hereby granted without fee,
* provided that the above copyright notice appear in all copies.
* Erin Catto makes no representations about the suitability
* of this software for any purpose.
* It is provided "as is" without express or implied warranty.
*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#include "imgui/imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"

#define GLFW_INCLUDE_NONE
#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "box2d-lite/World.h"
#include "box2d-lite/Body.h"
#include "box2d-lite/Joint.h"

namespace
{
	GLFWwindow* mainWindow = NULL;

	Body bodies[1000];
	Joint joints[100];
	Body enemies[30];
	float deltaTime = 0.0f;
	float gameoverTime = 0.0f;

	Body* bomb = NULL;

	float timeStep = 1.0f / 60.0f;
	int iterations = 10;
	Vec2 gravity(0.0f, -10.0f);

	int numBodies = 0;
	int numJoints = 0;
	int numEnemies = 0;

	int gameIndex = 0;

	int width = 1024;
	int height = 768;
	float zoom = 10.0f;
	float pan_y = 8.0f;

	double mouseX = 0;
	double mouseY = 0;

	float health = 100.0f;
	int castleBoxCount = 0;
	int currentBoxCount = 0;
	int score = 0;
	int bestScore = 0;

	World world(gravity, iterations);
}

static void glfwErrorCallback(int error, const char* description)
{
	printf("GLFW error %d: %s\n", error, description);
}

static void DrawText(int x, int y, const char* string)
{
	ImVec2 p;
	p.x = float(x);
	p.y = float(y);
	ImGui::Begin("Overlay", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
	ImGui::SetCursorPos(p);
	ImGui::TextColored(ImColor(230, 153, 153, 255), "%s", string);
	ImGui::End();
}

static void DrawBody(Body* body)
{
	Mat22 R(body->rotation);
	Vec2 x = body->position;
	Vec2 h = 0.5f * body->width;

	Vec2 v1 = x + R * Vec2(-h.x, -h.y);
	Vec2 v2 = x + R * Vec2(h.x, -h.y);
	Vec2 v3 = x + R * Vec2(h.x, h.y);
	Vec2 v4 = x + R * Vec2(-h.x, h.y);

	if (body == bomb)
		glColor3f(0.4f, 0.9f, 0.4f);
	/*if (body == enemies)
		glColor3f(1.0f, 0.0f, 0.0f);*/
	else
		glColor3f(0.8f, 0.8f, 0.9f);

	glBegin(GL_LINE_LOOP);
	glVertex2f(v1.x, v1.y);
	glVertex2f(v2.x, v2.y);
	glVertex2f(v3.x, v3.y);
	glVertex2f(v4.x, v4.y);
	glEnd();
}

static void DrawJoint(Joint* joint)
{
	Body* b1 = joint->body1;
	Body* b2 = joint->body2;

	Mat22 R1(b1->rotation);
	Mat22 R2(b2->rotation);

	Vec2 x1 = b1->position;
	Vec2 p1 = x1 + R1 * joint->localAnchor1;

	Vec2 x2 = b2->position;
	Vec2 p2 = x2 + R2 * joint->localAnchor2;

	glColor3f(0.5f, 0.5f, 0.8f);
	glBegin(GL_LINES);
	glVertex2f(x1.x, x1.y);
	glVertex2f(p1.x, p1.y);
	glVertex2f(x2.x, x2.y);
	glVertex2f(p2.x, p2.y);
	glEnd();
}

static void LaunchBomb()
{
	if (!bomb)
	{
		bomb = bodies + numBodies;
		bomb->Set(Vec2(0.5f, 0.5f), 20.0f);
		bomb->friction = 0.2f;
		world.Add(bomb);
		++numBodies;
	}

	bomb->position.Set(-3.0f, 6.0f);
	bomb->rotation = Random(-1.5f, 1.5f);
	bomb->velocity = 0.1f * Vec2(mouseX - (width / 2.0f), -mouseY + 540);
	bomb->angularVelocity = Random(-20.0f, 20.0f);
}

static void EnemySpawner(Body* e)
{
	for (int i = 1; i <= 20; i++)
	{
		e->Set(Vec2(0.5f, 0.5f), 40.0f);
		e->position.Set(10.0f * i, 0.75f);
		e->velocity = Vec2(-2.0f, 0.0f);
		e->friction = 10.0f;
		world.Add(e);
		++e; ++numEnemies;
	}
}

int MakeCastle(Body* b, Vec2 pos, int width, int length)
{
	Vec2 y;
	int count = 0;

	for (int i = 0; i < length; ++i)
	{
		y = pos;

		if (i < length - 2)
		{
			for (int j = 0; j < width; ++j)
			{
				b->Set(Vec2(0.5f, 0.5f), 10.0f);
				b->friction = 0.2f;
				b->position = y;
				world.Add(b);
				++b; ++numBodies;
				count++;

				y += Vec2(0.525f, 0.0f);
			}
		}
		else
		{
			for (int j = 0; j < width; j += 2)
			{
				b->Set(Vec2(0.5f, 0.5f), 10.0f);
				b->friction = 0.2f;
				b->position = y;
				world.Add(b);
				++b; ++numBodies;
				count++;

				y += Vec2(1.05f, 0.0f);
			}
		}

		pos += Vec2(0.0f, 0.75f);
	}

	return count;
}

static void CastleDefence(Body* b, Joint* j)
{
	b->Set(Vec2(300.0f, 20.0f), FLT_MAX); // Floor
	b->position.Set(0.0f, -0.5f * b->width.y);
	world.Add(b);
	++b; ++numBodies;

	castleBoxCount = MakeCastle(b, Vec2(-10.0f, 0.75f), 13, 10);

	EnemySpawner(enemies);
}


void (*games[])(Body* b, Joint* j) = { CastleDefence };
const char* gameStrings[] = {
	"Castle Defence" };

static void InitGame(int index)
{
	world.Clear();
	numBodies = 0;
	numJoints = 0;
	numEnemies = 0;
	bomb = NULL;

	gameIndex = index;
	games[index](bodies, joints);
}

static void Keyboard(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action != GLFW_PRESS)
	{
		return;
	}

	switch (key)
	{
	case GLFW_KEY_ESCAPE:
		// Quit
		glfwSetWindowShouldClose(mainWindow, GL_TRUE);
		break;

	case '1':
		InitGame(key - GLFW_KEY_1);
		break;

	case GLFW_KEY_A:
		World::accumulateImpulses = !World::accumulateImpulses;
		break;

	case GLFW_KEY_P:
		World::positionCorrection = !World::positionCorrection;
		break;

	case GLFW_KEY_W:
		World::warmStarting = !World::warmStarting;
		break;
	}
}

static void MousePos(GLFWwindow* window, double xpos, double ypos)
{
	mouseX = xpos;
	mouseY = ypos;
}

static void Mouse(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		LaunchBomb();
	}
}

static void Reshape(GLFWwindow*, int w, int h)
{
	width = w;
	height = h > 0 ? h : 1;

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float aspect = float(width) / float(height);
	if (width >= height)
	{
		// aspect >= 1, set the height from -1 to 1, with larger width
		glOrtho(-zoom * aspect, zoom * aspect, -zoom + pan_y, zoom + pan_y, -1.0, 1.0);
	}
	else
	{
		// aspect < 1, set the width to -1 to 1, with larger height
		glOrtho(-zoom, zoom, -zoom / aspect + pan_y, zoom / aspect + pan_y, -1.0, 1.0);
	}
}

int main(int, char**)
{
	glfwSetErrorCallback(glfwErrorCallback);

	if (glfwInit() == 0)
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	mainWindow = glfwCreateWindow(width, height, "box2d-lite", NULL, NULL);
	if (mainWindow == NULL)
	{
		fprintf(stderr, "Failed to open GLFW mainWindow.\n");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(mainWindow);

	// Load OpenGL functions using glad
	int gladStatus = gladLoadGL();
	if (gladStatus == 0)
	{
		fprintf(stderr, "Failed to load OpenGL.\n");
		glfwTerminate();
		return -1;
	}

	glfwSwapInterval(1);
	glfwSetWindowSizeCallback(mainWindow, Reshape);
	glfwSetKeyCallback(mainWindow, Keyboard);
	glfwSetCursorPosCallback(mainWindow, MousePos);
	glfwSetMouseButtonCallback(mainWindow, Mouse);

	float xscale, yscale;
	glfwGetWindowContentScale(mainWindow, &xscale, &yscale);
	float uiScale = xscale;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsClassic();
	ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
	ImGui_ImplOpenGL2_Init();
	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = uiScale;

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float aspect = float(width) / float(height);
	if (width >= height)
	{
		// aspect >= 1, set the height from -1 to 1, with larger width
		glOrtho(-zoom * aspect, zoom * aspect, -zoom + pan_y, zoom + pan_y, -1.0, 1.0);
	}
	else
	{
		// aspect < 1, set the width to -1 to 1, with larger height
		glOrtho(-zoom, zoom, -zoom / aspect + pan_y, zoom / aspect + pan_y, -1.0, 1.0);
	}

	InitGame(0);

	while (!glfwWindowShouldClose(mainWindow)) // Loop
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Globally position text
		ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f));
		ImGui::Begin("Overlay", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
		ImGui::End();

		/*DrawText(5, 5, demoStrings[demoIndex]);
		DrawText(5, 35, "Keys: 1-9 Demos, Space to Launch the Bomb");*/

		/*char buffer[64];
		sprintf(buffer, "(A)ccumulation %s", World::accumulateImpulses ? "ON" : "OFF");
		DrawText(5, 65, buffer);

		sprintf(buffer, "(P)osition Correction %s", World::positionCorrection ? "ON" : "OFF");
		DrawText(5, 95, buffer);

		sprintf(buffer, "(W)arm Starting %s", World::warmStarting ? "ON" : "OFF");
		DrawText(5, 125, buffer);*/

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		world.Step(timeStep);

		for (int i = 0; i < numBodies; ++i)
		{
			DrawBody(bodies + i);
			if (-13.0f <= (bodies + i)->position.x && (bodies + i)->position.x <= -2.0f)
				if (0.0f <= (bodies + i)->position.y && (bodies + i)->position.y <= 7.0f) currentBoxCount++;
		}

		health = ((float)currentBoxCount / (float)castleBoxCount) * 100.0f;
		if (health >= 100.0f)health = 100.0f;

		char buffer[64];
		sprintf(buffer, "Health : %f", health);
		DrawText(5, 5, buffer);

		sprintf(buffer, "Best Score : %d", bestScore);
		DrawText(5, 35, buffer);

		sprintf(buffer, "Score : %d", score);
		DrawText(5, 65, buffer);

		if (health <= 70.0f)
		{
			sprintf(buffer, "Game Over!");
			DrawText(5, 95, buffer);

			gameoverTime += timeStep;
			if (gameoverTime >= 3.0f)
			{
				bestScore = score;
				score = 0;
				InitGame(0);
			}
		}

		currentBoxCount = 0;

		for (int i = 0; i < numJoints; ++i)
			DrawJoint(joints + i);

		for (int i = 0; i < numEnemies; i++)
		{
			DrawBody(enemies + i);
			(enemies + i)->AddForce(Vec2(-5.0f, 0.0f));

			if ((enemies + i)->velocity.x > 1.5f)
			{
				(enemies + i)->position = Vec2(50.0f, 0.75f);
			}
		}

		deltaTime += timeStep;
		if (deltaTime >= 2.0f)
		{
			score += deltaTime * 100 - 1;

			for (int i = 0; i < numEnemies; i++)
			{
				(enemies + i)->AddForce(Vec2(-10000.0f, Random(10000.0f, 15000.0f)));
				(enemies + i)->angularVelocity = Random(1.0f, 10.0f);
			}
			deltaTime = 0.0f;
		}

		glPointSize(4.0f);
		glColor3f(1.0f, 0.0f, 0.0f);
		glBegin(GL_POINTS);
		std::map<ArbiterKey, Arbiter>::const_iterator iter;
		for (iter = world.arbiters.begin(); iter != world.arbiters.end(); ++iter)
		{
			const Arbiter& arbiter = iter->second;
			for (int i = 0; i < arbiter.numContacts; ++i)
			{
				Vec2 p = arbiter.contacts[i].position;
				glVertex2f(p.x, p.y);
			}
		}
		glEnd();
		glPointSize(1.0f);

		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

		glfwPollEvents();
		glfwSwapBuffers(mainWindow);
	}

	glfwTerminate();
	return 0;
}
