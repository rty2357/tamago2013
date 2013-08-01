#ifndef GLWIDGET_HPP
#define GLWIDGET_HPP

#include <QtOpenGL/QGLWidget>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include "camera.hpp"
#include "stream.hpp"

class GLWidget : public QGLWidget
{
    Q_OBJECT

    public:

        GLWidget(QWidget *parent = 0);
        ~GLWidget();

    protected:

        void initializeGL();        //  OpenGL 初期化
        void resizeGL(int, int);    //  ウィジットリサイズ時のハンドラ
        void paintGL();             //  描画処理

        void keyPressEvent(QKeyEvent *event);
        void keyReleaseEvent(QKeyEvent *event);
        void mouseMoveEvent(QMouseEvent *event);
        void mousePressEvent(QMouseEvent *event);
        void mouseReleaseEvent(QMouseEvent *event);
        void wheelEvent(QWheelEvent *event);

    public:

        void setfps(int fps) { timer->start(1000/fps); }

    private:

        // FPS制御
        QTimer *timer;

        // 画面サイズ関連
        int    width;
        int    height;
        double aspect;

        // カメラ
        Camera camera;

        // マウス制御クラスに移動予定
        int mouse_prev_x;
        int mouse_prev_y;
        int mouse_prev_b;

        // データの管理暮らす
        StreamManager smanager;
};

#endif
