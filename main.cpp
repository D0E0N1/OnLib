#include <QApplication>
#include "mytcpserver.h"
#include "mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QStyle>
#include <QPalette>


// --- Новая функция для применения темы ---
void applyTheme(bool useDark) {
    if (useDark) {
        // Ваша существующая темная палитра
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        qApp->setPalette(darkPalette);
        qDebug() << "Applied Dark Theme.";
    } else {
        // Сбрасываем на стандартную палитру текущего стиля (Fusion)
        qApp->setPalette(QApplication::style()->standardPalette());
        qDebug() << "Applied Light (Standard) Theme.";
        // или так, если setPalette(standardPalette) не сработает как надо
        // QApplication::setPalette(QApplication::style()->standardPalette());
    }
}
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("JunTeam");
    QCoreApplication::setApplicationName("OnLib");

    // Установка стиля для всего приложения
    QApplication::setStyle("Fusion");

    // Чтение настройки темы ПЕРЕД применением палитры
    QSettings settings;
    bool useDarkTheme = settings.value("ui/useDarkTheme", true).toBool(); // По умолчанию темная

    applyTheme(useDarkTheme);

    // Запуск TCP сервера
    MyTcpServer server;
    qDebug() << "TCP Server started on port 33333";

    // Создание и отображение главного окна
    MainWindow mainWindow;
    mainWindow.setWindowTitle("OnLib - Library Management System");
    mainWindow.resize(800, 600);
    mainWindow.show();

    return a.exec();
}
