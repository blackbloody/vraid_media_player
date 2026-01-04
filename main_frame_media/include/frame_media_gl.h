#ifndef FRAME_MEDIA_GL_H
#define FRAME_MEDIA_GL_H
#pragma once

#include <mutex>
#include <memory>
#include "vertex_array.h"
#include "vertex_buffer.h"
#include "vertex_buffer_layout.h"
#include "shader.h"
#include "renderer.h"
#include "texture.h"

#include <QOpenGLWidget>
struct MdlTextCoordMatrix {
    float x0, y0;
    float x1, y1;
    float vpW, vpH;
    glm::mat4 mvp;
};

class GlFrameMedia : public QOpenGLWidget {
    Q_OBJECT
public:
    GlFrameMedia(QWidget* parent=nullptr);
    int viewportWidthPx()  const { return int(width()  * devicePixelRatioF()); }
    int viewportHeightPx() const { return int(height() * devicePixelRatioF()); }
    void updateText(int width, int height, const std::vector<uint8_t>& data);

    void submitFrame(int w, int h, std::shared_ptr<const std::vector<uint8_t>> pix);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
private slots:
    void cleanup();
private:
    bool initObjGl = false;
    bool cleaned_ = false;
    int obj_width, obj_height;

    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Texture> texture;

    std::unique_ptr<VertexBuffer> vb;
    std::unique_ptr<VertexArray> va;
    std::unique_ptr<IndexBuffer> ib;
    std::unique_ptr<VertexBufferLayout> vbl;
    std::unique_ptr<Shader> shader;


    // func
    void initMainRenderObject();
    MdlTextCoordMatrix createAspectRatioMatrix(const int& view_port_w, const int& view_port_h, const int& img_w, const int& img_h);
};

#endif // FRAME_MEDIA_GL_H
