#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <fstream>
#include <vector>
#include <queue>

#include "glError.h"
#include "gl/glew.h"
#include "gl/freeglut.h"
#include "ply.h"
#include "icVector.H"
#include "icMatrix.H"
#include "polyhedron.h"
#include "trackball.h"
#include "tmatrix.h"

#include "drawUtil.h"

Polyhedron* poly;
std::vector<PolyLine> lines;
std::vector<icVector3> init_points; // saveing one point for one streamline, we use this to generate lines for a streamline, and save streamlines into streamlines variable.
//std::vector<icVector3> sources;
//std::vector<icVector3> saddles;
//std::vector<icVector3> higher_order;
std::vector<icVector3> points;
std::vector<icVector2> tracing_points;

std::vector<icVector2> current_streamline_points; // used for storing points in a single streamline, to calculate other sample points
std::vector<icVector2> all_steamline_points;	// used for storing all points of existing streamlines except the current streamline, we will add current streamline when we finished using it.
std::vector<PolyLine> streamlines; // for drawing
std::vector<PolyLine> tracing_lines;
std::queue<std::vector<icVector2>> queue; // queue to save valid streamlines


/*scene related variables*/
const float zoomspeed = 0.9;
int win_width = 1024;
int win_height = 1024;
float aspectRatio = win_width / win_height;
const int view_mode = 0;		// 0 = othogonal, 1=perspective
const double radius_factor = 0.9;
/*
Use keys 1 to 0 to switch among different display modes.
Each display mode can be designed to show one type 
visualization result.

Predefined ones: 
display mode 1: solid rendering
display mode 2: show wireframes
display mode 3: render each quad with colors of vertices
*/
int display_mode = 1;

/*User Interaction related variabes*/
float s_old, t_old;
float rotmat[4][4];
double zoom = 1.0;
double translation[2] = { 0, 0 };
int mouse_mode = -2;	// -1 = no action, 1 = tranlate y, 2 = rotate

// IBFV related variables (Van Wijk 2002)
//https://www.win.tue.nl/~vanwijk/ibfv/
#define NPN		64
#define SCALE	4.0
#define ALPHA	8
float tmax = win_width / (SCALE * NPN);
float dmax = SCALE / win_width;
unsigned char* pixels;

const double STEP = 0.1; // You should experiment to find the optimal step size.
const int STEP_MAX = 1000; // Upper limit of steps to take for tracing each streamline.
const float d_sep = 0.8;
const float d_test = d_sep * 0.5;
const float initial_x = 0;
const float initial_y = 0;
bool traceOn = true;
/******************************************************************************
Forward declaration of functions
******************************************************************************/

void init(void);
void initIBFV();

/*glut attaching functions*/
void keyboard(unsigned char key, int x, int y);
void motion(int x, int y);
void displayIBFV();
void display(void);
void mouse(int button, int state, int x, int y);
void mousewheel(int wheel, int direction, int x, int y);
void reshape(int width, int height);

/*functions for element picking*/
void display_vertices(GLenum mode, Polyhedron* poly);
void display_quads(GLenum mode, Polyhedron* poly);
void display_selected_vertex(Polyhedron* poly);
void display_selected_quad(Polyhedron* poly);

/*display vis results*/
void display_polyhedron(Polyhedron* poly);
void find_singularities();
void clear_sing_points();
double find_point_x_in_quad(Quad* temp, double x, double y);
double find_point_y_in_quad(Quad* temp, double x, double y);

double sing_prox(icVector2 pos);
Quad* streamline_step(icVector2& cpos, icVector2& npos, Quad* cquad, bool forward);
Vertex* find_vertex(double xx, double yy);
std::vector<icVector2> build_streamline(const double x, const double y);
icVector2 select_candidate_seed_point_clockwise(const float x, const float y, const float vx, const float vy);
icVector2 select_candidate_seed_point_counterclockwise(const float x, const float y, const float vx, const float vy);
void evenly_spaced_algorithm();
bool is_seed_point_valid(const icVector2 point, const float min_d);
icVector2 calculate_vector(icVector2 point);

/******************************************************************************
Main program.
******************************************************************************/

int main(int argc, char* argv[])
{
	/*load mesh from ply file*/
	FILE* this_file = fopen("../data/vector_data/v1.ply", "r");
	poly = new Polyhedron(this_file);
	fclose(this_file);
	
	/*initialize the mesh*/
	poly->initialize(); // initialize the mesh
	poly->write_info();


	/*init glut and create window*/
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowPosition(20, 20);
	glutInitWindowSize(win_width, win_height);
	glutCreateWindow("Scientific Visualization");

	
	/*initialize openGL*/
	init();

	

	/*the render function and callback registration*/
	glutKeyboardFunc(keyboard);
	glutReshapeFunc(reshape);
	glutDisplayFunc(display);
	glutMotionFunc(motion);
	glutMouseFunc(mouse);
	glutMouseWheelFunc(mousewheel);

	/*event processing loop*/
	glutMainLoop();

	/*clear memory before exit*/
	poly->finalize();	// finalize everything
	free(pixels);
	clear_sing_points();
	return 0;
}

/******************************************************************************
Set projection mode
******************************************************************************/

void set_view(GLenum mode)
{
	GLfloat light_ambient0[] = { 0.3, 0.3, 0.3, 1.0 };
	GLfloat light_diffuse0[] = { 0.7, 0.7, 0.7, 1.0 };
	GLfloat light_specular0[] = { 0.0, 0.0, 0.0, 1.0 };

	GLfloat light_ambient1[] = { 0.0, 0.0, 0.0, 1.0 };
	GLfloat light_diffuse1[] = { 0.5, 0.5, 0.5, 1.0 };
	GLfloat light_specular1[] = { 0.0, 0.0, 0.0, 1.0 };

	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse0);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular0);

	glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse1);
	glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular1);


	glMatrixMode(GL_PROJECTION);
	if (mode == GL_RENDER)
		glLoadIdentity();

	if (aspectRatio >= 1.0) {
		if (view_mode == 0)
			glOrtho(-radius_factor * zoom * aspectRatio, radius_factor * zoom * aspectRatio, -radius_factor * zoom, radius_factor * zoom, -1000, 1000);
		else
			glFrustum(-radius_factor * zoom * aspectRatio, radius_factor * zoom * aspectRatio, -radius_factor * zoom, radius_factor * zoom, 0.1, 1000);
	}
	else {
		if (view_mode == 0)
			glOrtho(-radius_factor * zoom, radius_factor * zoom, -radius_factor * zoom / aspectRatio, radius_factor * zoom / aspectRatio, -1000, 1000);
		else
			glFrustum(-radius_factor * zoom, radius_factor * zoom, -radius_factor * zoom / aspectRatio, radius_factor * zoom / aspectRatio, 0.1, 1000);
	}

	GLfloat light_position[3];
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	light_position[0] = 5.5;
	light_position[1] = 0.0;
	light_position[2] = 0.0;
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	light_position[0] = -0.1;
	light_position[1] = 0.0;
	light_position[2] = 0.0;
	glLightfv(GL_LIGHT2, GL_POSITION, light_position);
}

/******************************************************************************
Update the scene
******************************************************************************/

void set_scene(GLenum mode, Polyhedron* poly)
{
	glTranslatef(translation[0], translation[1], -3.0);

	/*multiply rotmat to current mat*/
	{
		int i, j, index = 0;

		GLfloat mat[16];

		for (i = 0; i < 4; i++)
			for (j = 0; j < 4; j++)
				mat[index++] = rotmat[i][j];

		glMultMatrixf(mat);
	}

	glScalef(0.9 / poly->radius, 0.9 / poly->radius, 0.9 / poly->radius);
	glTranslatef(-poly->center.entry[0], -poly->center.entry[1], -poly->center.entry[2]);
}

/******************************************************************************
Init scene
******************************************************************************/

void init(void) {

	mat_ident(rotmat);

	/* select clearing color */
	glClearColor(0.0, 0.0, 0.0, 0.0);  // background
	glShadeModel(GL_FLAT);
	glPolygonMode(GL_FRONT, GL_FILL);

	glDisable(GL_DITHER);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	//set pixel storage modes
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	
	glEnable(GL_NORMALIZE);
	if (poly->orientation == 0)
		glFrontFace(GL_CW);
	else
		glFrontFace(GL_CCW);

	for (int i = 0; i < poly->nquads; i++)
	{
		Quad* quad = poly->qlist[i];
		quad->singularity = NULL;
	}
}

/******************************************************************************
Initialize IBFV patterns
******************************************************************************/

void initIBFV()
{
	pixels = (unsigned char*)malloc(sizeof(unsigned char) * win_width * win_height * 3);
	memset(pixels, 255, sizeof(unsigned char) * win_width * win_height * 3);

	tmax = win_width / (SCALE * NPN);
	dmax = SCALE / win_width;

	int lut[256];
	int phase[NPN][NPN];
	GLubyte pat[NPN][NPN][4];
	int i, j, k;

	for (i = 0; i < 256; i++) lut[i] = i < 127 ? 0 : 255;
	for (i = 0; i < NPN; i++)
		for (j = 0; j < NPN; j++) phase[i][j] = rand() % 256;

	for (i = 0; i < NPN; i++)
	{
		for (j = 0; j < NPN; j++)
		{
			pat[i][j][0] =
				pat[i][j][1] =
				pat[i][j][2] = lut[(phase[i][j]) % 255];
			pat[i][j][3] = ALPHA;
		}
	}
	
	glNewList(1, GL_COMPILE);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, NPN, NPN, 0, GL_RGBA, GL_UNSIGNED_BYTE, pat);
	glEndList();
}

/******************************************************************************
Pick objects from the scene
******************************************************************************/

int processHits(GLint hits, GLuint buffer[])
{
	unsigned int i, j;
	GLuint names, * ptr;
	double smallest_depth = 1.0e+20, current_depth;
	int seed_id = -1;
	unsigned char need_to_update;

	ptr = (GLuint*)buffer;
	for (i = 0; i < hits; i++) {  /* for each hit  */
		need_to_update = 0;
		names = *ptr;
		ptr++;

		current_depth = (double)*ptr / 0x7fffffff;
		if (current_depth < smallest_depth) {
			smallest_depth = current_depth;
			need_to_update = 1;
		}
		ptr++;
		current_depth = (double)*ptr / 0x7fffffff;
		if (current_depth < smallest_depth) {
			smallest_depth = current_depth;
			need_to_update = 1;
		}
		ptr++;
		for (j = 0; j < names; j++) {  /* for each name */
			if (need_to_update == 1)
				seed_id = *ptr - 1;
			ptr++;
		}
	}
	return seed_id;
}

/******************************************************************************
Diaplay all quads for selection
******************************************************************************/

void display_quads(GLenum mode, Polyhedron* this_poly)
{
	unsigned int i, j;
	GLfloat mat_diffuse[4];

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1., 1.);
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_LIGHTING);

	for (i = 0; i < this_poly->nquads; i++) {
		if (mode == GL_SELECT)
			glLoadName(i + 1);

		Quad* temp_q = this_poly->qlist[i];
		
		glBegin(GL_POLYGON);
		for (j = 0; j < 4; j++) {
			Vertex* temp_v = temp_q->verts[j];
			glVertex3d(temp_v->x, temp_v->y, temp_v->z);
		}
		glEnd();
	}
}

/******************************************************************************
Diaplay all vertices for selection
******************************************************************************/

void display_vertices(GLenum mode, Polyhedron* this_poly)
{
	for (int i = 0; i < this_poly->nverts; i++) {
		if (mode == GL_SELECT)
			glLoadName(i + 1);

		CHECK_GL_ERROR();

		Vertex* temp_v = this_poly->vlist[i];
		drawDot(temp_v->x, temp_v->y, temp_v->z, 0.15);
	}
	CHECK_GL_ERROR();
}

/******************************************************************************
Diaplay selected quad
******************************************************************************/

void display_selected_quad(Polyhedron* this_poly)
{
	if (this_poly->selected_quad == -1)
	{
		return;
	}

	unsigned int i, j;

	glDisable(GL_POLYGON_OFFSET_FILL);
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_LIGHTING);

	Quad* temp_q = this_poly->qlist[this_poly->selected_quad];

	glBegin(GL_POLYGON);
	for (j = 0; j < 4; j++) {
		Vertex* temp_v = temp_q->verts[j];
		glColor3f(1.0, 0.0, 0.0);
		glVertex3d(temp_v->x, temp_v->y, temp_v->z);
	}
	glEnd();
}

/******************************************************************************
Diaplay selected vertex
******************************************************************************/

void display_selected_vertex(Polyhedron* this_poly)
{
	if (this_poly->selected_vertex == -1)
	{
		return;
	}

	Vertex* temp_v = this_poly->vlist[this_poly->selected_vertex];
	drawDot(temp_v->x, temp_v->y, temp_v->z, 0.15, 1.0, 0.0,0.0);

	CHECK_GL_ERROR();
}

/******************************************************************************
Process a keyboard action.  In particular, exit the program when an
"escape" is pressed in the window.
******************************************************************************/

void keyboard(unsigned char key, int x, int y) {
	int i;

	// clear out lines and points
	lines.clear();
	points.clear();

	switch (key) {
	case 27:	// set excape key to exit program
		poly->finalize();  // finalize_everything
		exit(0);
		break;

	case '1':	// solid color display with lighting
		display_mode = 1;
		glutPostRedisplay();
		break;

	case '2':	// wireframe display
		display_mode = 2;
		glutPostRedisplay();
		break;

	case '3':	// checkerboard display
	{
		display_mode = 3;

		double L = (poly->radius * 2) / 30;
		for (int i = 0; i < poly->nquads; i++) {
			Quad* temp_q = poly->qlist[i];
			for (int j = 0; j < 4; j++) {

				Vertex* temp_v = temp_q->verts[j];

				temp_v->R = int(temp_v->x / L) % 2 == 0 ? 1 : 0;
				temp_v->G = int(temp_v->y / L) % 2 == 0 ? 1 : 0;
				temp_v->B = 0.0;
			}
		}
		glutPostRedisplay();
	}
	break;

	case '4':	// Drawing points and lines created by the dots_and_lines_example() function
		display_mode = 4;
		dots_and_lines_example(&points, &lines);
		glutPostRedisplay();
		break;

	case '5':	// IBFV vector field display
		display_mode = 5;
		//saddles.clear();
		//higher_order.clear();
		//sources.clear();
		//find_singularities();
		glutPostRedisplay();
		break;

	case '6':	// add your own display mode
	{
		display_mode = 6;
		//saddles.clear();
		//higher_order.clear();
		//sources.clear();
		//find_singularities();
		streamlines.clear();
		evenly_spaced_algorithm();

		glutPostRedisplay();
	}
	break;

	case 'r':	// reset rotation and transformation
		mat_ident(rotmat);
		translation[0] = 0;
		translation[1] = 0;
		zoom = 1.0;
		glutPostRedisplay();
		break;
	}
}

/******************************************************************************
Callback function for dragging mouse
******************************************************************************/

void motion(int x, int y) {
	float r[4];
	float s, t;

	s = (2.0 * x - win_width) / win_width;
	t = (2.0 * (win_height - y) - win_height) / win_height;

	if ((s == s_old) && (t == t_old))
		return;

	switch (mouse_mode) {
	case 2:

		Quaternion rvec;

		mat_to_quat(rotmat, rvec);
		trackball(r, s_old, t_old, s, t);
		add_quats(r, rvec, rvec);
		quat_to_mat(rvec, rotmat);

		s_old = s;
		t_old = t;

		display();
		break;

	case 1:

		translation[0] += (s - s_old);
		translation[1] += (t - t_old);

		s_old = s;
		t_old = t;

		display();
		break;
	}
}

/******************************************************************************
Callback function for mouse clicks
******************************************************************************/

void mouse(int button, int state, int x, int y) {

	int key = glutGetModifiers();

	if (button == GLUT_LEFT_BUTTON || button == GLUT_RIGHT_BUTTON) {
		
		if (state == GLUT_DOWN) {
			float xsize = (float)win_width;
			float ysize = (float)win_height;

			float s = (2.0 * x - win_width) / win_width;
			float t = (2.0 * (win_height - y) - win_height) / win_height;

			s_old = s;
			t_old = t;

			/*translate*/
			if (button == GLUT_LEFT_BUTTON)
			{
				mouse_mode = 1;
			}

			/*rotate*/
			if (button == GLUT_RIGHT_BUTTON)
			{
				mouse_mode = 2;
			}
		}
		else if (state == GLUT_UP) {

			if (button == GLUT_LEFT_BUTTON && key == GLUT_ACTIVE_SHIFT) {  // build up the selection feedback mode

				/*select face*/

				GLuint selectBuf[512];
				GLint hits;
				GLint viewport[4];

				glGetIntegerv(GL_VIEWPORT, viewport);

				glSelectBuffer(win_width, selectBuf);
				(void)glRenderMode(GL_SELECT);

				glInitNames();
				glPushName(0);

				glMatrixMode(GL_PROJECTION);
				glPushMatrix();
				glLoadIdentity();

				/*create 5x5 pixel picking region near cursor location */
				gluPickMatrix((GLdouble)x, (GLdouble)(viewport[3] - y), 1.0, 1.0, viewport);

				set_view(GL_SELECT);
				set_scene(GL_SELECT, poly);
				display_quads(GL_SELECT, poly);

				glMatrixMode(GL_PROJECTION);
				glPopMatrix();
				glFlush();

				glMatrixMode(GL_MODELVIEW);

				hits = glRenderMode(GL_RENDER);
				poly->selected_quad = processHits(hits, selectBuf);
				printf("Selected quad id = %d\n", poly->selected_quad);
				glutPostRedisplay();

				CHECK_GL_ERROR();

			}
			else if (button == GLUT_LEFT_BUTTON && key == GLUT_ACTIVE_CTRL)
			{
				/*select vertex*/

				GLuint selectBuf[512];
				GLint hits;
				GLint viewport[4];

				glGetIntegerv(GL_VIEWPORT, viewport);

				glSelectBuffer(win_width, selectBuf);
				(void)glRenderMode(GL_SELECT);

				glInitNames();
				glPushName(0);

				glMatrixMode(GL_PROJECTION);
				glPushMatrix();
				glLoadIdentity();

				/*  create 5x5 pixel picking region near cursor location */
				gluPickMatrix((GLdouble)x, (GLdouble)(viewport[3] - y), 1.0, 1.0, viewport);

				set_view(GL_SELECT);
				set_scene(GL_SELECT, poly);
				display_vertices(GL_SELECT, poly);

				glMatrixMode(GL_PROJECTION);
				glPopMatrix();
				glFlush();

				glMatrixMode(GL_MODELVIEW);

				hits = glRenderMode(GL_RENDER);
				poly->selected_vertex = processHits(hits, selectBuf);
				printf("Selected vert id = %d\n", poly->selected_vertex);

				if (poly->selected_vertex >= 0) {
					Vertex * vtemp = poly->vlist[poly->selected_vertex];
					init_points.push_back(icVector3(vtemp->x, vtemp->y, 0));
				}

				glutPostRedisplay();

			}

			mouse_mode = -1;
		}
	}
}

/******************************************************************************
Callback function for mouse wheel scroll
******************************************************************************/

void mousewheel(int wheel, int direction, int x, int y) {
	if (direction == 1) {
		zoom *= zoomspeed;
		glutPostRedisplay();
	}
	else if (direction == -1) {
		zoom /= zoomspeed;
		glutPostRedisplay();
	}
}

/******************************************************************************
Callback function for window reshaping
******************************************************************************/

void reshape(int width, int height)
{
	win_width = width;
	win_height = height;

	aspectRatio = (float)width / (float)height;

	glViewport(0, 0, width, height);

	set_view(GL_RENDER);

	// reset IBFV pixels buffer
	free(pixels);
	initIBFV();
}

/******************************************************************************
Display IBFV vector field visualization (used for Project 3)
******************************************************************************/

void displayIBFV()
{
	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);
	glDisable(GL_LIGHT1);
	glDisable(GL_BLEND);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_FLAT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glClearColor(0.5, 0.5, 0.5, 1.0);  // background for rendering color coding and lighting
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// draw the mesh using pixels and use vector field to advect texture coordinates
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, win_width, win_height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	double modelview_matrix[16], projection_matrix[16];
	int viewport[4];
	glGetDoublev(GL_MODELVIEW_MATRIX, modelview_matrix);
	glGetDoublev(GL_PROJECTION_MATRIX, projection_matrix);
	glGetIntegerv(GL_VIEWPORT, viewport);

	for (int i = 0; i < poly->nquads; i++)
	{
		Quad* qtemp = poly->qlist[i];

		glBegin(GL_QUADS);
		for (int j = 0; j < 4; j++)
		{
			Vertex* vtemp = qtemp->verts[j];

			double tx, ty, dummy;
			gluProject((GLdouble)vtemp->x, (GLdouble)vtemp->y, (GLdouble)vtemp->z,
				modelview_matrix, projection_matrix, viewport, &tx, &ty, &dummy);

			tx = tx / win_width;
			ty = ty / win_height;
			
			icVector2 dp = icVector2(vtemp->vx, vtemp->vy);
			normalize(dp);
			dp *= dmax;

			double dx = -dp.x;
			double dy = -dp.y;

			float px = tx + dx;
			float py = ty + dy;

			glTexCoord2f(px, py);
			glVertex3d(vtemp->x, vtemp->y, vtemp->z);
		}
		glEnd();
	}

	glEnable(GL_BLEND);

	// blend in noise pattern
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glTranslatef(-1.0, -1.0, 0.0);
	glScalef(2.0, 2.0, 1.0);

	glCallList(1);

	glBegin(GL_QUAD_STRIP);

	glTexCoord2f(0.0, 0.0);  glVertex2f(0.0, 0.0);
	glTexCoord2f(0.0, tmax); glVertex2f(0.0, 1.0);
	glTexCoord2f(tmax, 0.0);  glVertex2f(1.0, 0.0);
	glTexCoord2f(tmax, tmax); glVertex2f(1.0, 1.0);
	glEnd();
	glDisable(GL_BLEND);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glReadPixels(0, 0, win_width, win_height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	// draw the mesh using pixels without advecting texture coords
	glClearColor(1.0, 1.0, 1.0, 1.0);  // background for rendering color coding and lighting
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, win_width, win_height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	for (int i = 0; i < poly->nquads; i++)
	{
		Quad* qtemp = poly->qlist[i];
		glBegin(GL_QUADS);
		for (int j = 0; j < 4; j++)
		{
			Vertex* vtemp = qtemp->verts[j];
			double tx, ty, dummy;
			gluProject((GLdouble)vtemp->x, (GLdouble)vtemp->y, (GLdouble)vtemp->z,
				modelview_matrix, projection_matrix, viewport, &tx, &ty, &dummy);
			tx = tx / win_width;
			ty = ty / win_height;
			glTexCoord2f(tx, ty);
			glVertex3d(vtemp->x, vtemp->y, vtemp->z);
		}
		glEnd();
	}

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glShadeModel(GL_SMOOTH);
}

/******************************************************************************
Callback function for scene display
******************************************************************************/

void display(void)
{
	glClearColor(1.0, 1.0, 1.0, 1.0);  // background for rendering color coding and lighting

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	set_view(GL_RENDER);
	set_scene(GL_RENDER, poly);

	/*display the mesh*/
	display_polyhedron(poly);

	/*display selected elements*/
	display_selected_vertex(poly);
	display_selected_quad(poly);


	glFlush();
	glutSwapBuffers();
	glFinish();

	CHECK_GL_ERROR();
}

/******************************************************************************
Diaplay the polygon with visualization results
******************************************************************************/

void display_polyhedron(Polyhedron* poly)
{
	unsigned int i, j;

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1., 1.);

	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel(GL_SMOOTH);

	switch (display_mode)
	{
	case 1:	// solid color display with lighting
	{
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHT1);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		GLfloat mat_diffuse[4] = { 0.24, 0.4, 0.47, 0.0 };
		GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialf(GL_FRONT, GL_SHININESS, 50.0);

		for (int i = 0; i < poly->nquads; i++) {
			Quad* temp_q = poly->qlist[i];
			glBegin(GL_POLYGON);
			for (int j = 0; j < 4; j++) {
				Vertex* temp_v = temp_q->verts[j];
				glNormal3d(temp_v->normal.entry[0], temp_v->normal.entry[1], temp_v->normal.entry[2]);
				glVertex3d(temp_v->x, temp_v->y, temp_v->z);
			}
			glEnd();
		}
	}
	break;

	case 2:	// wireframe display
	{
		glDisable(GL_LIGHTING);
		glEnable(GL_LINE_SMOOTH);
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glLineWidth(1.0);

		for (int i = 0; i < poly->nquads; i++) {
			Quad* temp_q = poly->qlist[i];

			glBegin(GL_POLYGON);
			for (int j = 0; j < 4; j++) {
				Vertex* temp_v = temp_q->verts[j];
				glNormal3d(temp_q->normal.entry[0], temp_q->normal.entry[1], temp_q->normal.entry[2]);
				glColor3f(0.0, 0.0, 0.0);
				glVertex3d(temp_v->x, temp_v->y, temp_v->z);
			}
			glEnd();
		}

		glDisable(GL_BLEND);
	}
	break;

	case 3:	// checkerboard pattern display
	{
		glDisable(GL_LIGHTING);
		for (int i = 0; i < poly->nquads; i++) {
			Quad* temp_q = poly->qlist[i];
			glBegin(GL_POLYGON);
			for (int j = 0; j < 4; j++) {
				Vertex* temp_v = temp_q->verts[j];
				glColor3f(temp_v->R, temp_v->G, temp_v->B);
				glVertex3d(temp_v->x, temp_v->y, temp_v->z);
			}
			glEnd();
		}
	}
	break;

	case 4: // points and lines drawing example
	{
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHT1);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		GLfloat mat_diffuse[4] = { 0.24, 0.4, 0.47, 0.0 };
		GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
		glMaterialf(GL_FRONT, GL_SHININESS, 50.0);

		for (int i = 0; i < poly->nquads; i++) {
			Quad* temp_q = poly->qlist[i];
			glBegin(GL_POLYGON);
			for (int j = 0; j < 4; j++) {
				Vertex* temp_v = temp_q->verts[j];
				glNormal3d(temp_v->normal.entry[0], temp_v->normal.entry[1], temp_v->normal.entry[2]);
				glVertex3d(temp_v->x, temp_v->y, temp_v->z);
			}
			glEnd();
		}

		// draw lines
		for (int k = 0; k < lines.size(); k++)
		{
			drawPolyLine(lines[k], 1.0, 1.0, 0.0, 0.0);
		}

		// draw points
		for (int k = 0; k < points.size(); k++)
		{
			icVector3 point = points[k];
			drawDot(point.x, point.y, point.z);
		}
		break;
	}
	break;

	case 5:	// IBFV vector field display
	{
		displayIBFV();
		glutPostRedisplay();
	}
	break;

	case 6: // add your own display mode
	{
		displayIBFV();
		
		drawDot(initial_x, initial_y, 0);
		for (int k = 0; k < streamlines.size(); ++k)
		{
			drawPolyLine(streamlines[k], 1.0, 1.0, 0.0, 0.0);
		}

		if (traceOn) {
			for (int k = 0; k < tracing_lines.size(); ++k)
			{
				drawPolyLine(tracing_lines[k], 1.0, 1.0, 1.0, 0.0);
			}

			for (int k = 0; k < tracing_points.size(); ++k)
			{
				drawDot(tracing_points[k].x, tracing_points[k].y, 0, 0.15, 1, 1, 1);
			}
		}

		glutPostRedisplay();
	}
	break;

	default:
	{
		// don't draw anything
	}

	}
}


void find_singularities()
{
	// 1. Delete any old singularity points
	clear_sing_points();
	for (int i = 0; i < poly->nquads; i++)
	{
		Quad* quad = poly->qlist[i];
		// 2. Find x1, x2, y1, y2, fx1y1, fx2y1, fx1y2, fx2y2, gx1y1, gx2y1, gx1y2, gx2y2
		// x1 is the smallest x coordinate of the 4 vertices
		// x2 is the largest x coordinate of the 4 vertices
		// y1 is the smallest y coordinate of the 4 vertices
		// y2 is the largest y coordinate of the 4 vertices
		// fx1y1 is the vector value x component at the vertex with coordinates (x1,y1)
		// gx1y1 is the vector value y component at the vertex with coordinates (x1,y1)
		// ...
		// to access the vector components of a Vertex* object, use vert->vx and vert->vy
		double x1 = poly->smallest_x(quad);
		double x2 = poly->largest_x(quad);
		double y1 = poly->smallest_y(quad);
		double y2 = poly->largest_y(quad);
		double fx1y1 = find_point_x_in_quad(quad, x1, y1);
		double fx2y1 = find_point_x_in_quad(quad, x2, y1);
		double fx1y2 = find_point_x_in_quad(quad, x1, y2);
		double fx2y2 = find_point_x_in_quad(quad, x2, y2);
		double gx1y1 = find_point_y_in_quad(quad, x1, y1);
		double gx2y1 = find_point_y_in_quad(quad, x2, y1);
		double gx1y2 = find_point_y_in_quad(quad, x1, y2);
		double gx2y2 = find_point_y_in_quad(quad, x2, y2);
		// 3. compute the coefficients for solving the quadratic equation
		double a00 = fx1y1;
		double a10 = fx2y1 - fx1y1;
		double a01 = fx1y2 - fx1y1;
		double a11 = fx1y1 - fx2y1 - fx1y2 + fx2y2;
		double b00 = gx1y1;
		double b10 = gx2y1 - gx1y1;
		double b01 = gx1y2 - gx1y1;
		double b11 = gx1y1 - gx2y1 - gx1y2 + gx2y2;
		double c00 = a11 * b00 - a00 * b11;
		double c10 = a11 * b10 - a10 * b11;
		double c01 = a11 * b01 - a01 * b11;
		// 4. Compute the coefficients of the quadratic equation about s:
		// (-a11*c10)s2 + (-a11*c00 - a01*c10 + a10*c01)s + (a00*c01 - a01*c00) = 0.
		double a = -a11 * c10;
		double b = -a11 * c00 - a01 * c10 + a10 * c01;
		double c = a00 * c01 - a01 * c00;
		// 5. Use the quadratic formula to solve for the s.
		// (check beforehand for complex values or dividing by zero)
		// You will get two values for s because of the ��.
		double s1 = (-b + sqrt(b * b - 4. * a * c)) / 2. / a;
		double s2 = (-b - sqrt(b * b - 4. * a * c)) / 2. / a;
		// 6. Use both values of s to get two corresponding values for t:
		double t1 = -(c00 / c01) - (c10 / c01) * s1;
		double t2 = -(c00 / c01) - (c10 / c01) * s2;
		// 7. For each (s,t) pair, check that both values are between 0 and 1.
		// Either one or none of these pairs will satisfy this condition.
		// If one of the pairs has both components between 0 and 1,
		// then it corresponds to a singularity inside the quad.
		double s, t;

		if ( s1 > 0 && s1 < 1 && t1 > 0 && t1 < 1 ) {
			// (s1, t1)
			s = s1;
			t = t1;
		}
		else if (s2 > 0 && s2 < 1 && t2 > 0 && t2 < 1) {
			// (s2, t2)
			s = s2;
			t = t2;
		}
		else {
			// none of the pairs satisfies
			quad->singularity = NULL;
			continue;
		}
		// 8. Compute the coordinates of the singularity inside the quad using s and t
		// use s to interpolate between x1 and x2 (s tells you how far inbetween
		// x1 and x2 the x coordinate is).
		// use t to interpolate between y1 and y2 (t tells you how far inbetween
		// y1 and y2 the y coordinate is).

		double sing_x, sing_y;
		sing_x = x1 + s * (x2 - x1);
		sing_y = y1 + t * (y2 - y1);
		// 9. Insert the singularity into the quad data structure
		// quad->singularity = new icVector2(x,y);
		// You will need to create a new field in the Quad data structure to store
		// the singularity point. The Quad class definition is located in the 
		// polyhedron.h file.
		quad->singularity = new icVector2(sing_x, sing_y);

		// classify types of singularity
		// 10. calculate the jacobian values dfdx, dfdy, dgdx, dgdy
		double dfdx = (-(y2 - sing_y) * fx1y1 + (y2 - sing_y) * fx2y1 - (sing_y - y1) * fx1y2 + (sing_y - y1) * fx2y2) / (x2 - x1) * (y2 - y1);
		double dfdy = (-(x2 - sing_x) * fx1y1 - (sing_x - x1) * fx2y1 + (x2 - sing_x) * fx1y2 + (sing_x - x1) * fx2y2) / (x2 - x1) * (y2 - y1);
		double dgdx = (-(y2 - sing_y) * gx1y1 + (y2 - sing_y) * gx2y1 - (sing_y - y1) * gx1y2 + (sing_y - y1) * gx2y2) / (x2 - x1) * (y2 - y1);
		double dgdy = (-(x2 - sing_x) * gx1y1 - (sing_x - x1) * gx2y1 + (x2 - sing_x) * gx1y2 + (sing_x - x1) * gx2y2) / (x2 - x1) * (y2 - y1);
		icMatrix2x2 m = icMatrix2x2(dfdx, dfdy, dgdx, dgdy);
		double determ = determinant(m);
		//if (determ > 0)
		//	sources.push_back(icVector3(sing_x, sing_y, 0));
		//else if (determ == 0)
		//	saddles.push_back(icVector3(sing_x, sing_y, 0));
		//else
		//	higher_order.push_back(icVector3(sing_x, sing_y, 0));
	}
}

double find_point_x_in_quad(Quad* temp, double x, double y) {
	for (int i = 0; i < 4; i++) {
		Vertex* vtemp = temp->verts[i];
		if (vtemp->x == x && vtemp->y == y) {
			return vtemp->vx;
		}
	}
}

double find_point_y_in_quad(Quad* temp, double x, double y) {
	for (int i = 0; i < 4; i++) {
		Vertex* vtemp = temp->verts[i];
		if (vtemp->x == x && vtemp->y == y) {
			return vtemp->vy;
		}
	}
}

void clear_sing_points() {
	for (int i = 0; i < poly->nquads; i++)
	{
		Quad* quad = poly->qlist[i];
		if (quad->singularity != NULL) {
			// free memory
			delete quad->singularity;
		}
		quad->singularity = NULL;
	}
	return;
}

double sing_prox(icVector2 pos)
{
	double prox = DBL_MAX;
	for (int i = 0; i < poly->nquads; i++)
	{
		Quad* quad = poly->qlist[i];
		if (quad->singularity == NULL) continue;
		icVector2 spos = *(quad->singularity);
		// get the (x,y) coordinates of the
		// singularity in an icVector2 object.
		double dist = length(pos - spos);
		if (dist < prox)
			prox = dist;
	}
	return prox;
}

Quad* streamline_step(icVector2& cpos, icVector2& npos, Quad* cquad, bool forward)
{
	double x1, y1, x2, y2, f11, f12, f21, f22, g11, g21, g12, g22;
	Vertex* v11, * v12, * v21, * v22;

	x1 = poly->smallest_x(cquad);
	x2 = poly->largest_x(cquad);
	y1 = poly->smallest_y(cquad);
	y2 = poly->largest_y(cquad);

	v11 = find_vertex(x1, y1);
	f11 = v11->vx;
	g11 = v11->vy;

	v12 = find_vertex(x1, y2);
	f12 = v12->vx;
	g12 = v12->vy;

	v21 = find_vertex(x2, y1);
	f21 = v21->vx;
	g21 = v21->vy;

	v22 = find_vertex(x2, y2);
	f22 = v22->vx;
	g22 = v22->vy;

	double x0 = cpos.x;
	double y0 = cpos.y;
	icVector2 vect;
	double m1 = (x2 - x0) * (y2 - y0) / (x2 - x1) / (y2 - y1);
	double m2 = (x0 - x1) * (y2 - y0) / (x2 - x1) / (y2 - y1);
	double m3 = (x2 - x0) * (y0 - y1) / (x2 - x1) / (y2 - y1);
	double m4 = (x0 - x1) * (y0 - y1) / (x2 - x1) / (y2 - y1);
	vect.x = m1 * f11 + m2 * f21 + m3 * f12 + m4 * f22;
	vect.y = m1 * g11 + m2 * g21 + m3 * g12 + m4 * g22;
	normalize(vect);
	if (!forward) { vect *= -1.0; }
	npos.x = cpos.x + STEP * vect.x;
	npos.y = cpos.y + STEP * vect.y;

	Quad* nquad = cquad; //guess that the next quad will be the same
	/*check if npos is outside the current quad*/
	if (npos.x < x1 || npos.x > x2 || npos.y < y1 || npos.y > y2)
	{
		// set up local variables
		icVector2 cross_x1, cross_y1, cross_x2, cross_y2;
		double dprod_x1, dprod_y1, dprod_x2, dprod_y2;
		Edge* cross_edge;

		cross_y1 = icVector2(x0 + ((y1 - y0) / vect.y) * vect.x, y1);
		cross_y2 = icVector2(x0 + ((y2 - y0) / vect.y) * vect.x, y2);
		cross_x1 = icVector2(x1, y0 + ((x1 - x0) / vect.x) * vect.y);
		cross_x2 = icVector2(x2, y0 + ((x2 - x0) / vect.x) * vect.y);

		dprod_y1 = dot(vect, cross_y1 - cpos);
		dprod_y2 = dot(vect, cross_y2 - cpos);
		dprod_x1 = dot(vect, cross_x1 - cpos);
		dprod_x2 = dot(vect, cross_x2 - cpos);
		// check y1
		if (cross_y1.x >= x1 && cross_y1.x <= x2 && dprod_y1 > 0 ) {
			npos = cross_y1;
			cross_edge = poly->find_edge(v11, v21);
			nquad = poly->other_quad(cross_edge, cquad);
		}
		//check y2
		else if (cross_y2.x >= x1 && cross_y2.x <= x2 && dprod_y2 > 0) {
			npos = cross_y2;
			cross_edge = poly->find_edge(v12, v22);
			nquad = poly->other_quad(cross_edge, cquad);
		}
		//check x1
		else if (cross_x1.y >= y1 && cross_x1.y <= y2 && dprod_x1 > 0) {
			npos = cross_x1;
			cross_edge = poly->find_edge(v11, v12);
			nquad = poly->other_quad(cross_edge, cquad);
		}
		//check x2
		else if (cross_x2.y >= y1 && cross_x2.y <= y2 && dprod_x2 > 0) {
			npos = cross_x2;
			cross_edge = poly->find_edge(v21, v22);
			nquad = poly->other_quad(cross_edge, cquad);
		}
		// none of the crossing points meets the conditions
		else {
			nquad = poly->find_quad(npos.x, npos.y);
		}

		double proximity = sing_prox(npos);
		if (proximity < STEP) {
			nquad = NULL;
		}
	}
	return nquad;
}

// takes x and y coordinates of a seed point and draws a streamline through that point
std::vector<icVector2> build_streamline(const double x, const double y)
{
	std::vector<icVector2> new_streamline_points;
	new_streamline_points.clear();
	{
		Quad* cquad = poly->find_quad(x, y);
		if (cquad == NULL)
			// this means given x,y doesn't have a cooresponding quad
			return new_streamline_points;
		icVector2 cpos = icVector2(x, y);
		// save npos (new position) into current_streamline_points
		new_streamline_points.push_back(cpos);
		icVector2 npos;
		int step_counter = 0;
		PolyLine pline;
		while (cquad != NULL && step_counter < STEP_MAX)
		{
			cquad = streamline_step(cpos, npos, cquad, true);
			if (!is_seed_point_valid(npos, d_test))
				break;
			LineSegment linear_seg = LineSegment(cpos.x, cpos.y, 0, npos.x, npos.y, 0);
			pline.push_back(linear_seg);
			cpos = npos;
			// save npos (new position) into new_streamline_points
			new_streamline_points.push_back(npos);
			step_counter++;
		}
		streamlines.push_back(pline);
	}

	{
		Quad* cquad = poly->find_quad(x, y);
		if (cquad == NULL)
			// this means given x,y doesn't have a cooresponding quad
			return new_streamline_points;
		icVector2 cpos = icVector2(x, y);
		icVector2 npos;
		int step_counter = 0;
		PolyLine pline;
		while (cquad != NULL && step_counter < STEP_MAX)
		{
			cquad = streamline_step(cpos, npos, cquad, false);
			if (!is_seed_point_valid(npos, d_test))
				break;
			LineSegment linear_seg = LineSegment(cpos.x, cpos.y, 0, npos.x, npos.y, 0);
			pline.push_back(linear_seg);
			cpos = npos;
			// save npos (new position) into new_streamline_points
			new_streamline_points.push_back(npos);
			step_counter++;
		}
		streamlines.push_back(pline);
	}
	return new_streamline_points;
}


Vertex* find_vertex(double xx, double yy) {
	for (int i = 0; i < poly->nverts; i++) {
		Vertex* v = poly->vlist[i];
		if (v->x == xx && v->y == yy) {
			return v;
		}
	}
	return NULL;
}

bool is_seed_point_valid(const icVector2 point, const float min_d) {
	// 1. To check if a seed point is valid, we need to check if it exists in the polyhedron,
	//		we can check this by checking if it exists in the vlist.
	// 2. We also need to make sure the seed point has to be larger than D_sep far away from any other streamlines.
	//		We can check this by checking it's distance to all the intermediate points of all streamlines is smaller than D_sep. (I think this is going to take a lot of time)
	Quad* qtemp = poly->find_quad(point.x, point.y);
	if (qtemp == NULL) return false;

	for (int i = 0; i < all_steamline_points.size(); ++i) {
		icVector2 ptemp = all_steamline_points[i];
		float cur_distance = length(point - ptemp);
		if (cur_distance < min_d)
			return false;
	}

	for (int i = 0; i < current_streamline_points.size(); ++i) {
		icVector2 ptemp = current_streamline_points[i];
		float cur_distance = length(point - ptemp);
		if (cur_distance < min_d)
			return false;
	}

	// check if the point is too close to the points of streamlines in the queue
	// create a temp queue that has the same info as queue
	std::queue<std::vector<icVector2>> temp_q = queue;
	while(!temp_q.empty()) {
		// get one vector out of the queue
		std::vector<icVector2> streamline_points = temp_q.front();
		temp_q.pop();
		for (int j = 0; j < streamline_points.size(); ++j) {
			icVector2 ptemp = streamline_points[j];
			float cur_distance = length(point - ptemp);
			if (cur_distance < min_d)
				return false;
		}
	}
	return true;
}

// given a point and vector, find its +90 degree position
icVector2 select_candidate_seed_point_clockwise(const float x, const float y, const float vx, const float vy) {
	float new_vx = -vy;
	float new_vy = vx;
	float temp = sqrt(vy * vy + vx * vx); //distance
	temp = temp / d_sep; // get ratio 
	// increase or decrease vect
	new_vx /= temp;
	new_vy /= temp;
	float new_x = x - new_vx;
	float new_y = y - new_vy;
	return icVector2(new_x, new_y);
}

// given a point and vector, find its -90 degree position
icVector2 select_candidate_seed_point_counterclockwise(const float x, const float y, const float vx, const float vy) {
	float new_vx = vy;
	float new_vy = -vx;
	float temp = sqrt(vy * vy + vx * vx); //distance
	temp = temp / d_sep; // get ratio 
	// increase or decrease vect
	new_vx /= temp;
	new_vy /= temp;
	float new_x = x - new_vx;
	float new_y = y - new_vy;
	return icVector2(new_x, new_y);
}

icVector2 calculate_vector(icVector2 point) {
	Quad* cquad = poly->find_quad(point.x, point.y);
	double x1, y1, x2, y2, f11, f12, f21, f22, g11, g21, g12, g22;
	Vertex* v11, * v12, * v21, * v22;

	x1 = poly->smallest_x(cquad);
	x2 = poly->largest_x(cquad);
	y1 = poly->smallest_y(cquad);
	y2 = poly->largest_y(cquad);

	v11 = find_vertex(x1, y1);
	f11 = v11->vx;
	g11 = v11->vy;

	v12 = find_vertex(x1, y2);
	f12 = v12->vx;
	g12 = v12->vy;

	v21 = find_vertex(x2, y1);
	f21 = v21->vx;
	g21 = v21->vy;

	v22 = find_vertex(x2, y2);
	f22 = v22->vx;
	g22 = v22->vy;

	double x0 = point.x;
	double y0 = point.y;
	icVector2 vect;
	double m1 = (x2 - x0) * (y2 - y0) / (x2 - x1) / (y2 - y1);
	double m2 = (x0 - x1) * (y2 - y0) / (x2 - x1) / (y2 - y1);
	double m3 = (x2 - x0) * (y0 - y1) / (x2 - x1) / (y2 - y1);
	double m4 = (x0 - x1) * (y0 - y1) / (x2 - x1) / (y2 - y1);
	vect.x = m1 * f11 + m2 * f21 + m3 * f12 + m4 * f22;
	vect.y = m1 * g11 + m2 * g21 + m3 * g12 + m4 * g22;
	normalize(vect);
	return vect;
}

void evenly_spaced_algorithm() {
	// original phase: compute an inital streamline and put it into the queue
	// updated: compute an inital streamline, put its points vector into the queue
	
	// compute inital streamline and its points (saved in current_streamline_points)
	// set current_streamline_points to be the inital streamline
	// meanwhile, it will generate lines for inital streamline to draw on the screen
	current_streamline_points = build_streamline(initial_x, initial_y);

	bool finished = false;
	while (finished == false) {
		// select a candidate seed point at d = d_sep apart from the current streamline
		// firstly, seclect a sample point for the streamline
		icVector2 point;
		icVector2 vet;
		icVector2 candidate_point_clockwise;
		icVector2 candidate_point_counterclockwise;
		PolyLine pline;
		for (int i = 0; i < current_streamline_points.size(); ++i) {
			point = current_streamline_points[i];
			// calculate this point's vx, vy
			vet = calculate_vector(point);
			// calculate two candidate points for the sample point
			candidate_point_clockwise = select_candidate_seed_point_clockwise(point.x, point.y, vet.x, vet.y);
			if ( is_seed_point_valid(candidate_point_clockwise, d_sep) ) {
				// if a valid candidate has been selected 
				// then compute a new streamline and put it into the queue
				std::vector<icVector2> temp = build_streamline(candidate_point_clockwise.x, candidate_point_clockwise.y);
				queue.push(temp);

				// code for user to see how the streamlines are generated
				if (traceOn) {
					tracing_points.push_back(candidate_point_clockwise);
					LineSegment linear_seg = LineSegment(point.x, point.y, 0, candidate_point_clockwise.x, candidate_point_clockwise.y, 0);
					pline.push_back(linear_seg);
					tracing_lines.push_back(pline);
				}
			}
			
			candidate_point_counterclockwise = select_candidate_seed_point_counterclockwise(point.x, point.y, vet.x, vet.y);
			if ( is_seed_point_valid(candidate_point_counterclockwise, d_sep) ) {
				// if a valid candidate has been selected 
				// then compute a new streamline and put it into the queue
				std::vector<icVector2> temp = build_streamline(candidate_point_counterclockwise.x, candidate_point_counterclockwise.y);
				queue.push(temp);

				if (traceOn) {
					tracing_points.push_back(candidate_point_counterclockwise);
					LineSegment linear_seg = LineSegment(point.x, point.y, 0, candidate_point_counterclockwise.x, candidate_point_counterclockwise.y, 0);
					pline.push_back(linear_seg);
					tracing_lines.push_back(pline);
				}
			}
		}
		// here we have done using the current_streamline_points, put it into the all_streamline_points
		for (int j = 0; j < current_streamline_points.size(); ++j)
			all_steamline_points.push_back(current_streamline_points[j]);
		// and clear out memory
		current_streamline_points.clear();

		// if there is no more available streamline in the queue
		// set finished to true;
		if(queue.empty())
			finished = true;
		// else let the next streamline in the queue be the current streamline
		else {
			current_streamline_points = queue.front();
			queue.pop();
		}
		
	}
	return;
}