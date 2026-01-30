#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("荧光按钮测试");
    window.setMinimumSize(360, 240);
    window.setStyleSheet(
        "QWidget { background-color: #0b1021; }"
        "QPushButton {"
        "  color: #9efcff;"
        "  background-color: #0b1021;"
        "  border: 2px solid #38f8ff;"
        "  border-radius: 14px;"
        "  padding: 12px 28px;"
        "  font-size: 18px;"
        "  font-weight: 700;"
        "  letter-spacing: 0.6px;"
        "  text-transform: uppercase;"
        "}"
        "QPushButton:hover {"
        "  background-color: #11203b;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #0c182c;"
        "}"
    );

    auto *btn = new QPushButton("Glow");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(60);

    // 荧光效果：强发光阴影 + 呼吸动画
    auto *glow = new QGraphicsDropShadowEffect(btn);
    glow->setBlurRadius(26);
    glow->setColor(QColor(56, 248, 255, 190));
    glow->setOffset(0, 0);
    btn->setGraphicsEffect(glow);

    auto *pulse = new QPropertyAnimation(glow, "blurRadius", btn);
    pulse->setStartValue(18);
    pulse->setEndValue(36);
    pulse->setDuration(1400);
    pulse->setEasingCurve(QEasingCurve::InOutSine);
    pulse->setLoopCount(-1);
    pulse->start();

    auto *colorPulse = new QPropertyAnimation(glow, "color", btn);
    colorPulse->setStartValue(QColor(56, 248, 255, 150));
    colorPulse->setEndValue(QColor(158, 255, 255, 230));
    colorPulse->setDuration(1400);
    colorPulse->setEasingCurve(QEasingCurve::InOutSine);
    colorPulse->setLoopCount(-1);
    colorPulse->start();

    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(36, 32, 36, 32);
    layout->addStretch();
    layout->addWidget(btn, 0, Qt::AlignHCenter);
    layout->addStretch();

    window.show();
    return app.exec();
}
