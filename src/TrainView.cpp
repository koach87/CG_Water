/************************************************************************
     File:        TrainView.cpp

     Author:     
                  Michael Gleicher, gleicher@cs.wisc.edu

     Modifier
                  Yu-Chi Lai, yu-chi@cs.wisc.edu
     
     Comment:     
						The TrainView is the window that actually shows the 
						train. Its a
						GL display canvas (Fl_Gl_Window).  It is held within 
						a TrainWindow
						that is the outer window with all the widgets. 
						The TrainView needs 
						to be aware of the window - since it might need to 
						check the widgets to see how to draw

	  Note:        we need to have pointers to this, but maybe not know 
						about it (beware circular references)

     Platform:    Visio Studio.Net 2003/2005

*************************************************************************/

#include <iostream>
#include <Fl/fl.h>

// we will need OpenGL, and OpenGL needs windows.h
#include <windows.h>
//#include "GL/gl.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <GL/glu.h>

#include "TrainView.H"
#include "TrainWindow.H"
#include "Utilities/3DUtils.H"

#ifdef EXAMPLE_SOLUTION
#	include "TrainExample/TrainExample.H"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//************************************************************************
//
// * Constructor to set up the GL window
//========================================================================
TrainView::
TrainView(int x, int y, int w, int h, const char* l) 
	: Fl_Gl_Window(x,y,w,h,l)
//========================================================================
{
	mode( FL_RGB|FL_ALPHA|FL_DOUBLE | FL_STENCIL );

	resetArcball();
}

//************************************************************************
//
// * Reset the camera to look at the world
//========================================================================
void TrainView::
resetArcball()
//========================================================================
{
	// Set up the camera to look at the world
	// these parameters might seem magical, and they kindof are
	// a little trial and error goes a long way
	arcball.setup(this, 40, 250, .2f, .4f, 0);
}

//************************************************************************
//
// * FlTk Event handler for the window
//########################################################################
// TODO: 
//       if you want to make the train respond to other events 
//       (like key presses), you might want to hack this.
//########################################################################
//========================================================================
int TrainView::handle(int event)
{
	// see if the ArcBall will handle the event - if it does, 
	// then we're done
	// note: the arcball only gets the event if we're in world view
	if (tw->worldCam->value())
		if (arcball.handle(event)) 
			return 1;

	// remember what button was used
	static int last_push;

	switch(event) {
		// Mouse button being pushed event
		case FL_PUSH:
			last_push = Fl::event_button();
			// if the left button be pushed is left mouse button
			if (last_push == FL_LEFT_MOUSE  ) {
				doPick();
				damage(1);
				return 1;
			};
			break;

	   // Mouse button release event
		case FL_RELEASE: // button release
			damage(1);
			last_push = 0;
			return 1;

		// Mouse button drag event
		case FL_DRAG:

			// Compute the new control point position
			if ((last_push == FL_LEFT_MOUSE) && (selectedCube >= 0)) {
				ControlPoint* cp = &m_pTrack->points[selectedCube];

				double r1x, r1y, r1z, r2x, r2y, r2z;
				getMouseLine(r1x, r1y, r1z, r2x, r2y, r2z);

				double rx, ry, rz;
				mousePoleGo(r1x, r1y, r1z, r2x, r2y, r2z, 
								static_cast<double>(cp->pos.x), 
								static_cast<double>(cp->pos.y),
								static_cast<double>(cp->pos.z),
								rx, ry, rz,
								(Fl::event_state() & FL_CTRL) != 0);

				cp->pos.x = (float) rx;
				cp->pos.y = (float) ry;
				cp->pos.z = (float) rz;
				damage(1);
			}
			break;

		// in order to get keyboard events, we need to accept focus
		case FL_FOCUS:
			return 1;

		// every time the mouse enters this window, aggressively take focus
		case FL_ENTER:	
			focus(this);
			break;

		case FL_KEYBOARD:
		 		int k = Fl::event_key();
				int ks = Fl::event_state();
				if (k == 'p') {
					// Print out the selected control point information
					if (selectedCube >= 0) 
						printf("Selected(%d) (%g %g %g) (%g %g %g)\n",
								 selectedCube,
								 m_pTrack->points[selectedCube].pos.x,
								 m_pTrack->points[selectedCube].pos.y,
								 m_pTrack->points[selectedCube].pos.z,
								 m_pTrack->points[selectedCube].orient.x,
								 m_pTrack->points[selectedCube].orient.y,
								 m_pTrack->points[selectedCube].orient.z);
					else
						printf("Nothing Selected\n");

					return 1;
				};
				break;
	}

	return Fl_Gl_Window::handle(event);
}

//************************************************************************
//
// * this is the code that actually draws the window
//   it puts a lot of the work into other routines to simplify things
//========================================================================
void TrainView::draw()
{
	t_time+=1.0f;
	//*********************************************************************
	//
	// * Set up basic opengl informaiton
	//
	//**********************************************************************
	//initialized glad
	if (gladLoadGL())
	{
		//initiailize VAO, VBO, Shader...
		initTilesShader();
		initSineWater();
		initHeightWater();
		initSkyboxShader();

		if (!this->commom_matrices)
			this->commom_matrices = new UBO();
		this->commom_matrices->size = 2 * sizeof(glm::mat4);
		glGenBuffers(1, &this->commom_matrices->ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, this->commom_matrices->ubo);
		glBufferData(GL_UNIFORM_BUFFER, this->commom_matrices->size, NULL, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
		
		
	}
	else
		throw std::runtime_error("Could not initialize GLAD!");

	// Set up the view port
	glViewport(0,0,w(),h());

	// clear the window, be sure to clear the Z-Buffer too
	glClearColor(0,0,.3f,0);		// background should be blue

	// we need to clear out the stencil buffer since we'll use
	// it for shadows
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH);

	// Blayne prefers GL_DIFFUSE
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	// prepare for projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	setProjection();		// put the code to set up matrices here

	//######################################################################
	// TODO: 
	// you might want to set the lighting up differently. if you do, 
	// we need to set up the lights AFTER setting up the projection
	//######################################################################
	// enable the lighting
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	// top view only needs one light
	if (tw->topCam->value()) {
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
	} else {
		glEnable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
	}

	//*********************************************************************
	//
	// * set the light parameters
	//
	//**********************************************************************
	GLfloat lightPosition1[]	= {0,1,1,0}; // {50, 200.0, 50, 1.0};
	GLfloat lightPosition2[]	= {1, 0, 0, 0};
	GLfloat lightPosition3[]	= {0, -1, 0, 0};
	GLfloat yellowLight[]		= {0.5f, 0.5f, .1f, 1.0};
	GLfloat whiteLight[]			= {1.0f, 1.0f, 1.0f, 1.0};
	GLfloat blueLight[]			= {.1f,.1f,.3f,1.0};
	GLfloat grayLight[]			= {.3f, .3f, .3f, 1.0};

	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition1);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, whiteLight);
	glLightfv(GL_LIGHT0, GL_AMBIENT, grayLight);

	glLightfv(GL_LIGHT1, GL_POSITION, lightPosition2);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, yellowLight);

	glLightfv(GL_LIGHT2, GL_POSITION, lightPosition3);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, blueLight);

	// set linstener position 
	if(selectedCube >= 0)
		alListener3f(AL_POSITION, 
			m_pTrack->points[selectedCube].pos.x,
			m_pTrack->points[selectedCube].pos.y,
			m_pTrack->points[selectedCube].pos.z);
	else
		alListener3f(AL_POSITION, 
			this->source_pos.x, 
			this->source_pos.y,
			this->source_pos.z);


	//*********************************************************************
	// now draw the ground plane
	//*********************************************************************
	// set to opengl fixed pipeline(use opengl 1.x draw function)
	glUseProgram(0);

	setupFloor();
	glDisable(GL_LIGHTING);

	// delete by koach
	//drawFloor(200,10);


	//*********************************************************************
	// now draw the object and we need to do it twice
	// once for real, and then once for shadows
	//*********************************************************************
	glEnable(GL_LIGHTING);


	setupObjects();

	drawStuff();

	// this time drawing is for shadows (except for top view)
	if (!tw->topCam->value()) {
		setupShadows();
		drawStuff(true);
		unsetupShadows();
	}

	setUBO();
	glBindBufferRange(
		GL_UNIFORM_BUFFER, /*binding point*/0, this->commom_matrices->ubo, 0, this->commom_matrices->size);

	drawTiles();


	if (tw->waveBrowser->value() == 1)
		drawSineWater();
	else if (tw->waveBrowser->value() == 2)
		drawHeightWater();

	// draw skybox
	drawSkybox();
}

//************************************************************************
//
// * This sets up both the Projection and the ModelView matrices
//   HOWEVER: it doesn't clear the projection first (the caller handles
//   that) - its important for picking
//========================================================================
void TrainView::
setProjection()
//========================================================================
{
	// Compute the aspect ratio (we'll need it)
	float aspect = static_cast<float>(w()) / static_cast<float>(h());

	// Check whether we use the world camp
	if (tw->worldCam->value())
		arcball.setProjection(false);
	// Or we use the top cam
	else if (tw->topCam->value()) {
		float wi, he;
		if (aspect >= 1) {
			wi = 110;
			he = wi / aspect;
		} 
		else {
			he = 110;
			wi = he * aspect;
		}

		// Set up the top camera drop mode to be orthogonal and set
		// up proper projection matrix
		glMatrixMode(GL_PROJECTION);
		glOrtho(-wi, wi, -he, he, 200, -200);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(-90,1,0,0);
	} 
	// Or do the train view or other view here
	//####################################################################
	// TODO: 
	// put code for train view projection here!	
	//####################################################################
	else {
#ifdef EXAMPLE_SOLUTION
		trainCamView(this,aspect);
#endif
	}
}

//************************************************************************
//
// * this draws all of the stuff in the world
//
//	NOTE: if you're drawing shadows, DO NOT set colors (otherwise, you get 
//       colored shadows). this gets called twice per draw 
//       -- once for the objects, once for the shadows
//########################################################################
// TODO: 
// if you have other objects in the world, make sure to draw them
//########################################################################
//========================================================================
void TrainView::drawStuff(bool doingShadows)
{
	// Draw the control points
	// don't draw the control points if you're driving 
	// (otherwise you get sea-sick as you drive through them)
	if (!tw->trainCam->value()) {
		for(size_t i=0; i<m_pTrack->points.size(); ++i) {
			if (!doingShadows) {
				if ( ((int) i) != selectedCube)
					glColor3ub(240, 60, 60);
				else
					glColor3ub(240, 240, 30);
			}
			m_pTrack->points[i].draw();
		}
	}
	// draw the track
	//####################################################################
	// TODO: 
	// call your own track drawing code
	//####################################################################

#ifdef EXAMPLE_SOLUTION
	drawTrack(this, doingShadows);
#endif

	// draw the train
	//####################################################################
	// TODO: 
	//	call your own train drawing code
	//####################################################################
#ifdef EXAMPLE_SOLUTION
	// don't draw the train if you're looking out the front window
	if (!tw->trainCam->value())
		drawTrain(this, doingShadows);
#endif
}

// 
//************************************************************************
//
// * this tries to see which control point is under the mouse
//	  (for when the mouse is clicked)
//		it uses OpenGL picking - which is always a trick
//########################################################################
// TODO: 
//		if you want to pick things other than control points, or you
//		changed how control points are drawn, you might need to change this
//########################################################################
//========================================================================
void TrainView::
doPick()
//========================================================================
{
	// since we'll need to do some GL stuff so we make this window as 
	// active window
	make_current();		

	// where is the mouse?
	int mx = Fl::event_x(); 
	int my = Fl::event_y();

	// get the viewport - most reliable way to turn mouse coords into GL coords
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	// Set up the pick matrix on the stack - remember, FlTk is
	// upside down!
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity ();
	gluPickMatrix((double)mx, (double)(viewport[3]-my), 
						5, 5, viewport);

	// now set up the projection
	setProjection();

	// now draw the objects - but really only see what we hit
	GLuint buf[100];
	glSelectBuffer(100,buf);
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);

	// draw the cubes, loading the names as we go
	for(size_t i=0; i<m_pTrack->points.size(); ++i) {
		glLoadName((GLuint) (i+1));
		m_pTrack->points[i].draw();
	}

	// go back to drawing mode, and see how picking did
	int hits = glRenderMode(GL_RENDER);
	if (hits) {
		// warning; this just grabs the first object hit - if there
		// are multiple objects, you really want to pick the closest
		// one - see the OpenGL manual 
		// remember: we load names that are one more than the index
		selectedCube = buf[3]-1;
	} else // nothing hit, nothing selected
		selectedCube = -1;

	printf("Selected Cube %d\n",selectedCube);
}

void TrainView::setUBO()
{
	float wdt = this->pixel_w();
	float hgt = this->pixel_h();

	glm::mat4 view_matrix;
	glGetFloatv(GL_MODELVIEW_MATRIX, &view_matrix[0][0]);
	//HMatrix view_matrix; 
	//this->arcball.getMatrix(view_matrix);

	glm::mat4 projection_matrix;
	glGetFloatv(GL_PROJECTION_MATRIX, &projection_matrix[0][0]);
	//projection_matrix = glm::perspective(glm::radians(this->arcball.getFoV()), (GLfloat)wdt / (GLfloat)hgt, 0.01f, 1000.0f);

	glBindBuffer(GL_UNIFORM_BUFFER, this->commom_matrices->ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), &projection_matrix[0][0]);
	glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), &view_matrix[0][0]);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void TrainView::
initSkyboxShader()
{
	if (!skyboxShader) {
		this->skyboxShader = new Shader(PROJECT_DIR "/src/shaders/skyboxVS.glsl",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/skyboxFS.glsl");

		GLfloat skyboxVertices[] = {

			-1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			-1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f
		};

		//glGenVertexArrays(1, &this->skybox->vao);
		//glGenBuffers(1, this->skybox->vbo);

		//glBindVertexArray(this->skybox->vao);
		//glBindBuffer(GL_ARRAY_BUFFER, this->skybox->vbo[0]);
		//glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
		//glEnableVertexAttribArray(0);
		//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

		// skybox VAO
		glGenVertexArrays(1, &skyboxVAO);
		glGenBuffers(1, &skyboxVBO);
		glBindVertexArray(skyboxVAO);
		glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

		// load textures
		vector<std::string> faces;
		faces.push_back(PROJECT_DIR"/Images/skybox/left.jpg");
		faces.push_back(PROJECT_DIR"/Images/skybox/right.jpg");
		faces.push_back(PROJECT_DIR"/Images/skybox/top.jpg");
		faces.push_back(PROJECT_DIR"/Images/skybox/bottom.jpg");
		faces.push_back(PROJECT_DIR"/Images/skybox/front.jpg");
		faces.push_back(PROJECT_DIR"/Images/skybox/back.jpg");
		cubemapTexture = loadCubemap(faces);
	}
	
}
unsigned int TrainView::
loadCubemap(std::vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrComponents;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrComponents, 0);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}

void TrainView::
drawSkybox()
{
	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
	skyboxShader->Use();
	glUniform1i(glGetUniformLocation(this->skyboxShader->Program, "skybox"), 0);
	glm::mat4 view;
	glm::mat4 projection;

	glGetFloatv(GL_MODELVIEW_MATRIX, &view[0][0]);
	glGetFloatv(GL_PROJECTION_MATRIX, &projection[0][0]);
	view = glm::mat4(glm::mat3(view)); // remove translation from the view matrix

	glGetFloatv(GL_PROJECTION_MATRIX, &projection[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(this->skyboxShader->Program, "view"), 1, GL_FALSE, &view[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(this->skyboxShader->Program, "projection"), 1, GL_FALSE, &projection[0][0]);

	// skybox cube
	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glDepthFunc(GL_LESS); // set depth function back to default
}

void TrainView::
initTilesShader()
{
	if (!this->tilesShader)
		this->tilesShader = new
		Shader(
			PROJECT_DIR "/src/shaders/simple.vert",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/simple.frag");

	if (!this->tiles) {
		GLfloat  vertices[] = {
			// back
			1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f, 1.0f, -1.0f,
			1.0f, 1.0f, -1.0f,

			//left
			1.0f, -1.0f, 1.0f,
			1.0f, -1.0f, -1.0f,
			1.0f, 1.0f, -1.0f,
			1.0f, 1.0f, 1.0f,

			//front
			-1.0f, -1.0f, 1.0f,
			1.0f, -1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,

			//right
			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f, 1.0f,
			-1.0f, 1.0f, 1.0f,
			-1.0f, 1.0f, -1.0f,

			//down
			-1.0f, -1.0f, 1.0f,
			1.0f, -1.0f, 1.0f,
			1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,

		};
		GLfloat  normal[] = {
			//back
			0.0f, 0.0f, -1.0f,
			0.0f, 0.0f, -1.0f,
			0.0f, 0.0f, -1.0f,
			0.0f, 0.0f, -1.0f,

			//left
			1.0f, 0.0f, 0.0f,
			1.0f, 0.0f, 0.0f,
			1.0f, 0.0f, 0.0f,
			1.0f, 0.0f, 0.0f,

			//front
			0.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f,

			//right
			-1.0f, 0.0f, 0.0f,
			-1.0f, 0.0f, 0.0f,
			-1.0f, 0.0f, 0.0f,
			-1.0f, 0.0f, 0.0f,

			//down
			0.0f, -1.0f, 0.0f,
			0.0f, -1.0f, 0.0f,
			0.0f, -1.0f, 0.0f,
			0.0f, -1.0f, 0.0f,

		};
		GLfloat  texture_coordinate[] = {
			1.0f, 1.0f,
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,

			1.0f, 1.0f,
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,

			1.0f, 1.0f,
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,

			1.0f, 1.0f,
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,

			1.0f, 1.0f,
			0.0f, 1.0f,
			0.0f, 0.0f,
			1.0f, 0.0f,
		};
		GLuint element[] = {
			//back
			1, 0, 3,
			3, 2, 1,

			//left
			5, 4, 7,
			7, 6, 5,

			//front
			9, 8, 11,
			11, 10, 9,

			//right
			13, 12, 15,
			15, 14, 13,

			//down
			17, 16, 19,
			19, 18, 17,

		};

		this->tiles = new VAO;
		this->tiles->element_amount = sizeof(element) / sizeof(GLuint);
		glGenVertexArrays(1, &this->tiles->vao);
		glGenBuffers(3, this->tiles->vbo);
		glGenBuffers(1, &this->tiles->ebo);

		glBindVertexArray(this->tiles->vao);

		// Position attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->tiles->vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(0);

		// Normal attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->tiles->vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(normal), normal, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);

		// Texture Coordinate attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->tiles->vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(texture_coordinate), texture_coordinate, GL_STATIC_DRAW);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(2);

		//Element attribute
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->tiles->ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(element), element, GL_STATIC_DRAW);

		// Unbind VAO
		glBindVertexArray(0);
	}

	if (!this->tilesTexture)
		this->tilesTexture = new Texture2D(PROJECT_DIR "/Images/tiles.jpg");
}
void TrainView::
drawTiles()
{
	//bind shader
	this->tilesShader->Use();

	glm::mat4 model_matrix = glm::mat4();
	model_matrix = glm::translate(model_matrix, this->source_pos);
	model_matrix = glm::scale(model_matrix, glm::vec3(100.0f, 100.0f, 100.0f));
	glUniformMatrix4fv(
		glGetUniformLocation(this->tilesShader->Program, "u_model"), 1, GL_FALSE, &model_matrix[0][0]);
	glUniform3fv(
		glGetUniformLocation(this->tilesShader->Program, "u_color"),
		1,
		&glm::vec3(0.0f, 1.0f, 0.0f)[0]);
	this->tilesTexture->bind(0);
	glUniform1i(glGetUniformLocation(this->tilesShader->Program, "u_texture"), 0);

	//bind VAO
	glBindVertexArray(this->tiles->vao);

	glDrawElements(GL_TRIANGLES, this->tiles->element_amount, GL_UNSIGNED_INT, 0);

	//unbind VAO
	glBindVertexArray(0);

	//unbind shader(switch to fixed pipeline)
	glUseProgram(0);
}



void TrainView::
initSineWater()
{
	if (!this->sineWaterShader)
		this->sineWaterShader = new
		Shader(
			PROJECT_DIR "/src/shaders/sineVS.glsl",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/sineFS.glsl");

	if (!this->sineWater) {

		float size = 0.01f;
		unsigned int width = 2.0f / size;
		unsigned int height = 2.0f / size;

		GLfloat* vertices = new GLfloat[width * height * 4 * 3]();
		GLfloat* normal = new GLfloat[width * height * 4 * 3]();
		GLfloat* texture_coordinate = new GLfloat[width * height * 4 * 2]();
		GLuint* element = new GLuint[width * height * 6]();

		for (int i = 0; i < width * height * 4 * 3; i += 12)
		{
			unsigned int h = i / 12 / width;
			unsigned int w = i / 12 % width;

			vertices[i] = w * size - 1.0f + size;
			vertices[i + 1] = 0.6f;
			vertices[i + 2] = h * size - 1.0f + size;

			vertices[i + 3] = vertices[i] - size;
			vertices[i + 4] = 0.6f;
			vertices[i + 5] = vertices[i + 2];

			vertices[i + 6] = vertices[i + 3];
			vertices[i + 7] = 0.6f;
			vertices[i + 8] = vertices[i + 5] - size;

			vertices[i + 9] = vertices[i];
			vertices[i + 10] = 0.6f;
			vertices[i + 11] = vertices[i + 8];
		}

		for (int i = 0; i < width * height * 4 * 3; i += 3)
		{
			normal[i] = 0.0f;
			normal[i + 1] = 1.0f;
			normal[i + 2] = 0.0f;
		}

		for (int i = 0; i < width * height * 4 * 2; i += 8)
		{
			texture_coordinate[i] = 1.0f;
			texture_coordinate[i + 1] = 1.0f;

			texture_coordinate[i + 2] = 0.0f;
			texture_coordinate[i + 3] = 1.0f;

			texture_coordinate[i + 4] = 0.0f;
			texture_coordinate[i + 5] = 0.0f;

			texture_coordinate[i + 6] = 1.0f;
			texture_coordinate[i + 7] = 0.0f;
		}

		for (int i = 0, j = 0; i < width * height * 6; i += 6, j += 4)
		{
			element[i] = j + 1;
			element[i + 1] = j;
			element[i + 2] = j + 3;

			element[i + 3] = element[i + 2];
			element[i + 4] = j + 2;
			element[i + 5] = element[i];
		}

		this->sineWater = new VAO;
		this->sineWater->element_amount = width * height * 6;
		glGenVertexArrays(1, &this->sineWater->vao);
		glGenBuffers(3, this->sineWater->vbo);
		glGenBuffers(1, &this->sineWater->ebo);

		glBindVertexArray(this->sineWater->vao);

		// Position attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->sineWater->vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, width * height * 4 * 3 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(0);

		// Normal attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->sineWater->vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, width * height * 4 * 3 * sizeof(GLfloat), normal, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);

		// Texture Coordinate attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->sineWater->vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, width * height * 4 * 2 * sizeof(GLfloat), texture_coordinate, GL_STATIC_DRAW);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(2);

		//Element attribute
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->sineWater->ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, width * height * 6 * sizeof(GLuint), element, GL_STATIC_DRAW);

		// Unbind VAO
		glBindVertexArray(0);
	}
}
void TrainView::
drawSineWater()
{
	glEnable(GL_BLEND);

	this->sineWaterShader->Use();

	glm::mat4 model_matrix = glm::mat4();
	model_matrix = glm::translate(model_matrix, this->source_pos);
	model_matrix = glm::scale(model_matrix, glm::vec3(100.0f, 100.0f, 100.0f));

	glUniformMatrix4fv(
		glGetUniformLocation(this->sineWaterShader->Program, "u_model"), 1, GL_FALSE, &model_matrix[0][0]);
	glUniform3fv(
		glGetUniformLocation(this->sineWaterShader->Program, "u_color"),
		1,
		&glm::vec3(0.0f, 1.0f, 0.0f)[0]);

	glUniform1f(glGetUniformLocation(this->sineWaterShader->Program, ("amplitude")), 0.5f);
	glUniform1f(glGetUniformLocation(this->sineWaterShader->Program, ("wavelength")), 0.5f);
	glUniform1f(glGetUniformLocation(this->sineWaterShader->Program, ("speed")), 1.0f);
	glUniform1f(glGetUniformLocation(this->sineWaterShader->Program, ("time")), t_time);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glUniform1i(glGetUniformLocation(this->sineWaterShader->Program, "skybox"), 0);

	this->tilesTexture->bind(1);
	glUniform1i(glGetUniformLocation(this->sineWaterShader->Program, "tiles"), 1);


	this->moveFactor += this->WAVE_SPEED * t_time;
	this->moveFactor /= 1.0f;
	glUniform1f(glGetUniformLocation(this->sineWaterShader->Program, "moveFactor"), moveFactor);

	GLfloat* view_matrix = new GLfloat[16];

	glGetFloatv(GL_MODELVIEW_MATRIX, view_matrix);

	view_matrix = inverse(view_matrix);

	this->cameraPosition = glm::vec3(view_matrix[12], view_matrix[13], view_matrix[14]);
	glUniform3fv(glGetUniformLocation(this->sineWaterShader->Program, "cameraPos"), 1, &glm::vec3(cameraPosition)[0]);

	this->lightColor = glm::vec3(0.5f, 0.5f, 0.1f);
	glUniform3fv(glGetUniformLocation(this->sineWaterShader->Program, "lightColor"), 1, &glm::vec3(lightColor)[0]);

	this->lightPosition = glm::vec3(50.0f, 200.0f, 50.0f);
	glUniform3fv(glGetUniformLocation(this->sineWaterShader->Program, "lightPosition"), 1, &glm::vec3(lightPosition)[0]);

	//bind VAO
	glBindVertexArray(this->sineWater->vao);

	glDrawElements(GL_TRIANGLES, this->sineWater->element_amount, GL_UNSIGNED_INT, 0);

	//unbind VAO
	glBindVertexArray(0);

	//unbind shader(switch to fixed pipeline)
	glUseProgram(0);

	glDisable(GL_BLEND);
}
void TrainView::
initHeightWater()
{
	if (!this->heightWaterShader)
		this->heightWaterShader = new
		Shader(
			PROJECT_DIR "/src/shaders/heightMapVS.glsl",
			nullptr, nullptr, nullptr,
			PROJECT_DIR "/src/shaders/heightMapFS.glsl");

	if (!this->heightWater) {

		float size = 0.01f;
		unsigned int width = 2.0f / size;
		unsigned int height = 2.0f / size;

		GLfloat* vertices = new GLfloat[width * height * 4 * 3]();
		GLfloat* normal = new GLfloat[width * height * 4 * 3]();
		GLfloat* texture_coordinate = new GLfloat[width * height * 4 * 2]();
		GLuint* element = new GLuint[width * height * 6]();

		// vertices
		for (int i = 0; i < width * height * 4 * 3; i += 12)
		{
			unsigned int h = i / 12 / width;
			unsigned int w = i / 12 % width;

			vertices[i] = w * size - 1.0f + size;
			vertices[i + 1] = 0.6f;
			vertices[i + 2] = h * size - 1.0f + size;

			vertices[i + 3] = vertices[i] - size;
			vertices[i + 4] = 0.6f;
			vertices[i + 5] = vertices[i + 2];

			vertices[i + 6] = vertices[i + 3];
			vertices[i + 7] = 0.6f;
			vertices[i + 8] = vertices[i + 5] - size;

			vertices[i + 9] = vertices[i];
			vertices[i + 10] = 0.6f;
			vertices[i + 11] = vertices[i + 8];
		}
		
		// normal 
		for (int i = 0; i < width * height * 4 * 3; i += 3)
		{
			normal[i] = 0.0f;
			normal[i + 1] = 1.0f;
			normal[i + 2] = 0.0f;
		}

		// textrue
		for (int i = 0; i < width * height * 4 * 2; i += 8)
		{
			texture_coordinate[i] = 1.0f;
			texture_coordinate[i + 1] = 1.0f;

			texture_coordinate[i + 2] = 0.0f;
			texture_coordinate[i + 3] = 1.0f;

			texture_coordinate[i + 4] = 0.0f;
			texture_coordinate[i + 5] = 0.0f;

			texture_coordinate[i + 6] = 1.0f;
			texture_coordinate[i + 7] = 0.0f;
		}

		// element
		for (int i = 0, j = 0; i < width * height * 6; i += 6, j += 4)
		{
			element[i] = j + 1;
			element[i + 1] = j;
			element[i + 2] = j + 3;

			element[i + 3] = element[i + 2];
			element[i + 4] = j + 2;
			element[i + 5] = element[i];
		}



		this->heightWater = new VAO;
		this->heightWater->element_amount = width * height * 6;
		glGenVertexArrays(1, &this->heightWater->vao);
		glGenBuffers(3, this->heightWater->vbo);
		glGenBuffers(1, &this->heightWater->ebo);

		glBindVertexArray(this->heightWater->vao);

		glUseProgram(this->heightWaterShader->Program);
		glUniform1f(
			glGetUniformLocation(this->heightWaterShader->Program, "a_time"), sinWaterCounter);
		
		// Position attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->heightWater->vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, width * height * 4 * 3 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(0);
		
		// Normal attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->heightWater->vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, width * height * 4 * 3 * sizeof(GLfloat), normal, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);

		// Texture Coordinate attribute
		glBindBuffer(GL_ARRAY_BUFFER, this->heightWater->vbo[2]);
		glBufferData(GL_ARRAY_BUFFER, width * height * 4 * 2 * sizeof(GLfloat), texture_coordinate, GL_STATIC_DRAW);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(2);

		//Element attribute
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->heightWater->ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, width * height * 6 * sizeof(GLuint), element, GL_STATIC_DRAW);

		// Unbind VAO
		glBindVertexArray(0);

	}

	for (int i = 0; i < 200; ++i)
	{
		std::string name;
		if (i < 10)
			name = "00" + std::to_string(i);
		else if (i < 100)
			name = "0" + std::to_string(i);
		else
			name = std::to_string(i);

		this->heightTexture.push_back(Texture2D(("Images/waves5/" + name + ".png").c_str()));
	}
}
void TrainView::
drawHeightWater()
{
	glEnable(GL_BLEND);
	//bind shader
	this->heightWaterShader->Use();

	glm::mat4 model_matrix = glm::mat4();
	model_matrix = glm::translate(model_matrix, this->source_pos);
	model_matrix = glm::scale(model_matrix, glm::vec3(100.0f, 100.0f, 100.0f));

	glUniformMatrix4fv(
		glGetUniformLocation(this->heightWaterShader->Program, "u_model"),
		1, GL_FALSE, &model_matrix[0][0]);
	glUniform3fv(
		glGetUniformLocation(this->heightWaterShader->Program, "u_color"),
		1, &glm::vec3(0.0f, 1.0f, 0.0f)[0]);

	heightTexture[heightMapIndex].bind(0);
	glUniform1i(glGetUniformLocation(this->heightWaterShader->Program, "u_texture"), 0);
	this->tilesTexture->bind(1);
	glUniform1i(glGetUniformLocation(this->heightWaterShader->Program, "tiles"), 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, this->cubemapTexture);
	glUniform1i(glGetUniformLocation(this->heightWaterShader->Program, "skyBox"), 0);
	//// ffff
	//glUniform1f(glGetUniformLocation(this->heightWaterShader->Program, "amplitude"), tw->amplitude->value());
	//glUniform1f(glGetUniformLocation(this->heightWaterShader->Program, "wavelength"), tw->waveLength->value());

	//glUniform1f(glGetUniformLocation(this->heightWaterShader->Program, "time"), t_time);

	GLfloat* view_matrix = new GLfloat[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, view_matrix);
	view_matrix = inverse(view_matrix);

	this->cameraPosition = glm::vec3(view_matrix[12], view_matrix[13], view_matrix[14]);
	glUniform3fv(glGetUniformLocation(this->heightWaterShader->Program, "camera"), 1, &cameraPosition[0]);

	//bind VAO
	glBindVertexArray(this->heightWater->vao);

	glDrawElements(GL_TRIANGLES, this->heightWater->element_amount, GL_UNSIGNED_INT, 0);

	//draw drops
	for (int i = 0; i < allDrop.size(); ++i)
	{
		if (t_time - allDrop[i].time > allDrop[i].keepTime)
		{
			allDrop.erase(allDrop.begin() + i);
			--i;
			continue;
		}

		glUniform2f(glGetUniformLocation(this->heightWaterShader->Program, "dropPoint"), allDrop[i].point.x, allDrop[i].point.y);
		glUniform1f(glGetUniformLocation(this->heightWaterShader->Program, "dropTime"), allDrop[i].time);
		glUniform1f(glGetUniformLocation(this->heightWaterShader->Program, "interactiveRadius"), allDrop[i].radius);

		glDrawElements(GL_TRIANGLES, this->heightWater->element_amount, GL_UNSIGNED_INT, 0);
	}

	//unbind VAO
	glBindVertexArray(0);

	//unbind shader(switch to fixed pipeline)
	glUseProgram(0);

	glDisable(GL_BLEND);

}

GLfloat* TrainView::
inverse(GLfloat* m)
{
	GLfloat* inv = new GLfloat[16]();
	float det;

	int i;

	inv[0] = m[5] * m[10] * m[15] -
		m[5] * m[11] * m[14] -
		m[9] * m[6] * m[15] +
		m[9] * m[7] * m[14] +
		m[13] * m[6] * m[11] -
		m[13] * m[7] * m[10];

	inv[4] = -m[4] * m[10] * m[15] +
		m[4] * m[11] * m[14] +
		m[8] * m[6] * m[15] -
		m[8] * m[7] * m[14] -
		m[12] * m[6] * m[11] +
		m[12] * m[7] * m[10];

	inv[8] = m[4] * m[9] * m[15] -
		m[4] * m[11] * m[13] -
		m[8] * m[5] * m[15] +
		m[8] * m[7] * m[13] +
		m[12] * m[5] * m[11] -
		m[12] * m[7] * m[9];

	inv[12] = -m[4] * m[9] * m[14] +
		m[4] * m[10] * m[13] +
		m[8] * m[5] * m[14] -
		m[8] * m[6] * m[13] -
		m[12] * m[5] * m[10] +
		m[12] * m[6] * m[9];

	inv[1] = -m[1] * m[10] * m[15] +
		m[1] * m[11] * m[14] +
		m[9] * m[2] * m[15] -
		m[9] * m[3] * m[14] -
		m[13] * m[2] * m[11] +
		m[13] * m[3] * m[10];

	inv[5] = m[0] * m[10] * m[15] -
		m[0] * m[11] * m[14] -
		m[8] * m[2] * m[15] +
		m[8] * m[3] * m[14] +
		m[12] * m[2] * m[11] -
		m[12] * m[3] * m[10];

	inv[9] = -m[0] * m[9] * m[15] +
		m[0] * m[11] * m[13] +
		m[8] * m[1] * m[15] -
		m[8] * m[3] * m[13] -
		m[12] * m[1] * m[11] +
		m[12] * m[3] * m[9];

	inv[13] = m[0] * m[9] * m[14] -
		m[0] * m[10] * m[13] -
		m[8] * m[1] * m[14] +
		m[8] * m[2] * m[13] +
		m[12] * m[1] * m[10] -
		m[12] * m[2] * m[9];

	inv[2] = m[1] * m[6] * m[15] -
		m[1] * m[7] * m[14] -
		m[5] * m[2] * m[15] +
		m[5] * m[3] * m[14] +
		m[13] * m[2] * m[7] -
		m[13] * m[3] * m[6];

	inv[6] = -m[0] * m[6] * m[15] +
		m[0] * m[7] * m[14] +
		m[4] * m[2] * m[15] -
		m[4] * m[3] * m[14] -
		m[12] * m[2] * m[7] +
		m[12] * m[3] * m[6];

	inv[10] = m[0] * m[5] * m[15] -
		m[0] * m[7] * m[13] -
		m[4] * m[1] * m[15] +
		m[4] * m[3] * m[13] +
		m[12] * m[1] * m[7] -
		m[12] * m[3] * m[5];

	inv[14] = -m[0] * m[5] * m[14] +
		m[0] * m[6] * m[13] +
		m[4] * m[1] * m[14] -
		m[4] * m[2] * m[13] -
		m[12] * m[1] * m[6] +
		m[12] * m[2] * m[5];

	inv[3] = -m[1] * m[6] * m[11] +
		m[1] * m[7] * m[10] +
		m[5] * m[2] * m[11] -
		m[5] * m[3] * m[10] -
		m[9] * m[2] * m[7] +
		m[9] * m[3] * m[6];

	inv[7] = m[0] * m[6] * m[11] -
		m[0] * m[7] * m[10] -
		m[4] * m[2] * m[11] +
		m[4] * m[3] * m[10] +
		m[8] * m[2] * m[7] -
		m[8] * m[3] * m[6];

	inv[11] = -m[0] * m[5] * m[11] +
		m[0] * m[7] * m[9] +
		m[4] * m[1] * m[11] -
		m[4] * m[3] * m[9] -
		m[8] * m[1] * m[7] +
		m[8] * m[3] * m[5];

	inv[15] = m[0] * m[5] * m[10] -
		m[0] * m[6] * m[9] -
		m[4] * m[1] * m[10] +
		m[4] * m[2] * m[9] +
		m[8] * m[1] * m[6] -
		m[8] * m[2] * m[5];

	det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

	if (det == 0)
		return false;

	det = 1.0 / det;

	for (i = 0; i < 16; i++)
		inv[i] = inv[i] * det;

	return inv;
}