#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// Необходимые Qt классы
#include <QMainWindow> // Базовый класс окна
#include <QStackedWidget> // Для переключения экранов
#include <QWidget> // Базовый класс для виджетов
#include <QPushButton> // Кнопки
#include <QLineEdit> // Поля ввода
#include <QTextEdit> // Область вывода ответа сервера
#include <QComboBox> // Выпадающий список (для критериев поиска)
#include <QTableWidget> // Таблицы
#include <QGroupBox> // Группировка элементов
#include <QHeaderView> // Для настройки заголовков таблиц
#include <QMessageBox> // Для всплывающих сообщений
#include <QAction>
#include <QMenu>
#include <QMenuBar>

#include <QDateEdit>
#include <QtCharts/QChartView> // Для отображения графиков
#include <QtCharts/QChart>     // Сам график

QT_BEGIN_NAMESPACE

namespace QtCharts {
class QChartView;
class QChart;
}

QT_END_NAMESPACE

// Пользовательский класс клиента
#include "client.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT // Макрос для использования сигналов и слотов

public:
    // Конструктор и деструктор
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots: // Слоты для обработки сигналов (например, нажатий кнопок)

    // --- Navigation Slots ---
    void onRegButtonClicked();       // Переход на экран регистрации
    void onAuthButtonClicked();      // Переход на экран аутентификации

    // --> Новый слот: Показать экран статистики <--
    void onShowStatsScreenClicked();
    // --> Новый слот: Вернуться с экрана статистики <--
    void onBackFromStatsClicked();

    void onBackButtonClicked();      // Возврат на главный экран / Выход из аккаунта
    void onSendRegButtonClicked();   // Отправка запроса на регистрацию
    void onSendAuthButtonClicked();  // Отправка запроса на аутентификацию

    // --- Client Screen Slots ---
    void onClientExtendRentalClicked(); // Продление аренды
    void onClientViewStatsClicked();    // Просмотр своей статистики аренды
    void onClientViewInfoClicked();     // Просмотр своей информации
    void onClientSearchClicked();       // Нажатие кнопки "Найти" (поиск книг)
    void onClientViewAllBooksClicked(); // Нажатие кнопки "Показать все" (весь каталог)

    // --- Librarian Screen Slots ---
    void onLibrarianAssignBookClicked();    // Выдача книги пользователю
    void onLibrarianUnassignBookClicked();  // Возврат книги от пользователя
    void onLibrarianViewDebtsClicked();   // Просмотр аренд/долгов КОНКРЕТНОГО пользователя
    void onLibrarianGetUserInfoClicked();   // Получение информации о пользователе по логину
    void onLibrarianAddBookClicked();       // Добавление новой книги
    void onLibrarianUpdateBookClicked();    // Обновление информации о книге
    void onLibrarianSearchClicked();      // Нажатие кнопки "Найти книгу" (поиск книг)
    void onRefreshBooksClicked();         // Нажатие кнопки "Показать все книги" (весь каталог)
    void onRefreshDebtsClicked();         // Нажатие кнопки "Показать все выдачи/долги"

    // --- Server Response Handlers ---
    void handleBookStats(const QStringList& stats);                      // Обработка статистики аренды клиента
    void handleUserInfo(const QString& login, const QString& email, const QString& password); // Обработка своей инфо
    void handleUserDebts(const QStringList& debts);                      // Обработка списка аренд/долгов (из разных запросов)
    void handleOperationStatus(bool success, const QString& message);    // Обработка статуса операций (auth+, reg+, assign+ ...)
    void handleServerResponse(const QString& response);                  // Отображение сырого ответа сервера (для отладки)
    void handlePublicUserInfo(int id, const QString& login, const QString& email); // Обработка инфо юзера для библиотекаря
    void handleOverdueBooksNotification(const QStringList& messages);    // Обработка уведомлений о просрочке
    void handleClientBooksListUpdate(const QStringList& books);          // Общий обработчик обновления таблиц книг (после поиска/показать все)

    // Слоты для аннотаций
    void onClientBookDoubleClicked(QTableWidgetItem *item); // ★ Новый слот для клиента
    void onLibrarianBookDoubleClicked(QTableWidgetItem *item); // Слот для двойного клика по таблице
    void handleBookAnnotation(int book_id, const QString& title, const QString& annotation); // Обработка полученной аннотации
    void onSaveAnnotationClicked();      // Нажатие кнопки "Сохранить аннотацию"
    void onBackToLibrarianClicked();     // Нажатие кнопки "Назад" на экране аннотации

    // Новые слоты для списка пользователей
    void handleAllUsersList(const QStringList& users);
    void onRefreshUsersClicked();

    void onImportBooksClicked();
    void onExportBooksClicked();
    void handleExportCsvData(const QString& csvData);

    void onThemeChangeToggled(bool checked);

    // Новые слоты для кнопок управления пользователями
    void onBlockUserClicked();
    void onUnblockUserClicked();
    void onResetPasswordClicked();
    void onChangeEmailClicked();

    // Новые слоты для истории аренды клиента
    void onClientViewHistoryClicked();
    void handleRentalHistory(const QStringList& history);

    // Новые слоты для статистики
    void onRefreshStatsClicked();
    void handleLibraryStats(int totalBooks, int availableBooks, int rentedBooks,
                            int totalClients, int activeRentals, int overdueRentals);

    // Новые слоты для экрана статистики
    void onGenerateReportClicked();     // Слот для кнопки "Сформировать отчет"
    void onExportReportClicked();       // Слот для кнопки "Экспорт отчета"
    // Слот для обработки ответа сервера с ТАБЛИЧНЫМИ данными
    void handleStatisticsReportReceived(const QString& reportType, const QStringList& headers, const QStringList& data);
    // Слот для обработки ответа сервера с данными для ГРАФИКА
    void handleChartDataReceived(const QString& reportType, const QStringList& labels, const QList<qreal>& values);

    // Обработка полученного списка жанров
    void handleGenresList(const QStringList& genres);

    void onRefreshGenresClicked();


private: // Приватные методы и члены класса

    void setupMenus();

    // --- UI Setup Methods ---
    void setupMainScreen();       // Настройка главного экрана
    void setupRegScreen();        // Настройка экрана регистрации
    void setupAuthScreen();       // Настройка экрана аутентификации
    void setupClientScreen();     // Настройка экрана клиента
    void setupLibrarianScreen();  // Настройка экрана библиотекаря
    void setupAnnotationScreen();
    void setupStatsScreen();

    // --- Table Population / Display Methods ---
    void showBookStatsTable(const QStringList& stats);             // Заполнение таблицы статистики клиента
    void showUserRentalsTable(const QStringList& rentals);         // Заполнение таблицы аренд/долгов библиотекаря
    void populateBooksTable(QTableWidget* table, const QStringList& books); // Общий метод заполнения таблиц каталога книг

    // Новый метод для заполнения таблицы пользователей
    void populateUsersTable(const QStringList& users);

    // Новый метод для заполнения таблицы истории
    void populateHistoryTable(const QStringList& history);

    // Новые методы для отображения отчетов
    // Метод для отображения табличного отчета
    void displayStatisticsReport(const QStringList& headers, const QStringList& data);
    // Метод для отображения графика
    void displayChart(const QString& title, const QStringList& labels, const QList<qreal>& values, const QString& reportType);


    int getSelectedUserIdFromTable(QTableWidget* table);

    // Новая утилитарная функция для экранирования CSV полей
    QString escapeCsvField(const QString &field);

    // --- UI Member Variables ---
    QStackedWidget *mStackedWidget; // Виджет для переключения экранов

    // Main screen elements
    QWidget *mMainScreen;
    QPushButton *mRegButton;
    QPushButton *mAuthButton;

    // Registration screen elements
    QWidget *mRegScreen;
    QLineEdit *mRegLoginInput;
    QLineEdit *mRegPasswordInput;
    QLineEdit *mRegEmailInput;
    QComboBox *mRegRoleComboBox;
    QPushButton *mSendRegButton;
    QPushButton *mBackRegButton;

    // Authentication screen elements
    QWidget *mAuthScreen;
    QLineEdit *mAuthLoginInput;
    QLineEdit *mAuthPasswordInput;
    QPushButton *mSendAuthButton;
    QPushButton *mBackAuthButton;

    // Client screen elements
    QWidget *mClientScreen;
    // Аренда и инфо
    QLineEdit *mClientBookIdInput;       // Для продления
    QLineEdit *mClientDaysInput;         // Для продления
    QPushButton *mClientExtendRentalButton;
    QPushButton *mClientViewStatsButton;
    QPushButton *mClientViewInfoButton;
    QTableWidget *mClientStatsTable;       // Таблица арендованных книг клиента
    // Каталог
    QTableWidget *mClientAllBooksTable;   // Таблица всех книг
    QLineEdit    *mClientSearchInput;       // Поле ввода для поиска
    QComboBox    *mClientSearchCriteria;    // Выбор критерия поиска
    QPushButton  *mClientSearchButton;      // Кнопка "Найти"
    QPushButton  *mClientViewAllBooksButton; // Кнопка "Показать все"
    // Навигация
    QPushButton *mClientBackButton;

    // Librarian screen elements
    QWidget *mLibrarianScreen;
    // Управление фондом
    QLineEdit *mLibrarianBookTitleInput;
    QLineEdit *mLibrarianBookAuthorInput;
    QLineEdit *mLibrarianBookYearInput;
    QLineEdit *mLibrarianBookGenreInput;
    QLineEdit *mLibrarianBookIdInput;       // Для Add/Update/Assign/Unassign
    QPushButton *mLibrarianAddBookButton;
    QPushButton *mLibrarianUpdateBookButton;
    // Управление пользователями
    QLineEdit *mLibrarianUserIdInput;       // Для Assign/Unassign/Debts (пользователя)
    QLineEdit *mLibrarianUserLoginInput;    // Для Get Info
    QPushButton *mLibrarianAssignBookButton;
    QPushButton *mLibrarianUnassignBookButton;
    QPushButton *mLibrarianViewDebtsButton;  // Показать аренды/долги КОНКРЕТНОГО юзера
    QPushButton *mLibrarianGetUserInfoButton;
    // Каталог книг
    QTableWidget *mLibrarianBooksTable;    // Таблица всех книг
    QLineEdit    *mLibrarianSearchInput;       // Поле ввода для поиска
    QComboBox    *mLibrarianSearchCriteria;    // Выбор критерия поиска
    QPushButton  *mLibrarianSearchButton;      // Кнопка "Найти"
    QPushButton  *mLibrarianRefreshBooksButton; // Кнопка "Показать все" книги (бывш. Refresh)
    // Выдачи/Долги
    QTableWidget *mLibrarianDebtsTable;     // Таблица всех аренд/долгов
    QPushButton *mRefreshDebtsButton;       // Кнопка "Показать все" аренды/долги
    // Навигация
    QPushButton *mLibrarianBackButton;


    // Виджет и элементы экрана редактирования аннотации
    QWidget *mAnnotationScreen;
    QLabel *mAnnotationBookTitleLabel; // Показать название книги
    QTextEdit *mAnnotationEdit;        // Поле редактирования аннотации
    QPushButton *mSaveAnnotationButton;
    QPushButton *mBackToLibrarianButton;

    // Переменная для хранения ID редактируемой книги
    int mCurrentlyEditingBookId = -1;

    // Новые элементы для списка пользователей на экране библиотекаря
    QTableWidget *mLibrarianUsersTable;
    QPushButton  *mRefreshUsersButton;

    QPushButton *mImportBooksButton;
    QPushButton *mExportBooksButton;

    // Элементы меню
    QMenuBar *mMenuBar;
    QMenu *mViewMenu;
    QAction *mThemeAction;

    // Новые кнопки управления пользователями
    QPushButton *mBlockUserButton;
    QPushButton *mUnblockUserButton;
    QPushButton *mResetPasswordButton;
    QPushButton *mChangeEmailButton;

    // Новые элементы для истории на экране клиента
    QGroupBox    *mClientHistoryGroup; // Опционально, для группировки
    QTableWidget *mClientHistoryTable;
    QPushButton  *mClientViewHistoryButton;

    // Члены UI для статистики
    QGroupBox   *mStatsGroup; // Группа для статистики
    QLabel      *mTotalBooksLabel;
    QLabel      *mAvailableBooksLabel;
    QLabel      *mRentedBooksLabel;
    QLabel      *mTotalClientsLabel;
    QLabel      *mActiveRentalsLabel;
    QLabel      *mOverdueRentalsLabel;
    QPushButton *mRefreshStatsButton;

    // Новая кнопка на экране библиотекаря для перехода к отчетности
    QPushButton *mShowStatsScreenButton;

    // Response display area (Общее для всех экранов)
    QTextEdit *mResponseTextEdit;

    // Новые элементы UI для экрана детальной статистики/отчетов
    QWidget            *mStatsScreen;           // Виджет для экрана статистики
    QDateEdit          *mStartDateEdit;       // Выбор начальной даты
    QDateEdit          *mEndDateEdit;         // Выбор конечной даты
    QComboBox          *mReportTypeCombo;     // Выбор типа отчета
    QComboBox          *mGenreFilterCombo;    // Пример фильтра (по жанру)
    QPushButton        *mGenerateReportButton;// Кнопка "Сформировать отчет"
    QPushButton        *mExportReportButton;  // Кнопка "Экспорт отчета в CSV"
    QPushButton        *mBackFromStatsButton; // Кнопка "Назад" к библиотекарю

    // Виджет для переключения между таблицей и графиком
    QStackedWidget     *mReportDisplayWidget;
    // Таблица для детального отображения данных отчета
    QTableWidget       *mReportTableWidget;
    // Виджет для отображения графика (из QtCharts)
    QtCharts::QChartView *mReportChartView;

};

#endif // MAINWINDOW_H
