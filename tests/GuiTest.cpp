#include <QtTest/QtTest>
#include "gui/MainWindow.hpp"

class GuiTest : public QObject {
    Q_OBJECT
private slots:
    void testWindowCreation() {
        MainWindow w;
        w.show();
        QVERIFY(w.isVisible());
        QTest::qWait(500);
    }
};
QTEST_MAIN(GuiTest)
#include "GuiTest.moc"