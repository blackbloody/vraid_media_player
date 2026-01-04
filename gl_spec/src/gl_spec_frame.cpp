#include "gl_spec_frame.h"
#include <glad/glad.h>
#include <glm/ext/matrix_clip_space.hpp>

#include <QSurfaceFormat>
#include <QOpenGLContext>

#include <QDebug>
#include <string>

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

GlSpecViewFrame::GlSpecViewFrame(QWidget* parent) : QOpenGLWidget(parent) {
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    setFormat(fmt);
}

void GlSpecViewFrame::cleanup() {
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
    shaderWave.reset();
    vaWave.reset();
    vbWave.reset();
    renderer.reset();

    if (needDone && QOpenGLContext::currentContext() == ctx)
        doneCurrent();

}

void GlSpecViewFrame::setViewWav(const float& start_sec, const float& view_port_sec, const int& gl_draw_count, const std::vector<float> &draw_data) {

    this->view_mode = ViewSignalDataMode::WaveForm;
    this->signalWaveSize = static_cast<int>(draw_data.size());
    this->gl_draw_count = gl_draw_count;

    //if (QOpenGLContext::currentContext() == context()) {}

    if (!vaWave) {
        makeCurrent();

        vaWave = make_unique<VertexArray>();
        vbWave = make_unique<VertexBuffer>(
            draw_data.data(),
            draw_data.size() * sizeof(GLfloat),
            GL_DYNAMIC_DRAW
            );
        vblWave = make_unique<VertexBufferLayout>();
        vblWave->Pushs(2);
        vaWave->AddBuffer(*vbWave, *vblWave);

        float zoom = 1.0f;        // 1.0 = normal size, >1 = zoom in
        float pan_x = 0.0f;       // left/right pan
        float pan_y = 0.0f;       // up/down pan

        float left   = -1.0f / zoom + pan_x;
        float right  =  1.0f / zoom + pan_x;
        float bottom = -1.0f / zoom + pan_y;
        float top    =  1.0f / zoom + pan_y;

        glm::mat4 proj = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);

        shaderWave = make_unique<Shader>("resources/shader/MinimalShader.shader");
        shaderWave->Bind();
        shaderWave->SetUniform4f("u_Color", 1.0f, 1.0f, 1.0f, 1.0f);
        shaderWave->SetUniformMat4f("u_MVP", proj);

        vaWave->Unbind();
        vbWave->Bind();
        shaderWave->Unbind();

        doneCurrent();
        update();
    } else {
        makeCurrent();

        vbWave->Bind();
        GLCall(glBufferSubData(GL_ARRAY_BUFFER, 0, draw_data.size() * sizeof(GLfloat), draw_data.data()));
        vbWave->Bind();

        doneCurrent();
        update();
    }

}

void GlSpecViewFrame::setViewSpec(const float& start_sec, const float& view_port_sec, const SpectrogramTileOverlap& spec) {

    this->view_mode = ViewSignalDataMode::Mel_Spectrogram;
    if (!vaMelSpec) {
        float vertices[] = {
            // Positions     // UVs
            -1.0f, -1.0f,    0.0f, 0.0f,
            1.0f, -1.0f,    1.0f, 0.0f,
            1.0f,  1.0f,    1.0f, 1.0f,
            -1.0f,  1.0f,    0.0f, 1.0f,
        };
        unsigned int indices[] = {
            0, 1, 2,
            2, 3, 0
        };
        makeCurrent();

        vaMelSpec = make_unique<VertexArray>();
        vbMelSpec = make_unique<VertexBuffer>(
            vertices, 4 * 4 * sizeof(float), GL_DYNAMIC_DRAW
            );

        vblMelSpec = make_unique<VertexBufferLayout>();
        vblMelSpec->Pushs(2);
        vblMelSpec->Pushs(2);
        vaMelSpec->AddBuffer(*vbMelSpec, *vblMelSpec);

        ibMelSpec = make_unique<IndexBuffer>(indices, 6);

        texMelSpec = make_unique<Texture>(spec.width, spec.height, spec.data.data());
        shaderMelSpec = make_unique<Shader>("resources/shader/SpectrogramQuad.shader");
        shaderMelSpec->Bind();
        shaderMelSpec->SetUniform1i("u_Colormap", 3);
        shaderMelSpec->SetUniform1i("u_Texture", 0);  // Texture unit 0

        vaMelSpec->Unbind();
        vbMelSpec->Unbind();
        ibMelSpec->Unbind();
        texMelSpec->Unbind();
        shaderMelSpec->Unbind();

        doneCurrent();
        update();
    } else {
        makeCurrent();

        texMelSpec->updateText(spec.width, spec.height, spec.data.data());
        /*
        texMelSpec->Bind();
        GLCall(glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_R32F,
            spec.width,
            spec.height,
            0,
            GL_RED,
            GL_FLOAT,
            spec.data.data()
            ));
        texMelSpec->Unbind();
        */

        doneCurrent();
        update();
    }
}

void GlSpecViewFrame::initializeGL() {
    if (!gladLoadGLLoader((GLADloadproc)qtGetProc)) {
        qFatal("Failed to init GLAD");
    }
    qDebug() << "GL:" << (const char*)glGetString(GL_VERSION);
    connect(context(), &QOpenGLContext::aboutToBeDestroyed,
            this, &GlSpecViewFrame::cleanup, Qt::DirectConnection);

    renderer = make_unique<Renderer>();

    // Optional: set default viewport now; resizeGL will refine with DPR
    int w = width(), h = height();
    const int fbw = int(w * devicePixelRatioF());
    const int fbh = int(h * devicePixelRatioF());
    glViewport(0, 0, fbw, fbh);
}

void GlSpecViewFrame::resizeGL(int w, int h) {
    const int fbw = int(w * devicePixelRatioF());
    const int fbh = int(h * devicePixelRatioF());
    glViewport(0, 0, fbw, fbh);
    //glViewport(0, 0, w, h);
}

void GlSpecViewFrame::paintGL() {
    //if (cleaned_) return;

    renderer->Clear();
    switch(view_mode) {
    case ViewSignalDataMode::WaveForm:
        if (vaWave) {
            GLCall(glPointSize(0.01f));
            shaderWave->Bind();
            shaderWave->SetUniform4f("u_Color", 0.0f, 0.0f, 1.0f, 1.0f);

            GLenum enumType = GL_LINE_STRIP;
            if (signalWaveSize == gl_draw_count * 2) {
                enumType = GL_LINE_STRIP;
            } else {
                enumType = GL_LINES;
            }
            renderer->DrawWithoutIndexBuffer(*vaWave, *shaderWave, gl_draw_count, enumType);
            vaWave->Unbind();
            shaderWave->Unbind();
        }
        break;
    case ViewSignalDataMode::Mel_Spectrogram:
        if (vaMelSpec) {
            texMelSpec->Bind(0);
            shaderMelSpec->Bind();
            shaderMelSpec->SetUniform1i("u_Colormap", 3);
            renderer->Draw(*vaMelSpec, *ibMelSpec, *shaderMelSpec);

            vaMelSpec->Unbind();
            ibMelSpec->Unbind();
            shaderMelSpec->Unbind();
        }
        break;
    }

}
