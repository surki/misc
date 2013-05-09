#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>

Display *gDisplay = NULL;
Window gWindow = 0;
int gWidth = 0, gHeight = 0;
Atom gDelAtom = None;

void LogFBConfig(GLXFBConfig &config)
{
    int n = 0;

    glXGetFBConfigAttrib(gDisplay, config, GLX_FBCONFIG_ID, &n);
    printf("FBConfig ID=%d\n", n);

    glXGetFBConfigAttrib(gDisplay, config, GLX_BUFFER_SIZE, &n);
    printf("    GLX_BUFFER_SIZE=%d\n", n);

    glXGetFBConfigAttrib(gDisplay, config, GLX_DOUBLEBUFFER, &n);
    printf("    GLX_DOUBLEBUFFER=%s\n", n == True ? "TRUE" : "FALSE");

    glXGetFBConfigAttrib(gDisplay, config, GLX_RED_SIZE, &n);
    printf("    GLX_RED_SIZE=%d\n", n);

    glXGetFBConfigAttrib(gDisplay, config, GLX_GREEN_SIZE, &n);
    printf("    GLX_GREEN_SIZE=%d\n", n);

    glXGetFBConfigAttrib(gDisplay, config, GLX_BLUE_SIZE, &n);
    printf("    GLX_BLUE_SIZE=%d\n", n);

    glXGetFBConfigAttrib(gDisplay, config, GLX_ALPHA_SIZE, &n);
    printf("    GLX_ALPHA_SIZE=%d\n", n);

    glXGetFBConfigAttrib(gDisplay, config, GLX_DEPTH_SIZE, &n);
    printf("    GLX_DEPTH_SIZE=%d\n", n);

    glXGetFBConfigAttrib(gDisplay, config, GLX_STENCIL_SIZE, &n);
    printf("    GLX_STENCIL_SIZE=%d\n", n);

    glXGetFBConfigAttrib(gDisplay, config, GLX_CONFIG_CAVEAT, &n);
    printf("    GLX_CONFIG_CAVEAT=");
    switch(n)
    {
    case GLX_NONE:
	printf("NONE\n");
	break;
    case GLX_SLOW_CONFIG:
	printf("SLOW\n");
	break;
    case GLX_NON_CONFORMANT_CONFIG:
	printf("NON-CONFORMANT\n");
	break;
    }
}

void CreateWindowAndGLContext()
{
    gDisplay = XOpenDisplay(NULL);
    if (!gDisplay)
    {
	fprintf(stderr, "Couldn't connect to X server\n");
	exit(EXIT_FAILURE);
    }

    // Make sure we have the RENDER extension
    int render_event_base, render_error_base;
    if(!XRenderQueryExtension(gDisplay, &render_event_base, &render_error_base))
    {
        fprintf(stderr, "No RENDER extension found\n");
        exit(EXIT_FAILURE);
    }

    // Get the list of FBConfigs that match our attribute criteria
    int numfbconfigs = 0;
    int attrList[] =
	{
	    GLX_RENDER_TYPE, GLX_RGBA_BIT,
	    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
	    GLX_DOUBLEBUFFER, True,
	    GLX_RED_SIZE, 1,
	    GLX_GREEN_SIZE, 1,
	    GLX_BLUE_SIZE, 1,
	    GLX_ALPHA_SIZE, 1,
	    GLX_DEPTH_SIZE, 1,
	    None
	};
    GLXFBConfig *fbconfigs = glXChooseFBConfig(gDisplay,
					       DefaultScreen(gDisplay),
					       attrList,
					       &numfbconfigs);
    if (!fbconfigs)
    {
        fprintf(stderr, "No matching FBConfigs found\n");
        exit(EXIT_FAILURE);
    }

    // Find an FBConfig with a visual that has a RENDER picture format
    // that has alpha
    int chosenFBConfig = -1;
    XVisualInfo *visual = NULL;
    XRenderPictFormat *pictformat = NULL;
    for (int i = 0; i < numfbconfigs; i++)
    {
	visual = (XVisualInfo*)glXGetVisualFromFBConfig(gDisplay, fbconfigs[i]);
	if(!visual)
	    continue;

	pictformat = XRenderFindVisualFormat(gDisplay, visual->visual);
	if(!pictformat)
	    continue;

	chosenFBConfig = i;
	if(pictformat->direct.alphaMask > 0)
	{
	    break;
	}
    }
    if(chosenFBConfig == -1)
    {
        fprintf(stderr, "No matching FBConfig with valid Alpha found\n");
        exit(EXIT_FAILURE);
    }

    // Print the fbconfig info we have chosen
    printf("Chosen ");
    LogFBConfig(fbconfigs[chosenFBConfig]);

    // Create a colormap
    Colormap cmap = XCreateColormap(gDisplay,
				    RootWindow(gDisplay, DefaultScreen(gDisplay)),
				    visual->visual,
				    AllocNone);

    XSetWindowAttributes attr = {0};
    attr.colormap = cmap;
    attr.background_pixmap = None;
    attr.border_pixmap = None;
    attr.border_pixel = 0;
    attr.event_mask =
	StructureNotifyMask |
	EnterWindowMask |
	LeaveWindowMask |
	ExposureMask |
	ButtonPressMask |
	ButtonReleaseMask |
	OwnerGrabButtonMask |
	KeyPressMask |
	KeyReleaseMask;

    // Really? Use XRandR/Xinerama to find the real dimenstions, with
    // multi-monitor everywhere and all.
    gWidth = DisplayWidth(gDisplay, DefaultScreen(gDisplay))/2;
    gHeight = DisplayHeight(gDisplay, DefaultScreen(gDisplay))/2;
    gWindow = XCreateWindow(gDisplay,
			    RootWindow(gDisplay, DefaultScreen(gDisplay)),
			    gWidth/2, gHeight/2,
			    gWidth, gHeight,
			    0,
			    visual->depth,
			    InputOutput,
			    visual->visual,
			    CWBackPixmap | CWColormap | CWBorderPixel | CWEventMask,
			    &attr);
    if (!gWindow)
    {
        fprintf(stderr, "Could not create window\n");
        exit(EXIT_FAILURE);
    }

    if ((gDelAtom = XInternAtom(gDisplay, "WM_DELETE_WINDOW", 0)) != None)
    {
	XSetWMProtocols(gDisplay, gWindow, &gDelAtom, 1);
    }

    // TODO Set up WM hints etc
    XMapWindow(gDisplay, gWindow);

    // Create the OpenGL context
    GLXContext  glcontext = glXCreateContext(gDisplay, visual, NULL, True);
    if (!glcontext)
    {
        fprintf(stderr, "GL context creation failed!\n");
        exit(EXIT_FAILURE);
    }

    if (!glXMakeContextCurrent(gDisplay, gWindow, gWindow, glcontext))
    {
        fprintf(stderr, "glXMakeContextCurrent failed\n");
        exit(EXIT_FAILURE);
    }
}

// Draw a cube.
//
// Copied from interweb :)

/*  6----7
    /|   /|
    3----2 |
    | 5--|-4
    |/   |/
    0----1

*/

GLfloat cube_vertices[][8] =  {
    /*  X     Y     Z   Nx   Ny   Nz    S    T */
    {-1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 0.0, 0.0}, // 0
    { 1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 1.0, 0.0}, // 1
    { 1.0,  1.0,  1.0, 0.0, 0.0, 1.0, 1.0, 1.0}, // 2
    {-1.0,  1.0,  1.0, 0.0, 0.0, 1.0, 0.0, 1.0}, // 3

    { 1.0, -1.0, -1.0, 0.0, 0.0, -1.0, 0.0, 0.0}, // 4
    {-1.0, -1.0, -1.0, 0.0, 0.0, -1.0, 1.0, 0.0}, // 5
    {-1.0,  1.0, -1.0, 0.0, 0.0, -1.0, 1.0, 1.0}, // 6
    { 1.0,  1.0, -1.0, 0.0, 0.0, -1.0, 0.0, 1.0}, // 7

    {-1.0, -1.0, -1.0, -1.0, 0.0, 0.0, 0.0, 0.0}, // 5
    {-1.0, -1.0,  1.0, -1.0, 0.0, 0.0, 1.0, 0.0}, // 0
    {-1.0,  1.0,  1.0, -1.0, 0.0, 0.0, 1.0, 1.0}, // 3
    {-1.0,  1.0, -1.0, -1.0, 0.0, 0.0, 0.0, 1.0}, // 6

    { 1.0, -1.0,  1.0,  1.0, 0.0, 0.0, 0.0, 0.0}, // 1
    { 1.0, -1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 0.0}, // 4
    { 1.0,  1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 1.0}, // 7
    { 1.0,  1.0,  1.0,  1.0, 0.0, 0.0, 0.0, 1.0}, // 2

    {-1.0, -1.0, -1.0,  0.0, -1.0, 0.0, 0.0, 0.0}, // 5
    { 1.0, -1.0, -1.0,  0.0, -1.0, 0.0, 1.0, 0.0}, // 4
    { 1.0, -1.0,  1.0,  0.0, -1.0, 0.0, 1.0, 1.0}, // 1
    {-1.0, -1.0,  1.0,  0.0, -1.0, 0.0, 0.0, 1.0}, // 0

    {-1.0, 1.0,  1.0,  0.0,  1.0, 0.0, 0.0, 0.0}, // 3
    { 1.0, 1.0,  1.0,  0.0,  1.0, 0.0, 1.0, 0.0}, // 2
    { 1.0, 1.0, -1.0,  0.0,  1.0, 0.0, 1.0, 1.0}, // 7
    {-1.0, 1.0, -1.0,  0.0,  1.0, 0.0, 0.0, 1.0}, // 6
};

void DrawCube(void)
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer(3, GL_FLOAT, sizeof(GLfloat) * 8, &cube_vertices[0][0]);
    glNormalPointer(GL_FLOAT, sizeof(GLfloat) * 8, &cube_vertices[0][3]);
    glTexCoordPointer(2, GL_FLOAT, sizeof(GLfloat) * 8, &cube_vertices[0][6]);

    glDrawArrays(GL_QUADS, 0, 24);
}

void Draw()
{
    static float a = 0;
    static float b = 0;
    static float c = 0;

    float const light0_dir[] = {0, 1, 0, 0};
    float const light0_color[] = {78./255., 80./255., 184./255.,1};

    float const light1_dir[] = {-1, 1, 1, 0};
    float const light1_color[] = {255./255., 220./255., 97./255.,1};

    float const light2_dir[] = {0, -1, 0, 0};
    float const light2_color[] = {31./255., 75./255., 16./255.,1};

    float const aspect = (float)gWidth / (float)gHeight;

    glDrawBuffer(GL_BACK);

    glViewport(0, 0, gWidth, gHeight);

    // Clear with alpha = 0.0, i.e. full transparency
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-aspect, aspect, -1, 1, 2.5, 10);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLightfv(GL_LIGHT0, GL_POSITION, light0_dir);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_color);

    glLightfv(GL_LIGHT1, GL_POSITION, light1_dir);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_color);

    glLightfv(GL_LIGHT2, GL_POSITION, light2_dir);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_color);

    glTranslatef(0., 0., -5.);

    glRotatef(a, 1, 0, 0);
    glRotatef(b, 0, 1, 0);
    glRotatef(c, 0, 0, 1);

    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHTING);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    glColor4f(1., 1., 1., 0.5);

    glCullFace(GL_FRONT);
    DrawCube();
    glCullFace(GL_BACK);
    DrawCube();

    a = fmod(a+0.1, 360.);
    b = fmod(b+0.5, 360.);
    c = fmod(c+0.25, 360.);

    glXSwapBuffers(gDisplay, gWindow);
}

void MainLoop()
{
    XEvent event;
    XConfigureEvent *xc;

    while (true)
    {
	Draw();

	while (XPending(gDisplay))
	{
	    XNextEvent(gDisplay, &event);
	    switch (event.type)
	    {
	    case ClientMessage:
		if (event.xclient.data.l[0] == gDelAtom)
		{
		    return;
		}
		break;

	    case ConfigureNotify:
		xc = &(event.xconfigure);
		gWidth = xc->width;
		gHeight = xc->height;
		break;
	    }
	}
    }

    return;
}

int main(int argc, char *argv[])
{
    CreateWindowAndGLContext();

    MainLoop();

    return 0;
}

