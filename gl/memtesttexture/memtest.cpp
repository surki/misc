#define GLX_GLXEXT_PROTOTYPES 1
#define GL_GLEXT_PROTOTYPES 1

#include "memtest.h"

using namespace std;

vector<GLuint> g_textures;
vector<GLuint> g_FBOs;
vector<GLuint> g_compressedTextures;

Display *g_pDisplay = NULL;
Window   g_window;
bool     g_bDoubleBuffered = GL_TRUE;
unsigned int g_width = 1360;
unsigned int g_height = 768;
unsigned int g_generateCompressedTexture = 0;
unsigned int g_useFBO = 0;

#define FORMAT(t, ext) { #t, t, ext }
static struct format formats[] = {
        FORMAT(GL_COMPRESSED_RGB_FXT1_3DFX, FXT1),
        FORMAT(GL_COMPRESSED_RGBA_FXT1_3DFX, FXT1),

        FORMAT(GL_COMPRESSED_RGB_S3TC_DXT1_EXT, S3TC),
        FORMAT(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, S3TC),
        FORMAT(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, S3TC),
        FORMAT(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, S3TC),

        FORMAT(GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, S3TC_srgb),
        FORMAT(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, S3TC_srgb),
        FORMAT(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, S3TC_srgb),
        FORMAT(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, S3TC_srgb),

        FORMAT(GL_COMPRESSED_RED_RGTC1_EXT, RGTC),
        FORMAT(GL_COMPRESSED_SIGNED_RED_RGTC1_EXT, RGTC_signed),
        FORMAT(GL_COMPRESSED_RED_GREEN_RGTC2_EXT, RGTC),
        FORMAT(GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT, RGTC_signed),
};

void Init(int argc, char *argv[]);
void Render();
void InitGL();
GLuint GenerateTexture(unsigned int width = 600, unsigned int height = 600, bool mipmap = false);
GLuint GenerateTextureFBO(unsigned int width, unsigned int height, bool mipmap, GLuint *fboOut);
GLuint GenerateCompressedTexture(unsigned int width = 600, unsigned int height = 600,
                                 GLenum format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, bool mipmap = false);
GLuint GenerateCompressedTextureFromFile(string filename,
                                         unsigned int width,
                                         unsigned int height);
void DrawTexture(GLuint texid, GLuint fbo = 0);

int main(int argc, char *argv[])
{
    XEvent event;

    if (argc < 2)
    {
        printf("Usage: %s N1 N2\n"
               "\t N1 = no. of textures \n"
               "\t N2 = no. of compressed textures\n",
               argv[0]);
        exit(1);
    }

    Init(argc, argv);

    while (1)
    {
        while (XPending(g_pDisplay) > 0)
        {
            XNextEvent(g_pDisplay, &event);

            switch( event.type )
            {
            case ConfigureNotify:
                glViewport(0, 0, event.xconfigure.width, event.xconfigure.height);
                break;
            }
        }

        Render();
    }
}

void Init(int argc, char *argv[])
{
    unsigned int noOfTextures, noOfCompressedTextures;
    GLuint texID, fbo;

    InitGL();

    query_print_video_memory();

    assert(argc >= 3);

    char *gen = getenv("COMPRESSED_TEXTURE_GENERATE");
    if (gen && strcmp(gen, "1") == 0)
    {
        g_generateCompressedTexture = 1;
    }

    gen = getenv("USE_FBO");
    if (gen && strcmp(gen, "1") == 0)
    {
        g_useFBO = 1;
    }

    noOfTextures = atoi(argv[1]);
    noOfCompressedTextures = atoi(argv[2]);

    // Create no. of textures.
    for (int i = 0; i < noOfTextures; i++)
    {
        if (g_useFBO)
        {
            texID = GenerateTextureFBO(g_width, g_height, false, &fbo);
            g_FBOs.push_back(fbo);
            g_textures.push_back(texID);
        }
        else
        {
            texID = GenerateTexture(g_width, g_height);
            g_FBOs.push_back(0);
            g_textures.push_back(texID);
        }
    }

    // Create no. of compressed textures.
    for (int i = 0; i < noOfCompressedTextures; i++)
    {
        if (g_generateCompressedTexture)
            texID = GenerateCompressedTexture(g_width, g_height);
        else
            texID = GenerateCompressedTextureFromFile("test.dds", g_width, g_height);

        g_compressedTextures.push_back(texID);
    }
}

void Render()
{
    static unsigned int n1 = 0, n2 = 0;

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    if (g_textures.size())
    {
        int num = random() % g_textures.size();

        for (int i = 0; i <= num; i++)
        {
            int n = random() % g_textures.size();
            DrawTexture(g_textures[n]);
            //SaveTextureIntoBmpFile("/tmp/test.bmp", g_textures[0], g_width, g_height);
        }
    }

    if (g_compressedTextures.size())
    {
        int num = random() % g_compressedTextures.size();

        for (int i = 0; i <= num; i++)
        {
            int n = random() % g_compressedTextures.size();
            DrawTexture(g_compressedTextures[n]);
            //SaveTextureIntoBmpFile("/tmp/test.bmp", g_compressedTextures[0], g_width, g_height);
        }
    }

    glXSwapBuffers( g_pDisplay, g_window );
    query_print_video_memory();

    //usleep(16000);
}

GLuint GenerateCompressedTexture(unsigned int width, unsigned int height,
                                 GLenum format, bool mipmap)
{
        GLuint tex, tex_src;
        bool pass;
        int level;
        unsigned bw, bh, bs;

        // We just generate a simple texture with no mipmap

        piglit_get_compressed_block_size(format, &bw, &bh, &bs);
        glClearColor(0.5, 0.5, 0.5, 0.5);
        glClear(GL_COLOR_BUFFER_BIT);

        tex_src = piglit_rgbw_texture(format, width, height,
                                      GL_FALSE /* mipmap */, GL_FALSE,
                                      GL_UNSIGNED_NORMALIZED);
        glGenTextures(1, &tex);

        for (level = 0; (height >> level) > 0; level++) {
                int w, h;
                int expected_size, size;
                void *compressed;

                w = width >> level;
                h = height >> level;
                expected_size = piglit_compressed_image_size(format, w, h);

                glBindTexture(GL_TEXTURE_2D, tex_src);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, level,
                                         GL_TEXTURE_COMPRESSED_IMAGE_SIZE,
                                         &size);

                int isCompressed = 0;
                glGetTexLevelParameteriv(GL_TEXTURE_2D, level,
                                         GL_TEXTURE_COMPRESSED_ARB,
                                         &isCompressed);
                assert(isCompressed);

                int iFormat = 0;
                glGetTexLevelParameteriv(GL_TEXTURE_2D, level,
                                         GL_TEXTURE_INTERNAL_FORMAT,
                                         &iFormat);
                assert(iFormat == format);

                if (size != expected_size) {
                        fprintf(stderr, "Format %d level %d (%dx%d) size %d "
                                "doesn't match expected size %d\n",
                                format, level, w, h, size, expected_size);
                        exit(1);
                }

                // printf("isCompressed=%d iFormat=%d size=%d DXT5=%d\n",
                //        isCompressed, iFormat, size, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);

                compressed = malloc(size);


                glGetCompressedTexImage(GL_TEXTURE_2D, level, compressed);

                glBindTexture(GL_TEXTURE_2D, tex);
                glCompressedTexImage2D(GL_TEXTURE_2D, level, format,
                                       w, h, 0, size, compressed);
                assert(glGetError() == GL_NO_ERROR);

                {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
                    // glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                }

                free(compressed);

                // No mipmap for now
                if (!mipmap)
                    break;

        }

        glDeleteTextures(1, &tex_src);
        glBindTexture(GL_TEXTURE_2D, 0);

        return tex;
}

GLuint GenerateCompressedTextureFromFile(string filename,
                                         unsigned int width,
                                         unsigned int height)
{
    GLuint texID;

    texID = load_dds_file(filename);
    assert(texID && "Couldn't load DDS file");

    return texID;
}

GLuint GenerateTexture(unsigned int width, unsigned int height, bool mipmap)
{
    GLuint texture;
    const GLfloat red[4] = {1.0, 0.0, 0.0, 0.0};
    const GLfloat blue[4] = {0.0, 0.0, 1.0, 0.0};

    const float color1[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    const float color2[4] = {0.0f, 1.0f, 0.0f, 1.0f};

    texture = piglit_checkerboard_texture(0, 0, width, height, 100, 100,  color1, color2);

    // texture = piglit_rgbw_texture(GL_RGBA, width, height,
    //                               GL_TRUE, GL_FALSE,
    //                               GL_UNSIGNED_NORMALIZED);

    return texture;
}

GLuint GenerateTextureFBO(unsigned int width, unsigned int height, bool mipmap, GLuint *fboOut)
{
    GLuint fbo;
    GLuint texID;
    const float color1[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    const float color2[4] = {0.0f, 1.0f, 0.0f, 1.0f};

    GLfloat *data = piglit_rgbw_image(GL_RGBA, width, height,
                                      GL_TRUE, GL_UNSIGNED_NORMALIZED);

    // Setup our FBO
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);

    // Now setup a texture to render to
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,  width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // And attach it to the FBO so we can render to it
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texID, 0);

    glTexImage2D(GL_TEXTURE_2D, 0,
                 GL_RGBA, width, height, 0,
                 GL_RGBA, GL_FLOAT, data);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);    // Unbind the FBO for now
    glDisable(GL_TEXTURE_2D);

    free(data);

    *fboOut = fbo;

    return texID;
}

void DrawTexture(GLuint texid, GLuint fbo)
{
    if (fbo)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);

    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, texid);
    glBegin(GL_QUADS);
    {
        glTexCoord2f(0.0,0.0) ; glVertex2d(0,0);
        glTexCoord2f(1.0,0.0) ; glVertex2d(g_width,0);
        glTexCoord2f(1.0,1.0) ; glVertex2d(g_width,g_height);
        glTexCoord2f(0.0,1.0) ; glVertex2d(0,g_height);
    }
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisable(GL_TEXTURE_2D);

    if (fbo)
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

void InitGL()
{
    Colormap colorMap;
    XSetWindowAttributes windowAttributes;
    XVisualInfo *visualInfo = NULL;
    GLXContext glxContext;
    int errorBase;
    int eventBase;

    // Open a connection to the X server
    g_pDisplay = XOpenDisplay( NULL );

    if( g_pDisplay == NULL )
    {
        fprintf(stderr, "glxsimple: %s\n", "could not open display");
        exit(1);
    }

    // Make sure OpenGL's GLX extension supported
    if( !glXQueryExtension( g_pDisplay, &errorBase, &eventBase ) )
    {
        fprintf(stderr, "glxsimple: %s\n", "X server has no OpenGL GLX extension");
        exit(1);
    }

    // Find an appropriate visual

    int doubleBufferVisual[]  =
    {
        GLX_RGBA,           // Needs to support OpenGL
        GLX_DEPTH_SIZE, 24, // Needs to support a 16 bit depth buffer
        GLX_DOUBLEBUFFER,   // Needs to support double-buffering
        None                // end of list
    };

    int singleBufferVisual[] =
    {
        GLX_RGBA,           // Needs to support OpenGL
        GLX_DEPTH_SIZE, 16, // Needs to support a 16 bit depth buffer
        None                // end of list
    };

    // Try for the double-bufferd visual first
    visualInfo = glXChooseVisual( g_pDisplay, DefaultScreen(g_pDisplay), doubleBufferVisual );

    if( visualInfo == NULL )
    {
        // If we can't find a double-bufferd visual, try for a single-buffered visual...
        visualInfo = glXChooseVisual( g_pDisplay, DefaultScreen(g_pDisplay), singleBufferVisual );

        if( visualInfo == NULL )
        {
            fprintf(stderr, "glxsimple: %s\n", "no RGB visual with depth buffer");
            exit(1);
        }

        g_bDoubleBuffered = false;
    }

    // Create an OpenGL rendering context
    glxContext = glXCreateContext( g_pDisplay,
                                   visualInfo,
                                   NULL,      // No sharing of display lists
                                   GL_TRUE ); // Direct rendering if possible

    if( glxContext == NULL )
    {
        fprintf(stderr, "glxsimple: %s\n", "could not create rendering context");
        exit(1);
    }

    // Create an X colormap since we're probably not using the default visual
    colorMap = XCreateColormap( g_pDisplay,
                                RootWindow(g_pDisplay, visualInfo->screen),
                                visualInfo->visual,
                                AllocNone );

    windowAttributes.colormap     = colorMap;
    windowAttributes.border_pixel = 0;
    windowAttributes.event_mask   = ExposureMask           |
                                    VisibilityChangeMask   |
                                    KeyPressMask           |
                                    KeyReleaseMask         |
                                    ButtonPressMask        |
                                    ButtonReleaseMask      |
                                    PointerMotionMask      |
                                    StructureNotifyMask    |
                                    SubstructureNotifyMask |
                                    FocusChangeMask;
    if (!g_width && !g_height)
    {
        int num = 0;
        XineramaScreenInfo *scrInfo = XineramaQueryScreens(g_pDisplay, &num);
        if(num && scrInfo)
        {
            g_width = scrInfo->width;
            g_height = scrInfo->height;
        }
    }

    // Create an X window with the selected visual
    g_window = XCreateWindow( g_pDisplay,
                              RootWindow(g_pDisplay, visualInfo->screen),
                              0, 0,     // x/y position of top-left outside corner of the window
                              g_width, g_height, // Width and height of window
                              0,        // Border width
                              visualInfo->depth,
                              InputOutput,
                              visualInfo->visual,
                              CWBorderPixel | CWColormap | CWEventMask,
                              &windowAttributes );

    // Bind the rendering context to the window
    glXMakeCurrent( g_pDisplay, g_window, glxContext );

    // Request the X window to be displayed on the screen
    XMapWindow( g_pDisplay, g_window );

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g_width, 0, g_height, -1, 1);
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
}
