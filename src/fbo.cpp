#include "fbo.h"
#include <SOIL2.h>
#include <iostream>

FBO::~FBO()
{
    clear();
}

void FBO::clear()
{
    glDeleteTextures(3, _buffers);
    glDeleteFramebuffers(1, &_fboId);
}

void FBO::init(int width, int height)
{
    glbinding::aux::enableGetErrorCallback();
    _width = width;
    _height = height;

    //1. Create a framebuffer object, and then bind it

    //2. Generate two texture ids in "_buffers"

    for(int i=0; i<2; ++i){
        //3. Bind the newly created texture

        //4. Allocate GPU memory for this empty texture

        //5. Attach the texture to FBO color attachment point
    }

    //6. Generate a renderbuffer to store depth, and then bind it

    //7. Allocate GPU memory

    //8. Attach the depth buffer to FBO depth attachment point

    //9. Set the list of draw buffers: the color attachment points

    //10. Check FBO status
    checkFBOAttachment();

    //11. Switch back to original framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glbinding::aux::disableGetErrorCallback();
}

void FBO::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, _fboId);
}

void FBO::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FBO::savePNG(const std::string &name, int i)
{
    glBindTexture(GL_TEXTURE_2D, textureId(i));
    GLubyte *data = new GLubyte [4 * _width * _height];
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    unsigned char* raw_data = reinterpret_cast<unsigned char*>(data);

    /*  mirror image vertically */
    for(int j = 0; j*2 < _height; ++j )
    {
        int index1 = j * _width * 4;
        int index2 = (_height - 1 - j) * _width * 4;
        for(int i = _width * 4; i > 0; --i )
        {
            unsigned char temp = raw_data[index1];
            raw_data[index1] = raw_data[index2];
            raw_data[index2] = temp;
            ++index1;
            ++index2;
        }
    }

    SOIL_save_image((DATA_DIR"/" + name).c_str(),SOIL_SAVE_TYPE_PNG,_width,_height,4,raw_data);
    glBindTexture(GL_TEXTURE_2D,0);
}

void FBO::checkFBOAttachment()
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch(status) {
        case GL_FRAMEBUFFER_COMPLETE:
            break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        std::cout << "An attachment could not be bound to frame buffer object!" << std::endl;
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        std::cout << "Attachments are missing! At least one image (texture) must be bound to the frame buffer object!" << std::endl;
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        std::cout << "A Draw buffer is incomplete or undefinied. All draw buffers must specify attachment points that have images attached." << std::endl;
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        std::cout << "A Read buffer is incomplete or undefinied. All read buffers must specify attachment points that have images attached." << std::endl;
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        std::cout << "All images must have the same number of multisample samples." << std::endl;
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS :
        std::cout << "If a layered image is attached to one attachment, then all attachments must be layered attachments. The attached layers do not have to have the same number of layers, nor do the layers have to come from the same kind of texture." << std::endl;
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
        std::cout << "Attempt to use an unsupported format combinaton!" << std::endl;
        break;
    default:
        std::cout << "Unknown error while attempting to create frame buffer object!" << std::endl;
        break;
    }
}
