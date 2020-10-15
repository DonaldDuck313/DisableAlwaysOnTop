#include <QApplication>
#include <QtWidgets>
#include <QSystemTrayIcon>
#include <Windows.h>
#pragma comment(lib,"user32.lib")

//This function disables Always On Top for the one window which has handler hwnd
BOOL CALLBACK disableOneWindowAlwaysOnTop(HWND hwnd, LPARAM = 0){
    //Get the title bar size and the window title so that we can filter out system processes
    RECT wrect;
    GetWindowRect(hwnd, &wrect);
    RECT crect;
    GetClientRect(hwnd, &crect);
    POINT lefttop = {crect.left, crect.top};
    ClientToScreen(hwnd, &lefttop);
    const int titleBarSize = lefttop.y - wrect.top;

    WCHAR titleData[1024];
    GetWindowTextW(hwnd, titleData, 1024);
    const QString title = QString::fromWCharArray(titleData);

    //Disable Always On Top for this window
    if(IsWindowVisible(hwnd) && titleBarSize > 0 && !title.contains(QRegExp("^(Default IME|MSCTFIME UI)?$", Qt::CaseInsensitive))){    //Make sure the window is visible, has a title bar and isn't a system process (Default IME or MSCTFIME UI) in order to avoid messing with system stuff
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);    //The last parameter SWP_NOMOVE | SWP_NOSIZE is so that calling this function doesn't move or resize the window
    }
    return TRUE;
}

//This function disables Always On Top for all windows that are currently open
void disableAllWindowsAlwaysOnTop(){
    EnumWindows(disableOneWindowAlwaysOnTop, 0);
}

QTimer timer;    //This has to be a global variable in order to be able to reference it in hideEvent

int main(int argc, char **argv){
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    //Create a timer that periodically disables Always On Top for all windows
    timer.setInterval(1000);
    QObject::connect(&timer, &QTimer::timeout, [&](){
        disableAllWindowsAlwaysOnTop();
    });
    timer.start();

    //Create the tray icon which will contain the user interface
    QSystemTrayIcon trayIcon;

    class: public QMenu{
    protected:
        void hideEvent(QHideEvent*) override{
            timer.start();    //Start the timer again when the context menu closes
        }
    } contextMenu;
    QAction *disableAlwaysOnTopAction = contextMenu.addAction(QObject::tr("&Disable always on top"));
    disableAlwaysOnTopAction->setCheckable(true);
    disableAlwaysOnTopAction->setChecked(true);
    QObject::connect(disableAlwaysOnTopAction, &QAction::triggered, [](bool checked){
        if(checked){
            timer.start();
        }
        else{
            timer.stop();
        }
    });
    QAction *aboutAction = contextMenu.addAction(QObject::tr("&About"));
    QObject::connect(aboutAction, &QAction::triggered, [](){
        timer.stop();    //Stop the timer while the message box is open, otherwise if several desktops are open it will swich back and forth between two random desktops
        QMessageBox msg;
        msg.setIconPixmap(QPixmap(":/icon.ico"));
        msg.setWindowIcon(QIcon(":/icon.ico"));
        msg.setWindowTitle(QObject::tr("About"));
        msg.setText(QObject::tr("Disable Always On Top program by Gustav Lindberg.") + "<br><br>" + QObject::tr("Icons made by %3 from %1 are licensed by %2. Some icons have been modified.").arg("<a href=\"https://www.flaticon.com/\">www.flaticon.com</a>", "<a href=\"http://creativecommons.org/licenses/by/3.0/\">CC 3.0 BY</a>", "<a href=\"https://www.flaticon.com/authors/freepik\" title=\"Freepik\">Freepik</a>"));
        msg.exec();
        timer.start();
    });
    QAction *exitAction = contextMenu.addAction(QObject::tr("E&xit"));
    QObject::connect(exitAction, &QAction::triggered, &app, &QApplication::quit);

    trayIcon.setContextMenu(&contextMenu);
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, [](QSystemTrayIcon::ActivationReason reason){
        if(reason == QSystemTrayIcon::Context){
            timer.stop();    //Stop the timer when the context menu opens, otherwise the context menu will close directly when it's opened
        }
    });

    trayIcon.setIcon(QIcon(":/icon.ico"));
    trayIcon.setToolTip(QObject::tr("Disable Always On Top"));
    trayIcon.show();

    return app.exec();
}
