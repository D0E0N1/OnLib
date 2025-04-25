#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout> // Для более структурированных layout'ов
#include <QLabel>
#include <QMessageBox>
#include <QHeaderView>
#include <QRegularExpression>
#include <QGroupBox>
#include <QComboBox>
#include <QTime>     // Для добавления времени в лог сервера
#include <QInputDialog> // Потенциально может понадобиться, но пока нет

#include <QFileDialog> // Для диалогов выбора/сохранения файла
#include <QFile>       // Для работы с файлами
#include <QDir>        // Для получения пути к домашней папке
#include <QTextStream> // Для потоковой записи/чтения текстовых файлов

#include <QMenuBar>
#include <QAction>
#include <QSettings>

extern void applyTheme(bool useDark); // ★ Объявляем внешнюю функцию

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 1. Инициализация основного виджета для переключения экранов
    mStackedWidget = new QStackedWidget(this);

    // 2. Настройка UI для каждого экрана
    setupMainScreen();
    setupRegScreen();
    setupAuthScreen();
    setupClientScreen();
    setupLibrarianScreen();
    setupAnnotationScreen();
    setupMenus(); // Вызов настройки меню

    // 3. Попытка подключения к серверу при запуске
    Client::get_instance().connectToServer("localhost", 33333);

    // 4. Настройка области для вывода ответов сервера (лог)
    mResponseTextEdit = new QTextEdit(this);
    mResponseTextEdit->setReadOnly(true);
    mResponseTextEdit->setMinimumHeight(80); // Меньше высота по умолчанию
    mResponseTextEdit->setMaximumHeight(150); // Ограничиваем максимальную высоту
    mResponseTextEdit->setLineWrapMode(QTextEdit::NoWrap); // Отключить перенос строк, если нужно

    // 5. Создание основного layout окна
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addWidget(mStackedWidget, 1); // Экраны занимают большую часть места
    mainLayout->addWidget(new QLabel("Ответ сервера:")); // Заголовок для лога
    mainLayout->addWidget(mResponseTextEdit);      // Область лога внизу

    // 6. Установка основного layout в центральный виджет
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget); // MainWindow теперь управляет этим виджетом

    // 7. Подключение сигналов от экземпляра Client к слотам MainWindow
    // Ответы сервера на запросы данных
    connect(&Client::get_instance(), &Client::booksListReceived,
            this, &MainWindow::handleClientBooksListUpdate); // Обновление списка книг
    connect(&Client::get_instance(), &Client::bookStatsReceived,
            this, &MainWindow::handleBookStats); // Обновление статистики аренды клиента
    connect(&Client::get_instance(), &Client::userInfoReceived,
            this, &MainWindow::handleUserInfo); // Получение полной информации клиента
    connect(&Client::get_instance(), &Client::userDebtsReceived,
            this, &MainWindow::handleUserDebts); // Получение списка аренд/долгов (для библиотекаря)
    connect(&Client::get_instance(), &Client::publicUserInfoReceived,
            this, &MainWindow::handlePublicUserInfo); // Получение публичной инфо для библиотекаря
    // Статус операций и уведомления
    connect(&Client::get_instance(), &Client::operationStatusReceived,
            this, &MainWindow::handleOperationStatus); // Обработка reg+, auth+, assign+, ошибок и т.д.
    connect(&Client::get_instance(), &Client::overdueBooksNotification,
            this, &MainWindow::handleOverdueBooksNotification); // Уведомления о просрочке
    // Отображение сырых ответов сервера для отладки
    connect(&Client::get_instance(), &Client::responseReceived,
            this, &MainWindow::handleServerResponse);
    // Connect для аннотации
    connect(&Client::get_instance(), &Client::bookAnnotationReceived,
            this, &MainWindow::handleBookAnnotation);
    // Connect для списка пользователей
    connect(&Client::get_instance(), &Client::allUsersListReceived,
            this, &MainWindow::handleAllUsersList);
    // Connect для CSV экспорта
    connect(&Client::get_instance(), &Client::exportCsvDataReceived,
            this, &MainWindow::handleExportCsvData);

    // 8. Установка начального экрана
    mStackedWidget->setCurrentWidget(mMainScreen);
}

MainWindow::~MainWindow()
{
    // Корректное освобождение ресурсов клиента при закрытии окна
    Client::destroy_instance();
}

// Метод настройки меню
void MainWindow::setupMenus() {
    mMenuBar = menuBar(); // Получаем menuBar главного окна

    // --- Меню "Вид" или "Настройки" ---
    mViewMenu = mMenuBar->addMenu(tr("&Вид")); // или "&Настройки"

    // Действие для переключения темы
    mThemeAction = new QAction(tr("Тёмная тема"), this);
    mThemeAction->setCheckable(true); // Делаем его переключателем

    // Читаем сохраненную настройку и устанавливаем состояние QAction
    QSettings settings;
    bool useDarkTheme = settings.value("ui/useDarkTheme", true).toBool(); // По умолчанию темная
    mThemeAction->setChecked(useDarkTheme);

    // Соединяем сигнал изменения состояния с нашим слотом
    connect(mThemeAction, &QAction::triggered, this, &MainWindow::onThemeChangeToggled);

    mViewMenu->addAction(mThemeAction);
}

// Слот для обработки переключения темы
void MainWindow::onThemeChangeToggled(bool checked) {
    qDebug() << "Theme toggled. Dark theme enabled:" << checked;
    // 1. Сохраняем новое значение в настройках
    QSettings settings;
    settings.setValue("ui/useDarkTheme", checked);

    // 2. Применяем новую тему ко всему приложению
    applyTheme(checked);
}

// =============================================
// Navigation Handlers
// =============================================
void MainWindow::onRegButtonClicked() {
    mStackedWidget->setCurrentWidget(mRegScreen);
}

void MainWindow::onAuthButtonClicked() {
    mStackedWidget->setCurrentWidget(mAuthScreen);
}

void MainWindow::onBackButtonClicked() {
    // Сброс состояния клиента при выходе
    Client::get_instance().disconnectFromServer();
    Client::get_instance().setRole("guest");
    Client::get_instance().setUserId(-1);

    // Очистка полей ввода на всех экранах
    // Клиент
    if(mClientBookIdInput) mClientBookIdInput->clear();
    if(mClientDaysInput) mClientDaysInput->clear();
    if(mClientSearchInput) mClientSearchInput->clear();
    // Библиотекарь
    if(mLibrarianUserIdInput) mLibrarianUserIdInput->clear();
    if(mLibrarianBookIdInput) mLibrarianBookIdInput->clear();
    if(mLibrarianUserLoginInput) mLibrarianUserLoginInput->clear();
    if(mLibrarianSearchInput) mLibrarianSearchInput->clear();
    if(mLibrarianBookTitleInput) mLibrarianBookTitleInput->clear();
    if(mLibrarianBookAuthorInput) mLibrarianBookAuthorInput->clear();
    if(mLibrarianBookYearInput) mLibrarianBookYearInput->clear();
    if(mLibrarianBookGenreInput) mLibrarianBookGenreInput->clear();
    // Регистрация и аутентификация
    if(mRegLoginInput) mRegLoginInput->clear();
    if(mRegPasswordInput) mRegPasswordInput->clear();
    if(mRegEmailInput) mRegEmailInput->clear();
    if(mAuthLoginInput) mAuthLoginInput->clear();
    if(mAuthPasswordInput) mAuthPasswordInput->clear();


    // Очистка таблиц
    if(mClientStatsTable) mClientStatsTable->setRowCount(0);
    if(mClientAllBooksTable) mClientAllBooksTable->setRowCount(0);
    if(mLibrarianBooksTable) mLibrarianBooksTable->setRowCount(0);
    if(mLibrarianDebtsTable) mLibrarianDebtsTable->setRowCount(0);

    // Очистка лога сервера и переход на главный экран
    mResponseTextEdit->clear();
    mStackedWidget->setCurrentWidget(mMainScreen);
}

// =============================================
// Registration/Authentication Handlers
// =============================================
void MainWindow::onSendRegButtonClicked() {
    qDebug() << "[DEBUG] onSendRegButtonClicked() called!";
    QString login = mRegLoginInput->text().trimmed();
    QString password = mRegPasswordInput->text(); // Не триммим пароль
    QString email = mRegEmailInput->text().trimmed();
    QString role = mRegRoleComboBox->currentText();

    // --- Валидация на клиенте ---
    if (login.length() < 4 || login.length() > 12 || !QRegularExpression("^[a-zA-Z0-9]+$").match(login).hasMatch()) {
        QMessageBox::warning(this, "Ошибка регистрации", "Логин: 4-12 символов (латиница/цифры)."); return;
    }
    if (password.length() < 8 || password.length() > 12 || !QRegularExpression("^[a-zA-Z0-9!@#$%^&*()_+-=]+$").match(password).hasMatch()) {
        QMessageBox::warning(this, "Ошибка регистрации", "Пароль: 8-12 символов (лат. буквы, цифры, !@#$%^&*()_+-=)."); return;
    }
    if (!QRegularExpression("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$").match(email).hasMatch()) {
        QMessageBox::warning(this, "Ошибка регистрации", "Неверный формат email."); return;
    }

    // --- Отправка запроса ---
    QString request = QString("reg&%1&%2&%3&%4")
                          .arg(login).arg(password).arg(email).arg(role);

    qDebug() << "[DEBUG] Sending Request:" << request;

    Client::get_instance().sendRequest(request);

    // Очистка полей (кроме роли) после попытки отправки
    mRegLoginInput->clear();
    mRegPasswordInput->clear();
    mRegEmailInput->clear();
    qDebug() << "Registration request sent for" << login;
}

void MainWindow::onSendAuthButtonClicked() {
    QString login = mAuthLoginInput->text().trimmed();
    QString password = mAuthPasswordInput->text();

    if (login.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Ошибка входа", "Введите логин и пароль.");
        return;
    }

    QString request = QString("auth&%1&%2").arg(login).arg(password);
    Client::get_instance().sendRequest(request);
    // Поля логина/пароля не очищаем сразу, ждем ответа handleOperationStatus
    qDebug() << "Authentication request sent for" << login;
}

// =============================================
// Client Screen Action Handlers
// =============================================
void MainWindow::onClientExtendRentalClicked() {
    bool bookOk, daysOk;
    int book_id = mClientBookIdInput->text().toInt(&bookOk);
    int days = mClientDaysInput->text().toInt(&daysOk);

    if (!bookOk || !daysOk || book_id <= 0 || days <= 0) {
        QMessageBox::warning(this, "Продление аренды", "Введите корректный ID книги (> 0) и количество дней (> 0).");
        return;
    }
    Client::get_instance().extendRental(book_id, days);
    // Очищаем поля после отправки запроса
    mClientBookIdInput->clear();
    mClientDaysInput->clear();
}

void MainWindow::onClientViewStatsClicked() {
    Client::get_instance().viewBookStats();
}

void MainWindow::onClientViewInfoClicked() {
    Client::get_instance().viewUserInfo();
}

void MainWindow::onClientSearchClicked() {
    QString searchTerm = mClientSearchInput->text().trimmed();
    if (searchTerm.isEmpty()) {
        QMessageBox::warning(this, "Поиск книг", "Введите текст для поиска.");
        return;
    }
    QString criteriaValue = mClientSearchCriteria->currentData().toString();
    Client::get_instance().searchBooks(criteriaValue, searchTerm);
}

void MainWindow::onClientViewAllBooksClicked() {
    mClientSearchInput->clear();
    Client::get_instance().viewAllBooks(); // Запрос полного списка
}

// =============================================
// Librarian Screen Action Handlers
// =============================================
void MainWindow::onLibrarianAssignBookClicked() {
    bool userOk, bookOk;
    int user_id = mLibrarianUserIdInput->text().toInt(&userOk);
    int book_id = mLibrarianBookIdInput->text().toInt(&bookOk);
    if (!userOk || !bookOk || user_id <= 0 || book_id <= 0) {
        QMessageBox::warning(this, "Выдача книги", "Введите корректные User ID (> 0) и Book ID (> 0).");
        return;
    }
    Client::get_instance().assignBook(user_id, book_id);
    // Очищаем поля ввода ID после запроса
    mLibrarianUserIdInput->clear();
    mLibrarianBookIdInput->clear();
}

void MainWindow::onLibrarianUnassignBookClicked() {
    bool userOk, bookOk;
    int user_id = mLibrarianUserIdInput->text().toInt(&userOk);
    int book_id = mLibrarianBookIdInput->text().toInt(&bookOk);
    if (!userOk || !bookOk || user_id <= 0 || book_id <= 0) {
        QMessageBox::warning(this, "Возврат книги", "Введите корректные User ID (> 0) и Book ID (> 0).");
        return;
    }
    Client::get_instance().unassignBook(user_id, book_id);
        // Очищаем поля ввода ID после запроса
    mLibrarianUserIdInput->clear();
    mLibrarianBookIdInput->clear();
}

void MainWindow::onLibrarianViewDebtsClicked() {
    bool userOk;
    int user_id = mLibrarianUserIdInput->text().toInt(&userOk);
    if (!userOk || user_id <= 0) {
        QMessageBox::warning(this, "Просмотр выдач пользователя", "Введите корректный User ID (> 0).");
        return;
    }
    Client::get_instance().viewUserDebts(user_id);
}

void MainWindow::onLibrarianGetUserInfoClicked() {
    QString login = mLibrarianUserLoginInput->text().trimmed();
    if (login.isEmpty()) {
        QMessageBox::warning(this, "Информация о пользователе", "Введите логин пользователя.");
        return;
    }
    Client::get_instance().getUserInfo(login);
    mLibrarianUserLoginInput->clear();
}

void MainWindow::onLibrarianAddBookClicked() {
    QString title = mLibrarianBookTitleInput->text().trimmed();
    QString author = mLibrarianBookAuthorInput->text().trimmed();
    QString year = mLibrarianBookYearInput->text().trimmed();
    QString genre = mLibrarianBookGenreInput->text().trimmed();

    if (title.isEmpty() || author.isEmpty() || year.isEmpty() || genre.isEmpty()) {
        QMessageBox::warning(this, "Добавление книги", "Все поля информации о книге должны быть заполнены.");
        return;
    }
    bool yearOk;
    year.toInt(&yearOk); // Простая проверка на число
    if (!yearOk) {
        QMessageBox::warning(this, "Добавление книги", "Год должен быть числом.");
        return;
    }

    Client::get_instance().addBookToLibrary(title, author, year, genre);
    // Очистка полей информации о книге после попытки добавления
    mLibrarianBookTitleInput->clear();
    mLibrarianBookAuthorInput->clear();
    mLibrarianBookYearInput->clear();
    mLibrarianBookGenreInput->clear();
}

void MainWindow::onLibrarianUpdateBookClicked() {
    bool bookOk;
    int book_id = mLibrarianBookIdInput->text().toInt(&bookOk);
    QString title = mLibrarianBookTitleInput->text().trimmed();
    QString author = mLibrarianBookAuthorInput->text().trimmed();
    QString year = mLibrarianBookYearInput->text().trimmed();
    QString genre = mLibrarianBookGenreInput->text().trimmed();

    if (!bookOk || book_id <= 0) {
        QMessageBox::warning(this, "Обновление книги", "Введите корректный Book ID (> 0).");
        return;
    }
    // Требуем все поля для обновления, чтобы избежать сложностей на сервере
    if (title.isEmpty() || author.isEmpty() || year.isEmpty() || genre.isEmpty()) {
        QMessageBox::warning(this, "Обновление книги", "Для обновления все поля информации о книге должны быть заполнены.");
        return;
    }
    bool yearOk;
    year.toInt(&yearOk);
    if (!yearOk) {
        QMessageBox::warning(this, "Обновление книги", "Год должен быть числом.");
        return;
    }

    Client::get_instance().updateBookInfo(book_id, title, author, year, genre);
    // Очистка всех полей информации о книге и ID после попытки обновления
    mLibrarianBookIdInput->clear();
    mLibrarianBookTitleInput->clear();
    mLibrarianBookAuthorInput->clear();
    mLibrarianBookYearInput->clear();
    mLibrarianBookGenreInput->clear();
}

void MainWindow::onLibrarianSearchClicked() {
    QString searchTerm = mLibrarianSearchInput->text().trimmed();
    if (searchTerm.isEmpty()) {
        QMessageBox::warning(this, "Поиск книг", "Введите текст для поиска.");
        return;
    }
    QString criteriaValue = mLibrarianSearchCriteria->currentData().toString();
    Client::get_instance().searchBooks(criteriaValue, searchTerm);
}

void MainWindow::onRefreshBooksClicked() {
    // Кнопка "Показать все" книги для библиотекаря
    mLibrarianSearchInput->clear();
    Client::get_instance().sendRequest("get_all_books");
}

void MainWindow::onRefreshDebtsClicked() {
    // Кнопка "Показать все выдачи/долги" для библиотекаря
    mLibrarianUserIdInput->clear(); // Очищаем ID пользователя, т.к. смотрим все
    Client::get_instance().sendRequest("get_all_debts");
}


// =============================================
// Response Handlers (Processing data from Server)
// =============================================
void MainWindow::handleBookStats(const QStringList& stats) {
    // Показываем статистику арендованных книг клиента в его таблице
    showBookStatsTable(stats);
}

void MainWindow::handleUserInfo(const QString& login, const QString& email, const QString& password) {
    // Показываем информацию самому пользователю
    QMessageBox::information(this, "Моя информация",
                             QString("Логин: %1\nEmail: %2\nПароль: %3") // ВАЖНО: Показ пароля!
                                 .arg(login, email, password));
}

void MainWindow::handleUserDebts(const QStringList& debts) {
    // Обрабатывает данные для таблицы долгов/аренд на экране библиотекаря
    if (mStackedWidget->currentWidget() == mLibrarianScreen) {
        showUserRentalsTable(debts);
    } else {
        qWarning() << "Received user debts/rentals data, but Librarian screen is not active.";
    }
}

void MainWindow::handleOperationStatus(bool success, const QString& message) {
    qDebug() << "handleOperationStatus - success:" << success << "message:" << message; // Отладка
    QString userVisibleMessage = message; // Сообщение по умолчанию
    bool needsLibrarianBooksUpdate = false;
    bool needsLibrarianDebtsUpdate = false;
    bool needsClientBookStatsUpdate = false;
    bool loginSuccessful = false; // Флаг успешного логина

    if (success) {
        // --- Регистрация ---
        if (message.startsWith("reg+")) {
            userVisibleMessage = "Регистрация прошла успешно! Пожалуйста, войдите.";
            Client::get_instance().setRole("guest");
            Client::get_instance().setUserId(-1);
            mStackedWidget->setCurrentWidget(mMainScreen); // Возврат на главный для входа
        }else if (message.startsWith("auth+")) {
            // ★ Убедимся, что Client уже обновил свои данные ★
            // (Так как сигнал испускается из onReadyRead после установки mRole и mUserId)

            userVisibleMessage = "Аутентификация прошла успешно.";
            QStringList parts = message.split('&'); // Получаем данные снова из сообщения
            if (parts.size() >= 4) {
                userVisibleMessage += "\nВы вошли как: " + parts[1];
                QString role = parts[2]; // Используем роль из СООБЩЕНИЯ
                qDebug() << "Login successful via handleOperationStatus. Role:" << role;

                mAuthLoginInput->clear();
                mAuthPasswordInput->clear();
                loginSuccessful = true;

                // Переключаем экран
                if (role == "client") {
                    mStackedWidget->setCurrentWidget(mClientScreen);
                        // ★ ЗАПРОСЫ ДАННЫХ ПОСЛЕ ПЕРЕКЛЮЧЕНИЯ ★
                    Client::get_instance().viewBookStats();
                    Client::get_instance().viewAllBooks();
                } else if (role == "librarian") {
                    mStackedWidget->setCurrentWidget(mLibrarianScreen);
                    // ★ ЗАПРОСЫ ДАННЫХ ПОСЛЕ ПЕРЕКЛЮЧЕНИЯ ★
                    onRefreshBooksClicked();
                    onRefreshDebtsClicked();
                    onRefreshUsersClicked();
                } else {
                    qWarning() << "Unknown role after authentication:" << role;
                    mStackedWidget->setCurrentWidget(mMainScreen);
                    userVisibleMessage += "\nНеизвестная роль: " + role;
                }
            } else {
                // Ошибка парсинга auth+ сообщения
                success = false; // Считаем это ошибкой
                userVisibleMessage = "Некорректный ответ аутентификации от сервера.";
                loginSuccessful = false;
            }
        } // --- Успешное обновление аннотации ---
        else if (message.startsWith("update_annotation+")) {
            userVisibleMessage = "Аннотация успешно сохранена.";
            // Показываем сообщение и ВОЗВРАЩАЕМСЯ на экран библиотекаря
            QMessageBox::information(this, "Успех", userVisibleMessage);
            // Имитируем нажатие кнопки "Назад", чтобы сбросить ID и очистить поле
            onBackToLibrarianClicked();
            return; // Важно! Прерываем выполнение здесь, чтобы не попасть в общий QMessageBox::information
        } else if (message.startsWith("import_books+")) {
            // Просто показываем сводку, полученную от сервера
            userVisibleMessage = message.mid(13).trimmed(); // Убираем "import_books+&"
            QMessageBox::information(this, "Результат импорта", userVisibleMessage);
                // После импорта нужно обновить список книг
            onRefreshBooksClicked();
            return; // Завершаем обработку здесь
        }
        // --- Другие успешные операции ---
        else if (message.startsWith("extend_rental+")) {
            userVisibleMessage = "Аренда книги успешно продлена.";
            needsClientBookStatsUpdate = true;
        } else if (message.startsWith("assign_book+")) {
            userVisibleMessage = "Книга успешно выдана пользователю.";
            needsLibrarianBooksUpdate = true;
            needsLibrarianDebtsUpdate = true;
        } else if (message.startsWith("unassign_book+")) {
            userVisibleMessage = "Книга успешно возвращена.";
            needsLibrarianBooksUpdate = true;
            needsLibrarianDebtsUpdate = true;
        } else if (message.startsWith("add_book_lib+")) {
            userVisibleMessage = "Книга успешно добавлена в библиотеку.";
            needsLibrarianBooksUpdate = true;
        } else if (message.startsWith("update_book+")) {
            userVisibleMessage = "Информация о книге успешно обновлена.";
            needsLibrarianBooksUpdate = true;
        } else if (message == "Operation successful") {
            userVisibleMessage = "Операция выполнена успешно.";
        }

        // Показываем сообщение об успехе (если это не регистрация, где мы уже переключились)
        if (!message.startsWith("reg+")) {
            QMessageBox::information(this, "Успех", userVisibleMessage);
        }

        // --- Обновления UI для действий, выполненных ПОСЛЕ входа (если это не сам логин) ---
        if (!loginSuccessful) { // Не обновляем сразу после логина, т.к. данные уже запрошены
            if(needsLibrarianBooksUpdate && mStackedWidget->currentWidget() == mLibrarianScreen) {
                onRefreshBooksClicked();
            }
            if(needsLibrarianDebtsUpdate && mStackedWidget->currentWidget() == mLibrarianScreen) {
                onRefreshDebtsClicked();
            }
            if (needsClientBookStatsUpdate && mStackedWidget->currentWidget() == mClientScreen) {
                Client::get_instance().viewBookStats();
            }
        }

    } else { // Обработка ошибок
        // --- Расшифровка сообщений об ошибках ---
        if (message == "auth-") {
            userVisibleMessage = "Ошибка аутентификации. Неверный логин или пароль.";
            // Пароль очищаем при любой ошибке входа, логин оставляем для удобства
            mAuthPasswordInput->clear();
        } else if (message == "reg-") {
            userVisibleMessage = "Ошибка регистрации. Возможно, пользователь с таким логином уже существует.";
            // Оставляем поля, чтобы пользователь мог исправить
        } else if (message.startsWith("error&")) {
            userVisibleMessage = message.mid(6).trimmed(); // Убираем префикс
                // Перевод стандартных ошибок
            if (userVisibleMessage == "Authentication required") userVisibleMessage = "Требуется вход в систему.";
            else if (userVisibleMessage == "Access denied") userVisibleMessage = "Доступ запрещен.";
            else if (userVisibleMessage == "Permission denied") userVisibleMessage = "Недостаточно прав.";
            else if (userVisibleMessage == "Invalid parameters") userVisibleMessage = "Неверные параметры запроса.";
            else if (userVisibleMessage == "Book is not available") userVisibleMessage = "Книга уже выдана или не найдена.";
            else if (userVisibleMessage == "Book is not assigned") userVisibleMessage = "Невозможно продлить (книга не на руках).";
            else if (userVisibleMessage == "User does not exist") userVisibleMessage = "Пользователь не найден.";
            else if (userVisibleMessage == "Invalid search criteria") userVisibleMessage = "Неверный критерий поиска.";
            else if (userVisibleMessage == "Search term cannot be empty") userVisibleMessage = "Введите текст для поиска.";
            else if (userVisibleMessage == "Unknown command") userVisibleMessage = "Сервер не понял команду.";
        }else if (message.startsWith("import_books-")) { // Если сервер явно вернул ошибку импорта
            userVisibleMessage = message.mid(13).trimmed(); // Убираем "import_books-&"
            userVisibleMessage = "Ошибка импорта: " + userVisibleMessage;
        }
        else if (message.startsWith("update_annotation-")) {
            userVisibleMessage = message.mid(20).trimmed(); // Обрезаем префикс "update_annotation-"
                // Если это наша кастомная ошибка с сервера
            if(userVisibleMessage.startsWith("Failed to update annotation in DB")) userVisibleMessage = "Не удалось сохранить аннотацию (ошибка базы данных).";
            else if(userVisibleMessage.startsWith("Book not found")) userVisibleMessage = "Не удалось сохранить аннотацию (книга не найдена).";
            else userVisibleMessage = "Не удалось сохранить аннотацию: " + userVisibleMessage; // Общая ошибка

        } else if (message.startsWith("annotation-")) {
            userVisibleMessage = message.mid(11).trimmed(); // Обрезаем префикс "annotation-"
            if(userVisibleMessage.startsWith("Book not found")) userVisibleMessage = "Не удалось загрузить аннотацию (книга не найдена).";
            else userVisibleMessage = "Не удалось загрузить аннотацию: " + userVisibleMessage; // Общая ошибка
            // ПРИ ОШИБКЕ ЗАГРУЗКИ ВОЗВРАЩАЕМ НАЗАД!
            onBackToLibrarianClicked(); // Не можем редактировать, если не загрузили
        }
        else if (message == "Disconnected from server") { // НЕОЖИДАННЫЙ дисконнект
            qDebug() << "Unexpected disconnection detected by handleOperationStatus.";
            // ★ Показываем сообщение, НО НЕ вызываем onBackButtonClicked рекурсивно ★
            if (mStackedWidget->currentWidget() != mMainScreen) { // Только если еще не вышли
                QMessageBox::information(this, "Соединение потеряно", "Соединение с сервером было разорвано unexpectedly.");
                // Может быть, здесь стоит просто переключить на главный экран, но без вызова всего onBackButtonClicked
                mStackedWidget->setCurrentWidget(mMainScreen);
            }
            return;
        } else if (message.isEmpty()){
            userVisibleMessage = "Неизвестная ошибка сервера (пустой ответ).";
        }

        // Показываем окно с ошибкой (если это не управляемый дисконнект)
        QMessageBox::warning(this, "Ошибка", userVisibleMessage);
    }
}

void MainWindow::handleServerResponse(const QString& response) {
    // Добавляет запись в лог ответов сервера
    mResponseTextEdit->append(QTime::currentTime().toString("hh:mm:ss") + " <<< " + response.trimmed());
}

void MainWindow::handlePublicUserInfo(int id, const QString& login, const QString& email) {
    // Показываем информацию о пользователе библиотекарю
    QMessageBox::information(this, "Информация о пользователе",
                             QString("ID: %1\nЛогин: %2\nEmail: %3")
                                 .arg(id).arg(login).arg(email));
}

void MainWindow::handleOverdueBooksNotification(const QStringList& messages) {
    // Показываем уведомления клиенту
    if(messages.isEmpty()) return;
    QString fullMessage = messages.join("\n\n");
    QMessageBox::warning(this, "Уведомление о книгах", fullMessage.trimmed());
}

void MainWindow::handleClientBooksListUpdate(const QStringList& books) {
    // Заполняет таблицу книг на активном экране (клиента или библиотекаря)
    QWidget* current = mStackedWidget->currentWidget();
    if (current == mClientScreen) {
        populateBooksTable(mClientAllBooksTable, books);
    } else if (current == mLibrarianScreen) {
        populateBooksTable(mLibrarianBooksTable, books);
    } else {
        qDebug() << "Received books list but neither client nor librarian screen is active.";
    }
}

// =============================================
// UI Setup Methods
// =============================================
void MainWindow::setupMainScreen() {
    mMainScreen = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(mMainScreen); // Сразу устанавливаем layout
    layout->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel("Добро пожаловать в OnLib!");
    QFont titleFont("Arial", 18, QFont::Bold); // Используем конкретный шрифт
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);

    // Виджет-контейнер для кнопок для лучшего управления их размером и положением
    QWidget *buttonContainer = new QWidget();
    QVBoxLayout *buttonLayout = new QVBoxLayout(buttonContainer);
    buttonLayout->setContentsMargins(0,0,0,0); // Без отступов внутри контейнера
    buttonLayout->setSpacing(15);
    mRegButton = new QPushButton(" Регистрация", buttonContainer);
    mAuthButton = new QPushButton(" Вход", buttonContainer);
    // Устанавливаем стили и размеры кнопок
    QString buttonStyle = "QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; padding: 10px; }"
                          "QPushButton:hover { background-color: #45a049; }";
    mRegButton->setStyleSheet(buttonStyle);
    mAuthButton->setStyleSheet(buttonStyle.replace("#4CAF50", "#008CBA").replace("#45a049", "#007ba7")); // Другой цвет для входа
    mRegButton->setMinimumSize(200, 45);
    mAuthButton->setMinimumSize(200, 45);
    buttonLayout->addWidget(mRegButton, 0, Qt::AlignCenter); // Центрируем кнопки в их layout
    buttonLayout->addWidget(mAuthButton, 0, Qt::AlignCenter);

    // Добавляем элементы в главный layout экрана
    layout->addStretch(1); // Растяжение сверху
    layout->addWidget(titleLabel);
    layout->addSpacing(40); // Больший отступ
    layout->addWidget(buttonContainer);
    layout->addStretch(2); // Большее растяжение снизу, чтобы поднять контент

    mStackedWidget->addWidget(mMainScreen);

    connect(mRegButton, &QPushButton::clicked, this, &MainWindow::onRegButtonClicked);
    connect(mAuthButton, &QPushButton::clicked, this, &MainWindow::onAuthButtonClicked);
}

void MainWindow::setupRegScreen() {
    mRegScreen = new QWidget();
    QVBoxLayout* mainVLayout = new QVBoxLayout(mRegScreen); // Главный вертикальный layout
    mainVLayout->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel("Регистрация");
    titleLabel->setFont(QFont("Arial", 16, QFont::Bold));
    titleLabel->setAlignment(Qt::AlignCenter);
    mainVLayout->addWidget(titleLabel);
    mainVLayout->addSpacing(20);

    // Группировка полей ввода
    QGroupBox *inputGroup = new QGroupBox();
    QGridLayout *gridLayout = new QGridLayout(inputGroup); // Используем сетку внутри группы

    mRegLoginInput = new QLineEdit(this);
    mRegLoginInput->setPlaceholderText("4-12 символов, латиница/цифры");
    mRegPasswordInput = new QLineEdit(this);
    mRegPasswordInput->setPlaceholderText("8-12 символов"); // Упрощенный плейсхолдер
    mRegPasswordInput->setEchoMode(QLineEdit::Password);
    mRegEmailInput = new QLineEdit(this);
    mRegEmailInput->setPlaceholderText("example@domain.com");
    mRegRoleComboBox = new QComboBox(this);
    mRegRoleComboBox->addItems({"client", "librarian"});

    gridLayout->addWidget(new QLabel("Логин*:"), 0, 0); gridLayout->addWidget(mRegLoginInput, 0, 1);
    gridLayout->addWidget(new QLabel("Пароль*:"), 1, 0); gridLayout->addWidget(mRegPasswordInput, 1, 1);
    gridLayout->addWidget(new QLabel("Email*:"), 2, 0); gridLayout->addWidget(mRegEmailInput, 2, 1);
    gridLayout->addWidget(new QLabel("Роль:"), 3, 0); gridLayout->addWidget(mRegRoleComboBox, 3, 1);
    gridLayout->setColumnStretch(1, 1); // Растягиваем вторую колонку (с полями ввода)

    mainVLayout->addWidget(inputGroup); // Добавляем группу в главный layout

    // Кнопки
    QHBoxLayout *buttonLayout = new QHBoxLayout(); // Горизонтальный layout для кнопок
    mSendRegButton = new QPushButton("Зарегистрироваться", this);
    mBackRegButton = new QPushButton("Назад", this);
    buttonLayout->addStretch(); // Прижать кнопки к краям
    buttonLayout->addWidget(mBackRegButton);
    buttonLayout->addWidget(mSendRegButton);
    buttonLayout->addStretch();

    mainVLayout->addLayout(buttonLayout); // Добавляем кнопки под группой
    mainVLayout->addStretch(); // Растяжение снизу

    mStackedWidget->addWidget(mRegScreen);

    connect(mSendRegButton, &QPushButton::clicked, this, &MainWindow::onSendRegButtonClicked);
    connect(mBackRegButton, &QPushButton::clicked, this, &MainWindow::onBackButtonClicked);
}

void MainWindow::setupAuthScreen() {
    mAuthScreen = new QWidget();
    QVBoxLayout* mainVLayout = new QVBoxLayout(mAuthScreen); // Основной вертикальный layout
    mainVLayout->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel("Вход в систему");
    titleLabel->setFont(QFont("Arial", 16, QFont::Bold));
    titleLabel->setAlignment(Qt::AlignCenter);
    mainVLayout->addWidget(titleLabel);
    mainVLayout->addSpacing(20);

    // Группа для полей ввода
    QGroupBox *inputGroup = new QGroupBox();
    QGridLayout *gridLayout = new QGridLayout(inputGroup); // Сетка внутри

    mAuthLoginInput = new QLineEdit(this);
    mAuthLoginInput->setPlaceholderText("Введите ваш логин");
    mAuthPasswordInput = new QLineEdit(this);
    mAuthPasswordInput->setPlaceholderText("Введите ваш пароль");
    mAuthPasswordInput->setEchoMode(QLineEdit::Password);

    gridLayout->addWidget(new QLabel("Логин:"), 0, 0); gridLayout->addWidget(mAuthLoginInput, 0, 1);
    gridLayout->addWidget(new QLabel("Пароль:"), 1, 0); gridLayout->addWidget(mAuthPasswordInput, 1, 1);
    gridLayout->setColumnStretch(1, 1); // Растянуть колонку с полями ввода

    mainVLayout->addWidget(inputGroup);

    // Кнопки
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    mSendAuthButton = new QPushButton("Войти", this);
    mBackAuthButton = new QPushButton("Назад", this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(mBackAuthButton);
    buttonLayout->addWidget(mSendAuthButton);
    buttonLayout->addStretch();

    mainVLayout->addLayout(buttonLayout);
    mainVLayout->addStretch();

    mStackedWidget->addWidget(mAuthScreen);

    connect(mSendAuthButton, &QPushButton::clicked, this, &MainWindow::onSendAuthButtonClicked);
    connect(mBackAuthButton, &QPushButton::clicked, this, &MainWindow::onBackButtonClicked);
}

void MainWindow::setupClientScreen() {
    mClientScreen = new QWidget();
    QVBoxLayout *mainClientLayout = new QVBoxLayout(mClientScreen);

    // --- Секция управления арендой и информацией ---
    QGroupBox *rentalGroup = new QGroupBox("Мои аренды и информация");
    QVBoxLayout *rentalLayout = new QVBoxLayout(rentalGroup);

    QHBoxLayout *extendLayout = new QHBoxLayout();
    extendLayout->addWidget(new QLabel("ID книги:"));
    mClientBookIdInput = new QLineEdit(this);
    mClientBookIdInput->setPlaceholderText("ID");
    extendLayout->addWidget(mClientBookIdInput);
    extendLayout->addWidget(new QLabel("Продлить на (дней):"));
    mClientDaysInput = new QLineEdit(this);
    mClientDaysInput->setPlaceholderText("Дни");
    extendLayout->addWidget(mClientDaysInput);
    mClientExtendRentalButton = new QPushButton("Продлить", this); // Короче текст
    extendLayout->addWidget(mClientExtendRentalButton);
    extendLayout->addStretch();
    rentalLayout->addLayout(extendLayout);

    QHBoxLayout *infoActionsLayout = new QHBoxLayout();
    mClientViewStatsButton = new QPushButton("Обновить мои аренды", this); // Кнопка явного обновления
    mClientViewInfoButton = new QPushButton("Моя информация", this);
    infoActionsLayout->addWidget(mClientViewStatsButton);
    infoActionsLayout->addWidget(mClientViewInfoButton);
    infoActionsLayout->addStretch();
    rentalLayout->addLayout(infoActionsLayout);

    rentalLayout->addWidget(new QLabel("Текущие арендованные книги:"));
    mClientStatsTable = new QTableWidget(this);
    mClientStatsTable->setColumnCount(4);
    mClientStatsTable->setHorizontalHeaderLabels({"ID Книги", "Название", "Дата выдачи", "Дата возврата"});
    mClientStatsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    mClientStatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mClientStatsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mClientStatsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mClientStatsTable->setAlternatingRowColors(true);
    mClientStatsTable->verticalHeader()->setVisible(false);
    rentalLayout->addWidget(mClientStatsTable);
    mainClientLayout->addWidget(rentalGroup);

    // --- Секция каталога и поиска книг ---
    QGroupBox *allBooksGroup = new QGroupBox("Каталог библиотеки");
    QVBoxLayout *allBooksLayout = new QVBoxLayout(allBooksGroup);

    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel("Искать по:"));
    mClientSearchCriteria = new QComboBox(this);
    mClientSearchCriteria->addItem("Названию", "title");
    mClientSearchCriteria->addItem("Автору", "author");
    mClientSearchCriteria->addItem("Жанру", "genre");
    searchLayout->addWidget(mClientSearchCriteria);
    mClientSearchInput = new QLineEdit(this);
    mClientSearchInput->setPlaceholderText("Введите запрос...");
    searchLayout->addWidget(mClientSearchInput, 1);
    mClientSearchButton = new QPushButton("Найти", this);
    searchLayout->addWidget(mClientSearchButton);
    mClientViewAllBooksButton = new QPushButton("Показать все", this);
    searchLayout->addWidget(mClientViewAllBooksButton);
    allBooksLayout->addLayout(searchLayout);

    allBooksLayout->addWidget(new QLabel("Результат поиска / Весь каталог:"));
    mClientAllBooksTable = new QTableWidget(this);
    mClientAllBooksTable->setColumnCount(6);
    mClientAllBooksTable->setHorizontalHeaderLabels({"ID", "Название", "Автор", "Год", "Жанр", "Статус"});
    mClientAllBooksTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); // Авто-размер
    mClientAllBooksTable->horizontalHeader()->setStretchLastSection(true); // Растянуть последнюю колонку
    mClientAllBooksTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mClientAllBooksTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mClientAllBooksTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mClientAllBooksTable->setAlternatingRowColors(true);
    mClientAllBooksTable->verticalHeader()->setVisible(false);
    allBooksLayout->addWidget(mClientAllBooksTable, 1); // Растягиваем таблицу по вертикали
    mainClientLayout->addWidget(allBooksGroup, 1); // Растягиваем группу каталога

    // Кнопка "Назад" / Выход
    mClientBackButton = new QPushButton("Выйти", this);
    QHBoxLayout* backLayout = new QHBoxLayout();
    backLayout->addStretch();
    backLayout->addWidget(mClientBackButton);
    mainClientLayout->addLayout(backLayout); // Добавляем в конец

    mStackedWidget->addWidget(mClientScreen);

    // --- Подключения сигналов ---
    connect(mClientExtendRentalButton, &QPushButton::clicked, this, &MainWindow::onClientExtendRentalClicked);
    connect(mClientViewStatsButton, &QPushButton::clicked, this, &MainWindow::onClientViewStatsClicked);
    connect(mClientViewInfoButton, &QPushButton::clicked, this, &MainWindow::onClientViewInfoClicked);
    connect(mClientSearchButton, &QPushButton::clicked, this, &MainWindow::onClientSearchClicked);
    connect(mClientViewAllBooksButton, &QPushButton::clicked, this, &MainWindow::onClientViewAllBooksClicked);
    connect(mClientBackButton, &QPushButton::clicked, this, &MainWindow::onBackButtonClicked);
    connect(mClientAllBooksTable, &QTableWidget::itemDoubleClicked, this, &MainWindow::onClientBookDoubleClicked);
}

void MainWindow::setupLibrarianScreen() {
    mLibrarianScreen = new QWidget();
    QVBoxLayout *mainVLayout = new QVBoxLayout(mLibrarianScreen); // Главный вертикальный layout

    // Используем горизонтальный layout для двух колонок
    QHBoxLayout* columnsLayout = new QHBoxLayout();
    mainVLayout->addLayout(columnsLayout, 1); // Две колонки занимают основное место

    // --- Левая колонка: Управление и поиск книг ---
    QVBoxLayout* leftColumnLayout = new QVBoxLayout();
    columnsLayout->addLayout(leftColumnLayout, 1); // Левая колонка (растягивается)

    QGroupBox *bookManageGroup = new QGroupBox("Управление фондом");
    QVBoxLayout* bookManageLayout = new QVBoxLayout(bookManageGroup);
    // Сетка для полей ввода книги
    QGridLayout *bookInfoGrid = new QGridLayout();
    bookInfoGrid->addWidget(new QLabel("Название*:"), 0, 0);
    mLibrarianBookTitleInput = new QLineEdit(this); bookInfoGrid->addWidget(mLibrarianBookTitleInput, 0, 1);
    bookInfoGrid->addWidget(new QLabel("Автор*:"), 1, 0);
    mLibrarianBookAuthorInput = new QLineEdit(this); bookInfoGrid->addWidget(mLibrarianBookAuthorInput, 1, 1);
    bookInfoGrid->addWidget(new QLabel("Год*:"), 2, 0);
    mLibrarianBookYearInput = new QLineEdit(this); bookInfoGrid->addWidget(mLibrarianBookYearInput, 2, 1);
    bookInfoGrid->addWidget(new QLabel("Жанр*:"), 3, 0);
    mLibrarianBookGenreInput = new QLineEdit(this); bookInfoGrid->addWidget(mLibrarianBookGenreInput, 3, 1);
    bookInfoGrid->setColumnStretch(1, 1); // Растянуть поля ввода
    bookManageLayout->addLayout(bookInfoGrid);
    // Поле Book ID и кнопки Add/Update
    QHBoxLayout *bookIdLayout = new QHBoxLayout();
    bookIdLayout->addWidget(new QLabel("Book ID:"));
    mLibrarianBookIdInput = new QLineEdit(this);
    mLibrarianBookIdInput->setPlaceholderText("Для Update/Assign/Unassign");
    bookIdLayout->addWidget(mLibrarianBookIdInput);
    mLibrarianAddBookButton = new QPushButton("Добавить", this);
    bookIdLayout->addWidget(mLibrarianAddBookButton);
    mLibrarianUpdateBookButton = new QPushButton("Обновить", this);
    bookIdLayout->addWidget(mLibrarianUpdateBookButton);
    bookManageLayout->addLayout(bookIdLayout);

    // Добавляем кнопки импорта/экспорта
    QHBoxLayout *importExportLayout = new QHBoxLayout();
    mImportBooksButton = new QPushButton("Импорт из CSV...", this);
    mExportBooksButton = new QPushButton("Экспорт в CSV...", this);
    importExportLayout->addStretch();
    importExportLayout->addWidget(mImportBooksButton);
    importExportLayout->addWidget(mExportBooksButton);
    importExportLayout->addStretch();
    bookManageLayout->addLayout(importExportLayout); // Добавляем в группу управления фондом

    leftColumnLayout->addWidget(bookManageGroup); // Добавляем группу в левую колонку

    // Группа каталога книг (с поиском)
    QGroupBox *allBooksGroupL = new QGroupBox("Каталог книг");
    QVBoxLayout *allBooksLayoutL = new QVBoxLayout(allBooksGroupL);
    // Layout для поиска
    QHBoxLayout *searchLayoutL = new QHBoxLayout();
    searchLayoutL->addWidget(new QLabel("Искать по:"));
    mLibrarianSearchCriteria = new QComboBox(this);
    mLibrarianSearchCriteria->addItem("Названию", "title");
    mLibrarianSearchCriteria->addItem("Автору", "author");
    mLibrarianSearchCriteria->addItem("Жанру", "genre");
    searchLayoutL->addWidget(mLibrarianSearchCriteria);
    mLibrarianSearchInput = new QLineEdit(this);
    mLibrarianSearchInput->setPlaceholderText("Введите запрос...");
    searchLayoutL->addWidget(mLibrarianSearchInput, 1);
    mLibrarianSearchButton = new QPushButton("Найти", this);
    searchLayoutL->addWidget(mLibrarianSearchButton);
    mLibrarianRefreshBooksButton = new QPushButton("Все книги", this); // Бывшая "Refresh"
    searchLayoutL->addWidget(mLibrarianRefreshBooksButton);
    allBooksLayoutL->addLayout(searchLayoutL);
    // Таблица книг
    mLibrarianBooksTable = new QTableWidget(this);
    mLibrarianBooksTable->setColumnCount(6);
    mLibrarianBooksTable->setHorizontalHeaderLabels({"ID", "Название", "Автор", "Год", "Жанр", "Статус"});
    mLibrarianBooksTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    mLibrarianBooksTable->horizontalHeader()->setStretchLastSection(true);
    mLibrarianBooksTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mLibrarianBooksTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mLibrarianBooksTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mLibrarianBooksTable->setAlternatingRowColors(true);
    mLibrarianBooksTable->verticalHeader()->setVisible(false);
    allBooksLayoutL->addWidget(mLibrarianBooksTable, 1); // Растягиваем таблицу
    leftColumnLayout->addWidget(allBooksGroupL, 1); // Добавляем группу каталога, растягиваем

    // --- Правая колонка: Управление пользователями и долгами ---
    QVBoxLayout* rightColumnLayout = new QVBoxLayout();
    columnsLayout->addLayout(rightColumnLayout, 1); // Правая колонка (тоже растягивается)

    QGroupBox *userManageGroup = new QGroupBox("Управление пользователями и выдачей");
    QVBoxLayout* userManageLayout = new QVBoxLayout(userManageGroup);
        // Поля User ID и Login
    QGridLayout *userInfoGrid = new QGridLayout();
    userInfoGrid->addWidget(new QLabel("User ID:"), 0, 0);
    mLibrarianUserIdInput = new QLineEdit(this);
    mLibrarianUserIdInput->setPlaceholderText("Для Assign/Unassign/View User");
    userInfoGrid->addWidget(mLibrarianUserIdInput, 0, 1);
    userInfoGrid->addWidget(new QLabel("User Login:"), 1, 0);
    mLibrarianUserLoginInput = new QLineEdit(this);
    mLibrarianUserLoginInput->setPlaceholderText("Для Get Info");
    userInfoGrid->addWidget(mLibrarianUserLoginInput, 1, 1);
    userInfoGrid->setColumnStretch(1, 1); // Растянуть поля ввода
    userManageLayout->addLayout(userInfoGrid);
        // Кнопки действий с пользователями
    QGridLayout *userActionsGrid = new QGridLayout(); // Сетка для кнопок
    mLibrarianAssignBookButton = new QPushButton("Выдать книгу", this);
    userActionsGrid->addWidget(mLibrarianAssignBookButton, 0, 0);
    mLibrarianUnassignBookButton = new QPushButton("Принять книгу", this);
    userActionsGrid->addWidget(mLibrarianUnassignBookButton, 0, 1);
    mLibrarianViewDebtsButton = new QPushButton("Показать выдачи", this);
    userActionsGrid->addWidget(mLibrarianViewDebtsButton, 1, 0);
    mLibrarianGetUserInfoButton = new QPushButton("Инфо о пользователе", this);
    userActionsGrid->addWidget(mLibrarianGetUserInfoButton, 1, 1);
    userManageLayout->addLayout(userActionsGrid);
    rightColumnLayout->addWidget(userManageGroup);

    // СПИСОК ПОЛЬЗОВАТЕЛЕЙ
    QGroupBox *usersListGroup = new QGroupBox("Пользователи системы");
    QVBoxLayout *usersListLayout = new QVBoxLayout(usersListGroup);

    // Кнопка обновления списка пользователей
    mRefreshUsersButton = new QPushButton("Обновить список пользователей", this);
    QHBoxLayout *refreshUsersLayout = new QHBoxLayout(); // Чтобы кнопка не растягивалась
    refreshUsersLayout->addStretch();
    refreshUsersLayout->addWidget(mRefreshUsersButton);
    refreshUsersLayout->addStretch();
    usersListLayout->addLayout(refreshUsersLayout);

    // Таблица пользователей
    mLibrarianUsersTable = new QTableWidget(this);
    mLibrarianUsersTable->setColumnCount(2); // ID, Login
    mLibrarianUsersTable->setHorizontalHeaderLabels({"User ID", "Логин"});
    mLibrarianUsersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); // Авторазмер ID
    mLibrarianUsersTable->horizontalHeader()->setStretchLastSection(true); // Растянуть Login
    mLibrarianUsersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mLibrarianUsersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mLibrarianUsersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mLibrarianUsersTable->setAlternatingRowColors(true);
    mLibrarianUsersTable->verticalHeader()->setVisible(false);
    mLibrarianUsersTable->setMinimumHeight(150); // Ограничить минимальную высоту
    usersListLayout->addWidget(mLibrarianUsersTable, 1); // Растягиваем таблицу
    rightColumnLayout->addWidget(usersListGroup); // Добавляем группу в правую колонку

    // Группа отображения долгов/выдачи
    QGroupBox *debtsGroup = new QGroupBox("Информация о выдаче/долгах");
    QVBoxLayout *debtsLayout = new QVBoxLayout(debtsGroup);
    // Кнопка обновления всех долгов
    mRefreshDebtsButton = new QPushButton("Показать все выдачи/долги", this);
    QHBoxLayout* refreshDebtsLayout = new QHBoxLayout(); // Чтобы кнопка не растягивалась
    refreshDebtsLayout->addStretch();
    refreshDebtsLayout->addWidget(mRefreshDebtsButton);
    refreshDebtsLayout->addStretch();
    debtsLayout->addLayout(refreshDebtsLayout);
    // Таблица долгов
    mLibrarianDebtsTable = new QTableWidget(this);
    mLibrarianDebtsTable->setColumnCount(6);
    mLibrarianDebtsTable->setHorizontalHeaderLabels({"Rental ID", "User Login", "Book Title", "Start", "End", "Просрочено?"});
    mLibrarianDebtsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    mLibrarianDebtsTable->horizontalHeader()->setStretchLastSection(true); // Растянуть последнюю
    mLibrarianDebtsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mLibrarianDebtsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mLibrarianDebtsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    mLibrarianDebtsTable->setAlternatingRowColors(true);
    mLibrarianDebtsTable->verticalHeader()->setVisible(false);
    debtsLayout->addWidget(mLibrarianDebtsTable, 1); // Растягиваем таблицу
    rightColumnLayout->addWidget(debtsGroup, 1); // Добавляем группу долгов, растягиваем

    // --- Кнопка Назад/Выход (внизу всего окна) ---
    mLibrarianBackButton = new QPushButton("Выйти", this);
    QHBoxLayout* backButtonLayout = new QHBoxLayout();
    backButtonLayout->addStretch();
    backButtonLayout->addWidget(mLibrarianBackButton);
    mainVLayout->addLayout(backButtonLayout); // Добавляем в главный layout под колонками

    mStackedWidget->addWidget(mLibrarianScreen);

    // --- Подключения сигналов ---
    // Управление книгами
    connect(mLibrarianAddBookButton, &QPushButton::clicked, this, &MainWindow::onLibrarianAddBookClicked);
    connect(mLibrarianUpdateBookButton, &QPushButton::clicked, this, &MainWindow::onLibrarianUpdateBookClicked);
    connect(mLibrarianSearchButton, &QPushButton::clicked, this, &MainWindow::onLibrarianSearchClicked);
    connect(mLibrarianRefreshBooksButton, &QPushButton::clicked, this, &MainWindow::onRefreshBooksClicked);
        // Управление пользователями и выдачей
    connect(mLibrarianAssignBookButton, &QPushButton::clicked, this, &MainWindow::onLibrarianAssignBookClicked);
    connect(mLibrarianUnassignBookButton, &QPushButton::clicked, this, &MainWindow::onLibrarianUnassignBookClicked);
    connect(mLibrarianViewDebtsButton, &QPushButton::clicked, this, &MainWindow::onLibrarianViewDebtsClicked); // Запрос долгов конкретного пользователя
    connect(mLibrarianGetUserInfoButton, &QPushButton::clicked, this, &MainWindow::onLibrarianGetUserInfoClicked);
    connect(mRefreshDebtsButton, &QPushButton::clicked, this, &MainWindow::onRefreshDebtsClicked); // Запрос ВСЕХ долгов
    // Навигация
    connect(mLibrarianBackButton, &QPushButton::clicked, this, &MainWindow::onBackButtonClicked);
    // Connect для двойного клика по таблице книг
    connect(mLibrarianBooksTable, &QTableWidget::itemDoubleClicked, this, &MainWindow::onLibrarianBookDoubleClicked);
    connect(mRefreshUsersButton, &QPushButton::clicked, this, &MainWindow::onRefreshUsersClicked);

    connect(mImportBooksButton, &QPushButton::clicked, this, &MainWindow::onImportBooksClicked);
    connect(mExportBooksButton, &QPushButton::clicked, this, &MainWindow::onExportBooksClicked);
}


// =============================================
// Table Population / Display Methods
// =============================================
void MainWindow::populateBooksTable(QTableWidget* table, const QStringList& books) {
    if (!table) return;

    // Очищаем таблицу перед заполнением
    table->setRowCount(0);
    table->clearSelection(); // Сбрасываем выделение
    table->setSortingEnabled(false); // Отключаем сортировку на время заполнения

    for (const QString &bookData : books) {
        QStringList parts = bookData.split(',');
        if (parts.size() < 6) continue; // Пропускаем некорректные строки

        int row = table->rowCount();
        table->insertRow(row);

        // Заполнение ячеек
        table->setItem(row, 0, new QTableWidgetItem(parts.value(0))); // ID
        table->setItem(row, 1, new QTableWidgetItem(parts.value(1))); // Title
        table->setItem(row, 2, new QTableWidgetItem(parts.value(2))); // Author
        table->setItem(row, 3, new QTableWidgetItem(parts.value(3))); // Year
        table->setItem(row, 4, new QTableWidgetItem(parts.value(4))); // Genre

        // Форматирование статуса
        QTableWidgetItem* statusItem = new QTableWidgetItem();
        statusItem->setTextAlignment(Qt::AlignCenter);
        bool isAvailable = (parts.value(5) == "1"); // 1 - доступна
        statusItem->setText(isAvailable ? "Доступна" : "Занята");
        statusItem->setForeground(isAvailable ? QColorConstants::DarkGreen : QColorConstants::Red);
        table->setItem(row, 5, statusItem);

        // Запрет редактирования для всех ячеек в строке
        for (int col = 0; col < 6; ++col) {
            if (QTableWidgetItem* item = table->item(row, col)) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
        }
    }

    table->resizeColumnsToContents(); // Авто-размер колонок по содержимому
    table->horizontalHeader()->setStretchLastSection(true); // Растягиваем последнюю колонку (Статус)
    table->setSortingEnabled(true); // Включаем сортировку после заполнения
}

void MainWindow::showBookStatsTable(const QStringList& stats) {
    // Заполняет таблицу арендованных книг клиента (mClientStatsTable)
    if (!mClientStatsTable) return;

    mClientStatsTable->setRowCount(0);
    mClientStatsTable->clearSelection();
    mClientStatsTable->setSortingEnabled(false);

    // В отличие от populateBooksTable, здесь stats - это одномерный список "id,title,start,end,id,title,start,end,..."
    // Но сервер теперь возвращает stats как join строк "id,title,start,end" через "|". Обрабатываем как books.
    for (const QString &statData : stats) {
        QStringList parts = statData.split(',');
        if (parts.size() < 4) continue; // Ожидаем 4 поля

        int row = mClientStatsTable->rowCount();
        mClientStatsTable->insertRow(row);

        mClientStatsTable->setItem(row, 0, new QTableWidgetItem(parts.value(0))); // ID
        mClientStatsTable->setItem(row, 1, new QTableWidgetItem(parts.value(1))); // Title
        mClientStatsTable->setItem(row, 2, new QTableWidgetItem(parts.value(2))); // Start Date
        mClientStatsTable->setItem(row, 3, new QTableWidgetItem(parts.value(3))); // End Date

        // Запрет редактирования
        for (int col = 0; col < 4; ++col) {
            if (QTableWidgetItem* item = mClientStatsTable->item(row, col)) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
        }
    }
    mClientStatsTable->resizeColumnsToContents();
    mClientStatsTable->horizontalHeader()->setStretchLastSection(true);
    mClientStatsTable->setSortingEnabled(true);
}

void MainWindow::showUserRentalsTable(const QStringList& rentals) {
    // Заполняет таблицу долгов/аренд на экране библиотекаря (mLibrarianDebtsTable)
    if (!mLibrarianDebtsTable) return;

    mLibrarianDebtsTable->setRowCount(0);
    mLibrarianDebtsTable->clearSelection();
    mLibrarianDebtsTable->setSortingEnabled(false);

    for (const QString &rentalData : rentals) {
        QStringList parts = rentalData.split(',');
        if (parts.size() < 6) continue; // Ожидаем ID, UserLogin, Title, Start, End, IsOverdue

        int row = mLibrarianDebtsTable->rowCount();
        mLibrarianDebtsTable->insertRow(row);

        mLibrarianDebtsTable->setItem(row, 0, new QTableWidgetItem(parts.value(0))); // Rental ID
        mLibrarianDebtsTable->setItem(row, 1, new QTableWidgetItem(parts.value(1))); // User Login
        mLibrarianDebtsTable->setItem(row, 2, new QTableWidgetItem(parts.value(2))); // Book Title
        mLibrarianDebtsTable->setItem(row, 3, new QTableWidgetItem(parts.value(3))); // Start Date
        mLibrarianDebtsTable->setItem(row, 4, new QTableWidgetItem(parts.value(4))); // End Date

        // Форматирование статуса просрочки
        QTableWidgetItem *statusItem = new QTableWidgetItem();
        statusItem->setTextAlignment(Qt::AlignCenter);
        bool isOverdue = (parts.value(5) == "1");
        statusItem->setText(isOverdue ? "ДА" : "Нет");
        statusItem->setForeground(isOverdue ? QColorConstants::Red : QColorConstants::DarkGreen);
        mLibrarianDebtsTable->setItem(row, 5, statusItem);

        // Запрет редактирования
        for (int col = 0; col < 6; ++col) {
            if (QTableWidgetItem* item = mLibrarianDebtsTable->item(row, col)) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
        }
    }
    mLibrarianDebtsTable->resizeColumnsToContents();
    mLibrarianDebtsTable->horizontalHeader()->setStretchLastSection(true);
    mLibrarianDebtsTable->setSortingEnabled(true);
}

// ★ Метод настройки экрана редактирования аннотации
void MainWindow::setupAnnotationScreen() {
    mAnnotationScreen = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(mAnnotationScreen);

    // Заголовок с названием книги
    mAnnotationBookTitleLabel = new QLabel("Аннотация для: [Книга не выбрана]");
    QFont titleFont = mAnnotationBookTitleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    mAnnotationBookTitleLabel->setFont(titleFont);
    mAnnotationBookTitleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(mAnnotationBookTitleLabel);

    // Поле для редактирования аннотации
    mAnnotationEdit = new QTextEdit(this);
    mAnnotationEdit->setPlaceholderText("Введите аннотацию книги здесь...");
    layout->addWidget(mAnnotationEdit, 1); // Растягиваем поле ввода

    // Кнопки Сохранить и Назад
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    mBackToLibrarianButton = new QPushButton("Назад (Без сохранения)", this);
    mSaveAnnotationButton = new QPushButton("Сохранить аннотацию", this);
    buttonLayout->addWidget(mBackToLibrarianButton);
    buttonLayout->addStretch(); // Отодвинуть кнопки друг от друга
    buttonLayout->addWidget(mSaveAnnotationButton);
    layout->addLayout(buttonLayout);

    mStackedWidget->addWidget(mAnnotationScreen); // Добавляем экран в стак

    // Подключаем кнопки
    connect(mSaveAnnotationButton, &QPushButton::clicked, this, &MainWindow::onSaveAnnotationClicked);
    connect(mBackToLibrarianButton, &QPushButton::clicked, this, &MainWindow::onBackToLibrarianClicked);
}

// Слот для двойного клика по таблице книг библиотекаря
void MainWindow::onLibrarianBookDoubleClicked(QTableWidgetItem *item) {
    if (!item) return; // Клик не по ячейке

    int row = item->row();
    if (row < 0 || !mLibrarianBooksTable) return;

    QTableWidgetItem* idItem = mLibrarianBooksTable->item(row, 0); // ID книги в первой колонке
    QTableWidgetItem* titleItem = mLibrarianBooksTable->item(row, 1); // Название во второй

    if (!idItem || !titleItem) {
        qWarning() << "Could not get ID or Title item from double-clicked row:" << row;
        return;
    }

    bool ok;
    int bookId = idItem->text().toInt(&ok);
    QString bookTitle = titleItem->text(); // Получаем название прямо из таблицы

    if (!ok || bookId <= 0) {
        qWarning() << "Invalid book ID in table row:" << idItem->text();
        return;
    }

    qDebug() << "Double clicked on book ID:" << bookId << "Title:" << bookTitle;

    // Сохраняем ID редактируемой книги
    mCurrentlyEditingBookId = bookId;

    // Подготавливаем экран редактирования
    mAnnotationBookTitleLabel->setText("Аннотация для: '" + bookTitle + "' (ID: " + QString::number(bookId) + ")");
    mAnnotationEdit->clear(); // Очищаем старую аннотацию
    mAnnotationEdit->setPlaceholderText("Загрузка аннотации...");

    // Запрашиваем актуальную аннотацию с сервера
    Client::get_instance().getBookAnnotation(bookId);

    // Переключаемся на экран редактирования
    mStackedWidget->setCurrentWidget(mAnnotationScreen);
}

// Слот для обработки полученной аннотации от сервера
void MainWindow::handleBookAnnotation(int book_id, const QString& title, const QString& annotation) {

    QWidget* currentScreen = mStackedWidget->currentWidget();

    // --- Сценарий 1: Библиотекарь редактирует аннотацию ---
    if (currentScreen == mAnnotationScreen && book_id == mCurrentlyEditingBookId) {
        qDebug() << "Received annotation for librarian edit screen, book ID:" << book_id;
        // Обновляем заголовок (на случай если он изменился)
        mAnnotationBookTitleLabel->setText("Аннотация для: '" + title + "' (ID: " + QString::number(book_id) + ")");
        // Устанавливаем текст аннотации
        mAnnotationEdit->setPlainText(annotation);
        mAnnotationEdit->setPlaceholderText("Введите аннотацию книги здесь...");
    }
    // --- Сценарий 2: Клиент запросил просмотр аннотации ---
    else if (currentScreen == mClientScreen) {
        qDebug() << "Received annotation for client view, book ID:" << book_id;
        QString infoTitle = "Аннотация для '" + title + "' (ID: " + QString::number(book_id) + ")";
        QString infoText = annotation.isEmpty() ? "Аннотация для этой книги еще не добавлена." : annotation;

        // Показываем информацию во всплывающем окне
        QMessageBox msgBox(this); // Создаем на стеке, чтобы было модальным для MainWindow
        msgBox.setWindowTitle(infoTitle);
        msgBox.setText(infoText);
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setStandardButtons(QMessageBox::Ok);
        // msgBox.setTextFormat(Qt::RichText); // Если аннотация содержит HTML или Markdown
        msgBox.exec(); // Показываем окно и ждем закрытия

    }
    // --- Сценарий 3: Ответ пришел не вовремя или для другой книги ---
    else {
        qDebug() << "Received annotation for book ID:" << book_id
                 << "but annotation screen (ID:" << mCurrentlyEditingBookId << ")"
                 << "or client screen is not active. Current screen:" << currentScreen->objectName(); // Для отладки
        // Если currentScreen не имеет objectName, выведет пустую строку.
    }
}

// Слот для кнопки "Сохранить аннотацию"
void MainWindow::onSaveAnnotationClicked() {
    if (mCurrentlyEditingBookId <= 0) {
        QMessageBox::warning(this, "Ошибка", "Не выбрана книга для сохранения аннотации.");
        return;
    }
    QString newAnnotation = mAnnotationEdit->toPlainText(); // Получаем текст
    qDebug() << "Saving annotation for book ID:" << mCurrentlyEditingBookId;
    Client::get_instance().updateBookAnnotation(mCurrentlyEditingBookId, newAnnotation);
    // Сообщение об успехе/ошибке будет показано в handleOperationStatus
    // Опционально: можно добавить индикатор "Сохранение..."
}

// Слот для кнопки "Назад" на экране аннотации
void MainWindow::onBackToLibrarianClicked() {
    mCurrentlyEditingBookId = -1; // Сбрасываем ID
    mAnnotationEdit->clear();    // Очищаем поле
    mAnnotationBookTitleLabel->setText("Аннотация для: [Книга не выбрана]");
    mStackedWidget->setCurrentWidget(mLibrarianScreen); // Возвращаемся на экран библиотекаря
}

// ★ Слот для двойного клика по таблице книг КЛИЕНТА ★
void MainWindow::onClientBookDoubleClicked(QTableWidgetItem *item) {
    if (!item) return; // Клик не по ячейке

    int row = item->row();
    if (row < 0 || !mClientAllBooksTable) return; // Используем таблицу клиента

    QTableWidgetItem* idItem = mClientAllBooksTable->item(row, 0); // ID книги в первой колонке

    if (!idItem) {
        qWarning() << "Could not get ID item from client book table row:" << row;
        return;
    }

    bool ok;
    int bookId = idItem->text().toInt(&ok);

    if (!ok || bookId <= 0) {
        qWarning() << "Invalid book ID in client table row:" << idItem->text();
        return;
    }

    qDebug() << "Client double clicked on book ID:" << bookId;

    // Запрашиваем аннотацию для этой книги
    // Ответ будет обработан слотом handleBookAnnotation
    Client::get_instance().getBookAnnotation(bookId);

    // Никуда не переключаемся, просто ждем ответа в handleBookAnnotation
}

// Слот для кнопки "Обновить список пользователей"
void MainWindow::onRefreshUsersClicked() {
    Client::get_instance().getAllUsers();
}

// Слот для обработки полученного списка пользователей
void MainWindow::handleAllUsersList(const QStringList& users) {
    qDebug() << "Received user list for display:" << users;
        // Заполняем таблицу пользователей на экране библиотекаря
    populateUsersTable(users);
}

// Метод для заполнения таблицы пользователей
void MainWindow::populateUsersTable(const QStringList& users) {
    if (!mLibrarianUsersTable) return;

    mLibrarianUsersTable->setRowCount(0);
    mLibrarianUsersTable->clearSelection();
    mLibrarianUsersTable->setSortingEnabled(false);

    for (const QString &userData : users) {
        QStringList parts = userData.split(',');
        if (parts.size() < 2) continue; // Ожидаем ID, Login

        int row = mLibrarianUsersTable->rowCount();
        mLibrarianUsersTable->insertRow(row);

        // Создаем QTableWidgetItem для ID и Login
        QTableWidgetItem* idItem = new QTableWidgetItem(parts.value(0)); // ID
        QTableWidgetItem* loginItem = new QTableWidgetItem(parts.value(1)); // Login

        // Опционально: Выравнивание ID по центру/правому краю
        //idItem->setTextAlignment(Qt::AlignCenter);

        // Добавляем в таблицу
        mLibrarianUsersTable->setItem(row, 0, idItem);
        mLibrarianUsersTable->setItem(row, 1, loginItem);

        // Запрет редактирования
        for (int col = 0; col < 2; ++col) {
            if (QTableWidgetItem* item = mLibrarianUsersTable->item(row, col)) {
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
        }
    }
    mLibrarianUsersTable->resizeColumnsToContents(); // Авторазмер колонок
    mLibrarianUsersTable->horizontalHeader()->setStretchLastSection(true); // Растянуть колонку Login
    mLibrarianUsersTable->setSortingEnabled(true);
}

// Слот для кнопки "Импорт из CSV"
void MainWindow::onImportBooksClicked() {
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    tr("Выберите CSV файл для импорта"),
                                                    QDir::homePath(), // Начать с домашней папки
                                                    tr("CSV Файлы (*.csv);;Все файлы (*)"));

    if (filePath.isEmpty()) {
        return; // Пользователь отменил выбор
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ошибка импорта", "Не удалось открыть файл:\n" + file.errorString());
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8"); // Явно указываем кодировку
    QString fileContent = in.readAll();
    file.close();

    if (fileContent.isEmpty()) {
        QMessageBox::warning(this, "Ошибка импорта", "Выбранный файл пуст.");
        return;
    }

    // ВАЖНОЕ ПРЕДУПРЕЖДЕНИЕ о потенциальной проблеме с '&'
    if (fileContent.contains('&')) {
        QMessageBox::warning(this, "Предупреждение", "Данные CSV содержат символ '&', что может привести к ошибкам при импорте из-за текущего протокола обмена. Рекомендуется удалить или заменить этот символ в файле.");
        // Можно либо прервать, либо продолжить на страх и риск пользователя
        // return;
    }


    // Отправляем данные на сервер
    Client::get_instance().importBooksCsv(fileContent);

    // TODO: Можно показать индикатор загрузки
    mResponseTextEdit->append("Отправка запроса на импорт..."); // Простое уведомление
}

// Слот для кнопки "Экспорт в CSV"
void MainWindow::onExportBooksClicked() {
    // TODO: Показать индикатор ожидания
    mResponseTextEdit->append("Запрос на экспорт каталога...");
    Client::get_instance().exportBooksCsv();
    // Результат придет в слот handleExportCsvData
}

// Слот для обработки полученных данных CSV для экспорта
void MainWindow::handleExportCsvData(const QString& csvData) {
    mResponseTextEdit->append("Получены данные для экспорта.");

    QString filePath = QFileDialog::getSaveFileName(this,
                                                    tr("Сохранить каталог как CSV"),
                                                    QDir::homePath() + "/onlib_catalog.csv", // Имя файла по умолчанию
                                                    tr("CSV Файлы (*.csv);;Все файлы (*)"));

    if (filePath.isEmpty()) {
        return; // Пользователь отменил сохранение
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ошибка экспорта", "Не удалось создать/открыть файл для записи:\n" + file.errorString());
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8"); // Устанавливаем кодировку UTF-8
    out << csvData;
    file.close();

    if (out.status() != QTextStream::Ok) {
        QMessageBox::critical(this, "Ошибка экспорта", "Произошла ошибка при записи в файл.");
    } else {
        QMessageBox::information(this, "Экспорт завершен", "Каталог книг успешно сохранен в файл:\n" + filePath);
    }
}
