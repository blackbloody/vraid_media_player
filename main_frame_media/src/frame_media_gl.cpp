#include "frame_media_gl.h"
#include <glad/glad.h>


#include <glm/ext/matrix_clip_space.hpp>

#include <QSurfaceFormat>
#include <QOpenGLContext>

#include <QDebug>

// Only if you're stuck with C++11
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

static void* qtGetProc(const char* name) {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    return reinterpret_cast<void*>(
        QOpenGLContext::currentContext()->getProcAddress(name));
#else
    return reinterpret_cast<void*>(
        QOpenGLContext::currentContext()->getProcAddress(QByteArray(name)));
#endif
}

GlFrameMedia::GlFrameMedia(QWidget* parent) : QOpenGLWidget(parent) {
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    setFormat(fmt);
}

void GlFrameMedia::cleanup() {
    if (cleaned_) return;
    cleaned_ = true;

    QOpenGLContext* ctx = context();
    if (!ctx) return;                      // already torn down

    // When called via aboutToBeDestroyed, Qt makes the context current.
    bool needDone = false;
    if (QOpenGLContext::currentContext() != ctx) {
        if (!isValid()) return;            // context not usable anymore; skip explicit deletes
        makeCurrent();
        needDone = true;
    }

    // Destroy ALL objects that might call gl* in their destructors:
    renderer.reset();
    shader.reset();
    vb.reset();
    ib.reset();
    va.reset();

    if (needDone && QOpenGLContext::currentContext() == ctx)
        doneCurrent();

}

void GlFrameMedia::updateText(int width, int height, const std::vector<uint8_t>& data) {
    if (va) {

        // makeCurrent();
        texture->updateText(width, height, data, 4);
        // doneCurrent();
        update();
    }
}

void GlFrameMedia::submitFrame(int w, int h, std::shared_ptr<const std::vector<uint8_t>> pix) {
    /*
    {
        std::lock_guard<std::mutex> lk(frame_mtx_);
        pendingFrameW_ = w;
        pendingFrameH_ = h;
        pendingFramePix_ = std::move(pix);
    }
    hasFramePending_.store(true, std::memory_order_release);
    */
    if (va) {
        texture->updateText(w, h, *pix, 4);
        update();
    }
}

void GlFrameMedia::initializeGL() {
    if (!gladLoadGLLoader((GLADloadproc)qtGetProc)) {
        qFatal("Failed to init GLAD");
    }
    qDebug() << "GL:" << (const char*)glGetString(GL_VERSION);
    connect(context(), &QOpenGLContext::aboutToBeDestroyed,
            this, &GlFrameMedia::cleanup, Qt::DirectConnection);

    initMainRenderObject();
    renderer = make_unique<Renderer>();

    int w = width(), h = height();
    const int fbw = int(w * devicePixelRatioF());
    const int fbh = int(h * devicePixelRatioF());
    glViewport(0, 0, fbw, fbh);
}

void GlFrameMedia::resizeGL(int w, int h) {
    const int fbw = int(w * devicePixelRatioF());
    const int fbh = int(h * devicePixelRatioF());
    glViewport(0, 0, fbw, fbh);

    std::string log = "device size: " + std::to_string(fbw) + "x" + std::to_string(fbh) + "\n" +
                      "media size: " + std::to_string(obj_width) + "x" + std::to_string(obj_height) + "\n";
    qDebug() << log.c_str();

    if (va && shader && initObjGl) {
        MdlTextCoordMatrix coordMatrix = createAspectRatioMatrix(fbw, fbh, this->obj_width, this->obj_height);
        shader->Bind();
        shader->SetUniformMat4f("u_MVP", coordMatrix.mvp);
        shader->Unbind();
    }
}

void GlFrameMedia::paintGL() {
    renderer->Clear();

    shader->Bind();
    texture->Bind(0);

    shader->SetUniform1i("u_Texture", 0);

    renderer->Draw(*va, *ib, *shader);

    ib->Unbind();
    va->Unbind();
    shader->Unbind();
}

void GlFrameMedia::initMainRenderObject() {

    MdlImageByte img = Texture::loadImageByte("/home/virus/Pictures/f_1.png", false);
    // MdlImageByte img = Texture::loadImageByte("/home/virus/Pictures/shot0002.png");
    this->obj_width = img.width;
    this->obj_height = img.height;

    MdlTextCoordMatrix coordMatrix = createAspectRatioMatrix(viewportWidthPx(), viewportHeightPx(), img.width, img.height);

    ////////////////////////////////////////////////////////////////////
    /// \brief vertices

    GLfloat vertices[] = {
        //   x     y      u     v
        +0.5f, -0.5f,  1.0f, 1.0f,   // bottom right
        +0.5f, +0.5f,  1.0f, 0.0f,   // top right
        -0.5f, -0.5f,  0.0f, 1.0f,   // bottom left
        -0.5f, +0.5f,  0.0f, 0.0f    // top left
    };

    GLuint indices[] = {
        0, 1, 2,
        2, 1, 3   // <-- FIX winding
    };

    va = make_unique<VertexArray>();
    vb = make_unique<VertexBuffer>(vertices, 4 * 4 * sizeof(float)); // 4 vertices * 4 floats
    ib = make_unique<IndexBuffer>(indices, 6);

    vbl = make_unique<VertexBufferLayout>();
    vbl->Pushs(2); // location 0: aPos (x, y)
    vbl->Pushs(2); // location 1: aTexCoord (u, v)
    va->AddBuffer(*vb, *vbl);

    shader = make_unique<Shader>("resources/shader/ShaderV_Flip.shader");
    shader->Bind();

    texture = make_unique<Texture>(img.width, img.height, img.pixels, img.channels);
    texture->Bind(0);

    shader->SetUniform1i("u_Texture", 0); // texture unit 0
    shader->SetUniformMat4f("u_MVP", coordMatrix.mvp);

    va->Unbind();
    vb->Unbind();
    ib->Unbind();
    shader->Unbind();

    initObjGl = true;
}

#include <glm/gtc/matrix_transform.hpp>

MdlTextCoordMatrix GlFrameMedia::createAspectRatioMatrix(const int& vpW, const int& vpH, const int& imgW, const int& imgH)
{
    float fvpW = float(vpW), fvpH = float(vpH);
    float fimgW = float(imgW), fimgH = float(imgH);

    float s = std::min(fvpW / fimgW, fvpH / fimgH) * 0.9f;
    float targetW = fimgW * s;
    float targetH = fimgH * s;

    glm::mat4 proj = glm::ortho(-fvpW*0.5f, fvpW*0.5f,
                                -fvpH*0.5f, fvpH*0.5f,
                                -1.0f, 1.0f);

    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(targetW, targetH, 1.0f));
    glm::mat4 mvp = proj * model;

    // x0..y1 not used anymore (set to 0 or keep for debug)
    return {0,0,0,0,fvpW,fvpH,mvp};
}
