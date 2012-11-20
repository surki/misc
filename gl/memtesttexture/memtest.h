#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <assert.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <GL/glext.h>
#include <GL/glxext.h>
#include <X11/extensions/Xinerama.h>

#include <string>
#include <vector>

struct format {
        const char *name;
        GLenum token;
        const char **extension;
};

const char *FXT1[] = {
        "GL_3DFX_texture_compression_FXT1",
        NULL
};

const char *S3TC[] = {
        "GL_EXT_texture_compression_s3tc",
        NULL
};

const char *S3TC_srgb[] = {
        "GL_EXT_texture_compression_s3tc",
        "GL_EXT_texture_sRGB",
        NULL
};

const char *RGTC[] = {
        "GL_ARB_texture_compression_rgtc",
        NULL
};

const char *RGTC_signed[] = {
        "GL_ARB_texture_compression_rgtc",
        "GL_EXT_texture_snorm",
        NULL
};

extern unsigned int g_usePBO;

void query_print_video_memory()
{
#define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX          0x9047
#define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
#define GPU_MEMORY_INFO_EVICTION_COUNT_NVX            0x904A
#define GPU_MEMORY_INFO_EVICTED_MEMORY_NVX            0x904B

    static GLint s_vidmem = 0, s_totalmem = 0, s_curr_avail_vidmem = 0, s_evict_cnt = 0, s_evict_mem = 0;
    GLint vidmem = 0, totalmem = 0, curr_avail_vidmem = 0, evict_cnt = 0, evict_mem = 0;

    glGetIntegerv(GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &vidmem);
    glGetIntegerv(GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalmem);
    glGetIntegerv(GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &curr_avail_vidmem);
    glGetIntegerv(GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &evict_cnt);
    glGetIntegerv(GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, &evict_mem);

    GLint delta_cnt = abs(evict_cnt - s_evict_cnt);
    GLint delta_mem = abs(evict_mem - s_evict_mem);
    if (delta_cnt > 0)
    {
        /* printf("Total memory available = %d\n", totalmem); */
        /* printf("Dedicated video memory = %d\n", vidmem); */
        /* printf("Currently available dedicated video memory = %d\n", curr_avail_vidmem); */
        printf("eviction count = %d eviction memory size = %d\n", delta_cnt, delta_mem);
    }

    s_vidmem            = vidmem;
    s_totalmem          = totalmem;
    s_curr_avail_vidmem = curr_avail_vidmem;
    s_evict_cnt         = evict_cnt;
    s_evict_mem         = evict_mem;
}

void  gl_debug_msg_proc_arb(GLuint source,
                            GLuint type,
                            GLuint id,
                            GLuint severity,
                            int length,
                            const char* message,
                            void* userParam)
{
    char debSource[36], debType[38], debSev[29];

#define CASE(s, er) case er: strcpy(s, #er); break;
    switch(source) {
        CASE(debSource, GL_DEBUG_SOURCE_API_ARB);
        CASE(debSource, GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB);
        CASE(debSource, GL_DEBUG_SOURCE_SHADER_COMPILER_ARB);
        CASE(debSource, GL_DEBUG_SOURCE_THIRD_PARTY_ARB);
        CASE(debSource, GL_DEBUG_SOURCE_APPLICATION_ARB);
        CASE(debSource, GL_DEBUG_SOURCE_OTHER_ARB);
    }

    switch(type) {
        CASE(debType, GL_DEBUG_TYPE_ERROR_ARB);
        CASE(debType, GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB);
        CASE(debType, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB);
        CASE(debType, GL_DEBUG_TYPE_PORTABILITY_ARB);
        CASE(debType, GL_DEBUG_TYPE_PERFORMANCE_ARB);
        CASE(debType, GL_DEBUG_TYPE_OTHER_ARB);
    }

    switch(severity) {
        CASE(debSev, GL_DEBUG_SEVERITY_HIGH_ARB);
        CASE(debSev, GL_DEBUG_SEVERITY_MEDIUM_ARB);
        CASE(debSev, GL_DEBUG_SEVERITY_LOW_ARB);
    }
#undef CASE

    printf("%s, %s, %d, %s, %s\n", debSource, debType, id, debSev, message);
}

bool
piglit_get_compressed_block_size(GLenum format,
                                 unsigned *bw, unsigned *bh, unsigned *bytes)
{
        switch (format) {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                *bw = *bh = 4;
                *bytes = 8;
                return true;
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RED_RGTC1:
        case GL_COMPRESSED_SIGNED_RED_RGTC1:
        case GL_COMPRESSED_LUMINANCE_LATC1_EXT:
        case GL_COMPRESSED_SIGNED_LUMINANCE_LATC1_EXT:
                *bw = *bh = 4;
                *bytes = 8;
                return true;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_RG_RGTC2:
        case GL_COMPRESSED_SIGNED_RG_RGTC2:
        case GL_COMPRESSED_LUMINANCE_ALPHA_LATC2_EXT:
        case GL_COMPRESSED_SIGNED_LUMINANCE_ALPHA_LATC2_EXT:
                *bw = *bh = 4;
                *bytes = 16;
                return true;
        case GL_COMPRESSED_RGB_FXT1_3DFX:
        case GL_COMPRESSED_RGBA_FXT1_3DFX:
                *bw = 8;
                *bh = 4;
                *bytes = 16;
                return true;
        default:
                /* return something rather than uninitialized values */
                *bw = *bh = *bytes = 1;
                return false;
        }
}

double get_time_now()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return ((double)now.tv_sec + ((double)now.tv_usec) * 0.000001);
}

/**
 * Generates an image of the given size with quadrants of red, green,
 * blue and white.
 * Note that for compressed teximages, where the blocking would be
 * problematic, we assign the whole layers at w == 4 to red, w == 2 to
 * green, and w == 1 to blue.
 *
 * \param internalFormat  either GL_RGBA or a specific compressed format
 * \param w  the width in texels
 * \param h  the height in texels
 * \param alpha  if TRUE, use varied alpha values, else all alphas = 1
 * \param basetype  either GL_UNSIGNED_NORMALIZED, GL_SIGNED_NORMALIZED
 *                  or GL_FLOAT
 */
GLfloat *
piglit_rgbw_image(GLenum internalFormat, int w, int h,
                  GLboolean alpha, GLenum basetype)
{
        float red[4]   = {1.0, 0.0, 0.0, 0.0};
        float green[4] = {0.0, 1.0, 0.0, 0.25};
        float blue[4]  = {0.0, 0.0, 1.0, 0.5};
        float white[4] = {1.0, 1.0, 1.0, 1.0};
        GLfloat *data;
        int x, y;

        if (!alpha) {
                red[3] = 1.0;
                green[3] = 1.0;
                blue[3] = 1.0;
                white[3] = 1.0;
        }

        switch (basetype) {
        case GL_UNSIGNED_NORMALIZED:
                break;

        case GL_SIGNED_NORMALIZED:
                for (x = 0; x < 4; x++) {
                        red[x] = red[x] * 2 - 1;
                        green[x] = green[x] * 2 - 1;
                        blue[x] = blue[x] * 2 - 1;
                        white[x] = white[x] * 2 - 1;
                }
                break;

        case GL_FLOAT:
                for (x = 0; x < 4; x++) {
                        red[x] = red[x] * 10 - 5;
                        green[x] = green[x] * 10 - 5;
                        blue[x] = blue[x] * 10 - 5;
                        white[x] = white[x] * 10 - 5;
                }
                break;

        default:
                assert(0);
        }

        data = (GLfloat *)malloc(w * h * 4 * sizeof(GLfloat));

        for (y = 0; y < h; y++) {
                for (x = 0; x < w; x++) {
                        const int size = w > h ? w : h;
                        const float *color;

                        if (x < w / 2 && y < h / 2)
                                color = red;
                        else if (y < h / 2)
                                color = green;
                        else if (x < w / 2)
                                color = blue;
                        else
                                color = white;

                        switch (internalFormat) {
                        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
                        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
                        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                        case GL_COMPRESSED_RGB_FXT1_3DFX:
                        case GL_COMPRESSED_RGBA_FXT1_3DFX:
                        case GL_COMPRESSED_RED_RGTC1:
                        case GL_COMPRESSED_SIGNED_RED_RGTC1:
                        case GL_COMPRESSED_RG_RGTC2:
                        case GL_COMPRESSED_SIGNED_RG_RGTC2:
                        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
                        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
                        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
                        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
                                if (size == 4)
                                        color = red;
                                else if (size == 2)
                                        color = green;
                                else if (size == 1)
                                        color = blue;
                                break;
                        default:
                                break;
                        }

                        memcpy(data + (y * w + x) * 4, color,
                               4 * sizeof(float));
                }
        }

        return data;
}

GLuint
piglit_rgbw_texture(GLenum internalFormat, int w, int h, GLboolean mip,
                    GLboolean alpha, GLenum basetype)
{
        int size, level;
        GLuint tex;

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (mip) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_NEAREST);
        } else {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_NEAREST);
        }

        for (level = 0, size = w > h ? w : h; size > 0; level++, size >>= 1) {
                GLfloat *data =
                        piglit_rgbw_image(internalFormat, w, h,
                                          alpha, basetype);

                glTexImage2D(GL_TEXTURE_2D, level,
                             internalFormat,
                             w, h, 0,
                             GL_RGBA, GL_FLOAT, data);

                free(data);

                if (!mip)
                        break;

                if (w > 1)
                        w >>= 1;
                if (h > 1)
                        h >>= 1;
        }

        return tex;
}

/**
 * Compute size (in bytes) neede to store an image in the given compressed
 * format.
 */
unsigned
piglit_compressed_image_size(GLenum format, unsigned width, unsigned height)
{
        unsigned bw, bh, bytes;
        bool b = piglit_get_compressed_block_size(format, &bw, &bh, &bytes);
        assert(b);
        return ((width + bw - 1) / bw) * ((height + bh - 1) / bh) * bytes;
}

/**
 * Generate a checkerboard texture
 *
 * \param tex                Name of the texture to be used.  If \c tex is
 *                           zero, a new texture name will be generated.
 * \param level              Mipmap level the checkerboard should be written to
 * \param width              Width of the texture image
 * \param height             Height of the texture image
 * \param horiz_square_size  Size of each checkerboard tile along the X axis
 * \param vert_square_size   Size of each checkerboard tile along the Y axis
 * \param black              RGBA color to be used for "black" tiles
 * \param white              RGBA color to be used for "white" tiles
 *
 * A texture with alternating black and white squares in a checkerboard
 * pattern is generated.  The texture data is written to LOD \c level of
 * the texture \c tex.
 *
 * If \c tex is zero, a new texture created.  This texture will have several
 * texture parameters set to non-default values:
 *
 *  - S and T wrap modes will be set to \c GL_CLAMP_TO_BORDER.
 *  - Border color will be set to { 1.0, 0.0, 0.0, 1.0 }.
 *  - Min and mag filter will be set to \c GL_NEAREST.
 *
 * \return
 * Name of the texture.  In addition, this texture will be bound to the
 * \c GL_TEXTURE_2D target of the currently active texture unit.
 */
GLuint
piglit_checkerboard_texture(GLuint tex, unsigned level,
                            unsigned width, unsigned height,
                            unsigned horiz_square_size,
                            unsigned vert_square_size,
                            const float *black, const float *white)
{
        static const GLfloat border_color[4] = { 1.0, 0.0, 0.0, 1.0 };
        unsigned i;
        unsigned j;

        float *const tex_data = (float *)malloc(width * height * (4 * sizeof(float)));
        float *texel = tex_data;

        for (i = 0; i < height; i++) {
                const unsigned row = i / vert_square_size;

                for (j = 0; j < width; j++) {
                        const unsigned col = j / horiz_square_size;

                        if ((row ^ col) & 1) {
                                memcpy(texel, white, 4 * sizeof(float));
                        } else {
                                memcpy(texel, black, 4 * sizeof(float));
                        }

                        texel += 4;
                }
        }

        if (tex == 0) {
                glGenTextures(1, &tex);

                glBindTexture(GL_TEXTURE_2D, tex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_LINEAR);

                /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, */
                /*              GL_NEAREST); */
                /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, */
                /*              GL_NEAREST); */
                /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, */
                /*              GL_CLAMP_TO_BORDER); */
                /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, */
                /*              GL_CLAMP_TO_BORDER); */
                /* glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, */
                /*               border_color); */
        } else {
                glBindTexture(GL_TEXTURE_2D, tex);
        }

        if (g_usePBO)
        {
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

                static GLuint ioBuf[1] = { 0 };
                if (ioBuf[0] == 0)
                {
                    glGenBuffers(sizeof(ioBuf)/sizeof(GLuint), ioBuf);
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ioBuf[0]);
                    glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, width * height * (4 * sizeof(float)),
                                 NULL, GL_STREAM_DRAW);
                }
                else
                {
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ioBuf[0]);
                }

                glBufferSubData(GL_PIXEL_UNPACK_BUFFER_ARB, 0, width * height * (4 * sizeof(float)),
                                tex_data);

                /* void* ioMem = glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY); */
                /* assert(ioMem);  */
                /* memcpy(ioMem, tex_data, width * height * (4 * sizeof(float))); */
                /* glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB); */

                glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, 0, GL_RGBA,
                             GL_FLOAT, BUFFER_OFFSET(0));
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
        }
        else
        {
                glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, 0, GL_RGBA,
                             GL_FLOAT, tex_data);
        }

        free(tex_data);

        return tex;
}

typedef unsigned char uint8;
typedef signed short int16;
typedef signed int int32;
typedef unsigned int uint32;

typedef struct _DDAHEADER
{
    char HeaderID[4];                       // Contains "DDSA" if valid DDS animation file

    int16 VersionNumber;             // 1 for now

    int16 AnimHeaderSize;           // Number of bytes in AnimationHeader
    int16 AnimationImages;           // Number of DDS images in animation

    int16 DDSChunkHeaderSize;        // sizeof(DDACHUNKHEADER)

    uint32 width;                    // Dimensions of animation
    uint32 height;
} DDAHEADER;

typedef struct _DDACHUNKHEADER
{
    uint32 NextImageHeaderOffset;    // Offset in bytes from end of this header structure to the beginning of next image header structure

    int32 ImageNumber;               // Image index number 1 for 1st, 2 for 2nd. etc.

    int32 DDSImageSize;              // Image size in bytes of raw DDS file to follow this header
} DDACHUNKHEADER;


typedef uint8 BYTE;
typedef uint32 DWORD_;

typedef struct tagDDPIXELFORMAT {
        DWORD_ dwSize;   // size of this structure (must be 32)
        DWORD_ dwFlags;  // see DDPF_*
        DWORD_ dwFourCC;
        DWORD_ dwRGBBitCount;    // Total number of bits for RGB formats
        DWORD_ dwRBitMask;
        DWORD_ dwGBitMask;
        DWORD_ dwBBitMask;
        DWORD_ dwRGBAlphaBitMask;
} DDPIXELFORMAT;

typedef struct tagDDCAPS2 {
        DWORD_ dwCaps1;  // Zero or more of the DDSCAPS_* members
        DWORD_ dwCaps2;  // Zero or more of the DDSCAPS2_* members
        DWORD_ dwReserved[2];
} DDCAPS2;

typedef struct tagDDSURFACEDESC2 {
        DWORD_ dwSize;   // size of this structure (must be 124)
        DWORD_ dwFlags;  // combination of the DDSS_* flags
        DWORD_ dwHeight;
        DWORD_ dwWidth;
        DWORD_ dwPitchOrLinearSize;
        DWORD_ dwDepth;  // Depth of a volume texture
        DWORD_ dwMipMapCount;
        DWORD_ dwReserved1[11];
        DDPIXELFORMAT ddpfPixelFormat;
        DDCAPS2 ddsCaps;
        DWORD_ dwReserved2;
} DDSURFACEDESC2;

#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
        ((DWORD_)(BYTE)(ch0) | ((DWORD_)(BYTE)(ch1) << 8) |   \
    ((DWORD_)(BYTE)(ch2) << 16) | ((DWORD_)(BYTE)(ch3) << 24 ))

#define FOURCC_DXT1     MAKEFOURCC('D','X','T','1')
#define FOURCC_DXT2     MAKEFOURCC('D','X','T','2')
#define FOURCC_DXT3     MAKEFOURCC('D','X','T','3')
#define FOURCC_DXT4     MAKEFOURCC('D','X','T','4')
#define FOURCC_DXT5     MAKEFOURCC('D','X','T','5')

GLuint load_dds_file(const std::string imgPath)
{
    FILE *fp;

    /* try to open the file */
    fp = fopen(imgPath.c_str(), "rb");
    if (fp == NULL)
        return 0;

    /* verify the type of file */
    char filecode[4];
    fread(filecode, 1, 4, fp);
    if (strncmp(filecode, "DDS ", 4) != 0) {
        fclose(fp);
        return 0;
    }

    /* get the surface desc */
    DDSURFACEDESC2 ddSurfaceInfo;
    memset(&ddSurfaceInfo, 0, sizeof(ddSurfaceInfo));
    fread(&ddSurfaceInfo, sizeof(ddSurfaceInfo), 1, fp);

    unsigned char * buffer;
    unsigned int bufsize;

    /* how big is it going to be including all mipmaps? */
    bufsize = ddSurfaceInfo.dwMipMapCount > 1 ?
        ddSurfaceInfo.dwPitchOrLinearSize * 2 :
        ddSurfaceInfo.dwPitchOrLinearSize;
    buffer = (unsigned char*)malloc(bufsize * sizeof(unsigned char));
    assert(buffer);
    memset(buffer, 0, bufsize * sizeof(unsigned char));
    fread(buffer, 1, bufsize, fp);

    /* close the file pointer */
    fclose(fp);

        // unsigned int components  = (ddSurfaceInfo.fourCC == FOURCC_DXT1) ? 3 : 4;
    unsigned int format;

    switch(ddSurfaceInfo.ddpfPixelFormat.dwFourCC)
    {
    case FOURCC_DXT1:
        format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        break;
    case FOURCC_DXT3:
        format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;
    case FOURCC_DXT5:
        format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        break;
    default:
        return 0;
    }// Create one OpenGL texture

    GLuint textureID;
    glGenTextures(1, &textureID);

    // "Bind" the newly created texture : all future texture functions will modify this texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set the appropriate texture parameters.  These parameters are saved
    // with the currently bound texture.  They will automatically be used
    // when the texture is re-bound.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    unsigned int blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
    unsigned int offset = 0;

    // For now we just care about one mipmap
    ddSurfaceInfo.dwMipMapCount = 0;

    /* load the mipmaps */
    for (unsigned int level = 0; level <= ddSurfaceInfo.dwMipMapCount &&
             (ddSurfaceInfo.dwWidth || ddSurfaceInfo.dwHeight); ++level)
    {
        unsigned int size = ((ddSurfaceInfo.dwWidth+3)/4)*((ddSurfaceInfo.dwHeight+3)/4)*blockSize;
        /* printf("width = %d height = %d mipMapCount=%d size=%d\n", */
        /*        ddSurfaceInfo.dwWidth, ddSurfaceInfo.dwHeight, ddSurfaceInfo.dwMipMapCount, size); */

        if (g_usePBO)
        {
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

                static GLuint ioBuf[1] = { 0 };
                if (ioBuf[0] == 0)
                {
                    glGenBuffers(sizeof(ioBuf)/sizeof(GLuint), ioBuf);
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ioBuf[0]);
                    glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, size, NULL, GL_STREAM_DRAW);
                }
                else
                {
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ioBuf[0]);
                }

                glBufferSubData(GL_PIXEL_UNPACK_BUFFER_ARB, 0, size,
                                buffer + offset);

                /* void* ioMem = glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY); */
                /* assert(ioMem); */
                /* memcpy(ioMem, buffer + offset, size); */
                /* glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB); */

                glCompressedTexImage2D(GL_TEXTURE_2D, level, format, ddSurfaceInfo.dwWidth, ddSurfaceInfo.dwHeight,
                                       0, size, BUFFER_OFFSET(0));
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
        }
        else
        {
            glCompressedTexImage2D(GL_TEXTURE_2D, level, format, ddSurfaceInfo.dwWidth, ddSurfaceInfo.dwHeight,
                                   0, size, buffer + offset);
        }

        offset += size;
        ddSurfaceInfo.dwWidth  /= 2;
        ddSurfaceInfo.dwHeight /= 2;
    }

    free(buffer);
    assert(textureID && "Invalid DDS file, textureID is zero");

    return textureID;
}

// ====================

typedef unsigned char BYTE;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef long LONG;

bool SaveMemoryToBMP( const char* filename, BYTE* pPixelsBGR, int real_width, int real_height,
                      int width_of_buffer, int height_of_buffer, bool flip_at_y);

void SaveTextureIntoBmpFile(char *filename, unsigned int nID, unsigned width, unsigned height)
{
    glBindTexture(GL_TEXTURE_2D, nID);
    int sz = width * height * 3; // texture dimension
    BYTE* pPixl = new BYTE [sz];
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR_EXT, GL_UNSIGNED_BYTE, pPixl);

    SaveMemoryToBMP(filename, pPixl, width, height, width, height, false);

    if (pPixl)
        delete [] pPixl;
}

// Definitions stolen from Windows world.

struct BITMAPFILEHEADER
{
  short bfType;
  int bfSize;
  short bfReserved1;
  short bfReserved2;
  int bfOffBits;
} __attribute__ ((packed));

struct BITMAPINFOHEADER
{
    int biSize;
    long biWidth;
    long biHeight;
    WORD  biPlanes;
    WORD  biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG  biXPelsPerMeter;
    LONG  biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} __attribute__ ((packed));

// Compression Types
#ifndef BI_RGB
#define BI_RGB        0  // none
#define BI_RLE8       1  // RLE 8-bit / pixel
#define BI_RLE4       2  // RLE 4-bit / pixel
#define BI_BITFIELDS  3  // Bitfields
#endif

bool SaveMemoryToBMP( const char* filename, BYTE* pPixelsBGR, int real_width, int real_height,
                      int width_of_buffer, int height_of_buffer, bool flip_at_y)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER bi;

    //Make sure its not a NULL filenam
    if(filename == NULL)
        return false;

    FILE *image;
    if((image=fopen(filename, "wb")) == NULL)
        return false;

#define DWORD_24BIT_WIDTHBYTES_ALIGN( width ) ((width*3) + ( width % 4 == 0 ? 0 : width % 2 == 0 ? 2 : (width+1) % 4 == 0 ? 3 : 1 ));

    // Buffer sizes
    int Height, WidthBytes;//, Pitch;
    Height = real_height;
    WidthBytes = DWORD_24BIT_WIDTHBYTES_ALIGN( real_width );

    // Write the file header
    ((char *)&(fh . bfType))[0] = 'B';
    ((char *)&(fh . bfType))[1] = 'M';
    fh . bfSize = (long)(sizeof (BITMAPINFOHEADER)+sizeof (BITMAPFILEHEADER)+WidthBytes*Height); //Size in BYTES
    fh . bfReserved1 = 0;
    fh . bfReserved2 = 0;
    fh . bfOffBits = sizeof (BITMAPINFOHEADER)+sizeof (BITMAPFILEHEADER);
    bi . biSize = sizeof (BITMAPINFOHEADER);
    bi . biWidth =real_width;
    bi . biHeight =real_height;
    bi . biPlanes = 1;
    bi . biBitCount = 24;
    bi . biCompression = BI_RGB;
    bi . biSizeImage = 0;
    bi . biXPelsPerMeter = 10000;
    bi . biYPelsPerMeter = 10000;
    bi . biClrUsed = 0;
    bi . biClrImportant = 0;

    fwrite((void *) &fh, sizeof(BITMAPFILEHEADER), 1, image);
    fwrite((void *) &bi, sizeof(BITMAPINFOHEADER), 1, image);

    BYTE* txdata = new BYTE[WidthBytes*Height + 4];

    for(int y = 0; y<real_height; ++y)
    {
        for(int x=0; x<real_width; ++x)
        {
            int ps, pd;
            ps = x*3 + (Height-y-1)*WidthBytes; // inverse Y coord for BMP format
            // ps = x + y*Width; // inverse Y coord for BMP format
            if(flip_at_y)
                pd = x + y*width_of_buffer;
            else
                pd = x + ((real_height-height_of_buffer)+height_of_buffer-y-1)*width_of_buffer;

            txdata[ps  ] = pPixelsBGR[pd*3  ];
            txdata[ps+1] = pPixelsBGR[pd*3+1];
            txdata[ps+2] = pPixelsBGR[pd*3+2];
        }
    }

    fwrite((void *) txdata, WidthBytes*Height, 1, image);

    if (txdata)
        delete[] txdata;

    return true;
}
