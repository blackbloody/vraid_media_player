#ifndef GL_SPEC_FRAME_H
#define GL_SPEC_FRAME_H
#pragma once

#include <memory>
#include "vertex_array.h"
#include "vertex_buffer.h"
#include "vertex_buffer_layout.h"
#include "shader.h"
#include "renderer.h"
#include "texture.h"

#include "obj_audio.h"

#include <QOpenGLWidget>


class GlSpecViewFrame : public QOpenGLWidget {
    Q_OBJECT
public:
    GlSpecViewFrame(QWidget* parent=nullptr);
    //~GlSpecViewFrame() { cleanup(); }
    int viewportWidthPx()  const { return int(width()  * devicePixelRatioF()); }
    int viewportHeightPx() const { return int(height() * devicePixelRatioF()); }
protected:
   void initializeGL() override;
   void resizeGL(int w, int h) override;
   void paintGL() override;
private slots:
   void cleanup();
private:
   ViewSignalDataMode view_mode = ViewSignalDataMode::WaveForm;
   bool cleaned_ = false;
   int signalWaveSize;
   int gl_draw_count;
   std::unique_ptr<Renderer> renderer;

   // vertex wave
   std::unique_ptr<VertexBuffer> vbWave;
   std::unique_ptr<VertexArray> vaWave;
   std::unique_ptr<VertexBufferLayout> vblWave;
   std::unique_ptr<Shader> shaderWave;

   std::unique_ptr<VertexBuffer> vbMelSpec;
   std::unique_ptr<VertexArray> vaMelSpec;
   std::unique_ptr<IndexBuffer> ibMelSpec;
   std::unique_ptr<VertexBufferLayout> vblMelSpec;
   std::unique_ptr<Texture> texMelSpec;
   std::unique_ptr<Shader> shaderMelSpec;

public:
   void setViewWav(const float& start_sec, const float& view_port_sec, const int& gl_draw_count, const std::vector<float> &draw_data);
   void setViewSpec(const float& start_sec, const float& view_port_sec, const SpectrogramTileOverlap& spec);
};

#endif // GL_SPEC_FRAME_H
