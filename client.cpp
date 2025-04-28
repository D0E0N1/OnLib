#include "client.h"
#include <QDate>

Client* Client::p_instance = nullptr;

Client::Client(QObject *parent) : QObject(parent),
    mTcpSocket(new QTcpSocket(this)),
    mRole("guest"),
    mUserId(-1)
{
    connect(mTcpSocket, &QTcpSocket::readyRead, this, &Client::onReadyRead);
    connect(mTcpSocket, &QTcpSocket::disconnected, this, &Client::onDisconnected);
}

Client& Client::get_instance() {
    if (!p_instance) {
        p_instance = new Client();
    }
    return *p_instance;
}

void Client::destroy_instance() {
    if (p_instance) {
        delete p_instance;
        p_instance = nullptr;
    }
}

void Client::connectToServer(const QString& host, quint16 port) {
    mTcpSocket->connectToHost(host, port);
    if (mTcpSocket->waitForConnected(3000)) {
        qDebug() << "Connected to server:" << host << port;
    } else {
        qDebug() << "Connection failed:" << mTcpSocket->errorString();
        emit operationStatusReceived(false, "Connection failed: " + mTcpSocket->errorString());
    }
}

void Client::sendRequest(const QString& request) {
    if (mTcpSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Not connected to server. Attempting to reconnect...";
        connectToServer("localhost", 33333);
    }

    if (mTcpSocket->state() == QAbstractSocket::ConnectedState) {
        mTcpSocket->write(request.toUtf8());
        qDebug() << "Request sent:" << request;
    } else {
        qDebug() << "Failed to reconnect to server.";
        emit operationStatusReceived(false, "Failed to reconnect to server");
    }
}

void Client::disconnectFromServer() {
    if (mTcpSocket->state() != QAbstractSocket::UnconnectedState) { // Проверка, что еще подключен
        mIsDisconnectingGracefully = true; // ★ Устанавливаем флаг ПЕРЕД дисконнектом
        mTcpSocket->disconnectFromHost();
    } else {
        mIsDisconnectingGracefully = false; // Сбрасываем на всякий случай, если уже отключены
    }
}

// User methods
void Client::extendRental(int book_id, int days) {
    QString request = QString("extend_rental&%1&%2&%3")
    .arg(mUserId)
        .arg(book_id)
        .arg(days);
    sendRequest(request);
}

void Client::viewBookStats() {
    QString request = QString("view_book_stats&%1").arg(mUserId);
    sendRequest(request);
}

void Client::viewUserInfo() {
    QString request = QString("view_user_info&%1").arg(mUserId);
    sendRequest(request);
}

// Librarian methods
void Client::assignBook(int user_id, int book_id) {
    QString request = QString("assign_book&%1&%2")
    .arg(user_id)
        .arg(book_id);
    sendRequest(request);
}

void Client::unassignBook(int user_id, int book_id) {
    QString request = QString("unassign_book&%1&%2")
    .arg(user_id)
        .arg(book_id);
    sendRequest(request);
}

void Client::viewUserDebts(int user_id) {
    QString request = QString("view_user_debts&%1").arg(user_id);
    sendRequest(request);
}

void Client::getUserInfo(const QString& login) {
    QString request = QString("get_user_info&%1").arg(login);
    sendRequest(request);
}

void Client::addBookToLibrary(const QString& title, const QString& author,
                              const QString& year, const QString& genre) {
    QString request = QString("add_book_to_lib&%1&%2&%3&%4")
    .arg(title)
        .arg(author)
        .arg(year)
        .arg(genre);
    sendRequest(request);
}

void Client::updateBookInfo(int book_id, const QString& title,
                            const QString& author, const QString& year,
                            const QString& genre) {
    QString request = QString("update_book_info&%1&%2&%3&%4&%5")
    .arg(book_id)
        .arg(title)
        .arg(author)
        .arg(year)
        .arg(genre);
    sendRequest(request);
}

// Getters/Setters
void Client::setRole(const QString& role) {
    mRole = role;
}

QString Client::getRole() const {
    return mRole;
}

void Client::setUserId(int id) {
    mUserId = id;
}

int Client::getUserId() const {
    return mUserId;
}

void Client::onReadyRead() {
    QByteArray response = mTcpSocket->readAll();
    QString responseStr = QString::fromUtf8(response).trimmed();
    qDebug() << "Raw response from server:" << responseStr;

    // Игнорируем сообщение о старте сервера
    if (responseStr.contains("Successful starting, OnLib is working!")) {
        return;
    }

    QStringList parts = responseStr.split('&');
    if (parts.isEmpty()) return;

    QString command = parts[0];

    // Обработка ответов
    if (command == "auth+") {
        if (parts.size() >= 4) {
            QString login = parts[1];
            mRole = parts[2];
            mUserId = parts[3].toInt();

            // Отправляем все данные, полученные при аутентификации
            emit operationStatusReceived(true, responseStr);
        }
    } else if (command == "reg+") {
        qDebug() << "Registration success, resetting role to guest";
        mRole = "guest";
        mUserId = -1;
        emit operationStatusReceived(true, responseStr);
    } // Обработка ответа с аннотацией
    else if (command == "annotation+") {
        // Ожидаем: annotation+ & book_id & title & annotation_text (4 части)
        if (parts.size() == 4) {
            bool ok;
            int book_id = parts[1].toInt(&ok);
            QString title = parts[2];
            QString annotation = parts[3];
            if (ok) {
                emit bookAnnotationReceived(book_id, title, annotation); // Теперь с ID!
            } else {
                qWarning() << "Invalid book_id in annotation+ response:" << parts[1];
                emit operationStatusReceived(false, "Server sent invalid book ID in annotation response.");
            }
        } else {
            qWarning() << "Invalid format for annotation+ response (expected 4 parts):" << responseStr;
            emit operationStatusReceived(false, "Invalid format for annotation response from server.");
        }
    } else if (command == "all_users+") { // ★ Обработка списка пользователей
        QStringList userList;
        if (parts.size() >= 2 && !parts[1].isEmpty()) {
            userList = parts[1].split('|'); // Разделяем на строки "id,login"
        }
        qDebug() << "[Client] Emitting allUsersListReceived with entries:" << userList;
        emit allUsersListReceived(userList);
    } else if (command == "export_books+") {
        if (parts.size() >= 2) {
            // Данные могут содержать '&', поэтому собираем все части начиная со второй
            QString csvData = parts.mid(1).join('&');
            emit exportCsvDataReceived(csvData);
        } else {
            qWarning() << "Received empty export_books+ response.";
            emit operationStatusReceived(false, "Сервер прислал пустые данные для экспорта.");
        }
    } else if (command == "rental_history+") {
        QStringList historyList;
        if (parts.size() >= 2 && !parts[1].isEmpty()) {
            historyList = parts[1].split('|');
        }
        qDebug() << "[Client] Emitting rentalHistoryReceived with entries:" << historyList;
        emit rentalHistoryReceived(historyList);
    }

    // Обработка УСПЕХА обновления аннотации (статус придет через operationStatusReceived)
    else if (command == "update_annotation+") {
        if (parts.size() == 2) { // update_annotation+ & book_id
            // Мы можем использовать book_id для чего-то, если нужно
            emit operationStatusReceived(true, responseStr); // Передаем исходное сообщение
        } else {
            qWarning() << "Invalid format for update_annotation+ response:" << responseStr;
            emit operationStatusReceived(false, "Invalid success response for annotation update");
        }
    }
    else if (command == "book_stats+") {
        QStringList bookEntriesList; // Переименуем для ясности (список ЗАПИСЕЙ)
        if (parts.size() >= 2 && !parts[1].isEmpty()) { // Проверяем, что данные не пустые
            // Просто разбиваем по '|' - каждая строка это уже нужная нам запись
            bookEntriesList = parts[1].split('|');

            // Проверка на просрочку/уведомления (можно оставить, она не влияет на сигнал)
            for (const QString& entry : bookEntriesList) {
                QStringList bookInfo = entry.split(','); // Разбиваем только для проверки дат
                if(bookInfo.size() >= 4) { // Добавим проверку размера
                    // Проверяем просрочку
                    QDate endDate = QDate::fromString(bookInfo[3], Qt::ISODate);
                    if (endDate.isValid() && endDate < QDate::currentDate()) {
                        QString overdueMsg = QString("Книга '%1' просрочена! Дата возврата: %2")
                                                 .arg(bookInfo[1], bookInfo[3]);
                        emit overdueBooksNotification({overdueMsg});
                    }
                    else if (endDate.isValid()) {
                        int daysLeft = QDate::currentDate().daysTo(endDate);
                        if (daysLeft <= 3 && daysLeft >= 0) { // Уведомляем за 3 дня до И если срок еще не прошел
                            QString warningMsg = QString("Книга '%1' должна быть возвращена через %2 дней (до %3)")
                                                     .arg(bookInfo[1])
                                                     .arg(daysLeft)
                                                     .arg(bookInfo[3]);
                            emit overdueBooksNotification({warningMsg});
                        }
                    }
                }
            }
        }
        // Отправляем сигнал со списком строк "id,title,start,end"
        qDebug() << "[Client] Emitting bookStatsReceived with entries:" << bookEntriesList;
        emit bookStatsReceived(bookEntriesList); // <-- ПЕРЕДАЕМ ПРАВИЛЬНЫЙ СПИСОК
    } // Обработка ответа со статистикой
    else if (command == "library_stats+") {
        if (parts.size() == 7) { // Ожидаем 6 значений + команда
            bool ok[6];
            int totalBooks = parts[1].toInt(&ok[0]);
            int availableBooks = parts[2].toInt(&ok[1]);
            int rentedBooks = parts[3].toInt(&ok[2]);
            int totalClients = parts[4].toInt(&ok[3]);
            int activeRentals = parts[5].toInt(&ok[4]);
            int overdueRentals = parts[6].toInt(&ok[5]);

            // Проверяем, что все преобразования прошли успешно
            bool allOk = true;
            for(int i=0; i<6; ++i) { if(!ok[i]) allOk = false; }

            if (allOk) {
                emit libraryStatsReceived(totalBooks, availableBooks, rentedBooks, totalClients, activeRentals, overdueRentals);
            } else {
                qWarning() << "Failed to parse integer from library_stats+ response:" << responseStr;
                emit operationStatusReceived(false, "Сервер прислал неверный формат статистики.");
            }
        } else {
            qWarning() << "Invalid format for library_stats+ response (expected 7 parts):" << responseStr;
            emit operationStatusReceived(false, "Сервер прислал неверный формат статистики.");
        }
    }

    else if (command == "user_info+") {
        if (parts.size() >= 2) {
            QStringList info = parts[1].split(',');
            if (info.size() >= 3) { // Проверяем 3 поля
                emit userInfoReceived(info[0], info[1], info[2]); // Добавляем password
            }
        }
    }
    else if (command == "user_debts+") {
        QStringList debts;
        if (parts.size() >= 2) {
            qDebug() << "Received debts data:" << parts;
            debts = parts[1].split('|');
        }
        emit userDebtsReceived(debts);
    }
    else if (command == "public_user_info+") {
        if (parts.size() >= 2) {
            QStringList info = parts[1].split(',');
            if (info.size() >= 3) {
                int id = info[0].toInt();
                QString login = info[1];
                QString email = info[2];
                emit publicUserInfoReceived(id, login, email);
            }
        }
    }
    else if (command == "books_list+") {
        QStringList books;
        if (parts.size() >= 2) {
            books = parts[1].split('|');
        }
        emit booksListReceived(books);
    }
    else if (command == "all_debts+") {
        QStringList debts;
        if (parts.size() >= 2) {
            debts = parts[1].split('|');
        }
        emit userDebtsReceived(debts);
    } // Новая обработка ответов на ДЕТАЛЬНЫЕ ОТЧЕТЫ
    else if (command == "report_table_data+") {
        // Ответ с ТАБЛИЧНЫМИ данными
        // Ожидаемый формат: report_table_data+ & ТипОтчета & Заголовок1,Заголовок2 & Данные11,Данные12|Данные21,Данные22...
        if (parts.size() >= 4) { // Должно быть как минимум 4 части
            QString reportType = parts[1]; // Тип отчета
            QStringList headers = parts[2].split(','); // Заголовки, разделенные запятой
            QString rawData = parts[3]; // Строка с данными (разделеными '|') или пустая

            QStringList dataRows;
            // Обрабатываем случай, когда данные не пустые
            if (!rawData.isEmpty()) {
                // Строки данных, разделенные '|'
                // Если символ '|' отсутствует, но строка данных не пуста, значит, пришла одна строка данных
                if (!rawData.contains('|')) {
                    dataRows.append(rawData);
                } else {
                    // Если '|' есть, разделяем как обычно, но пропускаем пустые строки (может быть '|' в конце)
                    dataRows = rawData.split('|', Qt::SkipEmptyParts);
                }
            }
            // Иначе dataRows останется пустым (если rawData был пуст)

            qDebug() << "[Client] Emitting statisticsReportReceived for" << reportType
                     << "Headers:" << headers.count() << "Rows:" << dataRows.count();
            // Испускаем сигнал с полученными данными
            emit statisticsReportReceived(reportType, headers, dataRows);
        } else {
            // Ошибка: неверный формат ответа от сервера
            qWarning() << "Invalid format for report_table_data+ response (expected 4+ parts):" << responseStr;
            emit operationStatusReceived(false, "Сервер прислал некорректный формат табличного отчета.");
        }
    }
    else if (command == "report_chart_data+") {
        // Ответ с данными для ГРАФИКА
        // Ожидаемый формат: report_chart_data+ & ТипОтчета & Метка1,Метка2 & Значение1,Значение2
        if (parts.size() == 4) { // Ровно 4 части
            QString reportType = parts[1]; // Тип отчета
            QStringList labels = parts[2].split(','); // Метки, разделенные запятой
            QStringList valueStrings = parts[3].split(','); // Значения (как строки), разделенные запятой

            QList<qreal> values; // Список для числовых значений
            bool conversionOk = true; // Флаг успешности конвертации

            // Пытаемся сконвертировать строковые значения в числа (qreal)
            for (const QString& s : valueStrings) {
                if (s.isEmpty() && valueStrings.count() == 1) { // Сервер может прислать пустую строку если данных нет
                    qDebug() << "Received empty value string for chart data.";
                    break; // Выходим, values останется пустым
                }
                bool ok;
                qreal val = s.toDouble(&ok);
                if (!ok) {
                    conversionOk = false; // Ошибка конвертации
                    qWarning() << "Failed to convert chart value to double:" << s;
                    break;
                }
                values.append(val); // Добавляем сконвертированное значение
            }

            // Проверяем, что конвертация прошла успешно И количество меток совпадает с количеством значений
            if (conversionOk && labels.count() == values.count()) {
                qDebug() << "[Client] Emitting chartDataReceived for" << reportType << "Labels:" << labels.count();
                // Испускаем сигнал с данными для графика
                emit chartDataReceived(reportType, labels, values);
            } else if (labels.count() != values.count()) {
                qWarning() << "Label/Value count mismatch for report_chart_data+ response:" << responseStr
                           << "Labels:" << labels.count() << "Values:" << values.count();
                emit operationStatusReceived(false, "Ошибка в данных для графика от сервера (несовпадение количества).");
            } else { // Ошибка конвертации conversionOk == false
                emit operationStatusReceived(false, "Ошибка в данных для графика от сервера (нечисловые значения).");
            }
        } else {
            // Ошибка: неверный формат ответа от сервера
            qWarning() << "Invalid format for report_chart_data+ response (expected 4 parts):" << responseStr;
            emit operationStatusReceived(false, "Сервер прислал некорректный формат данных для графика.");
        }
    } // Ответ со списком жанров
    else if (command == "genres_list+") {
        // Ожидаем: genres_list+&жанр1,жанр2,жанр3...
        QStringList genres;
        if (parts.size() == 2 && !parts[1].isEmpty()) {
            genres = parts[1].split(','); // Разделяем строку жанров по запятой
        } else if (parts.size() == 1) {
            // Сервер прислал genres_list+, но без данных (нет жанров в БД?)
            qDebug() << "Received empty genres list from server.";
        } else {
            qWarning() << "Invalid format for genres_list+ response:" << responseStr;
            emit operationStatusReceived(false, "Некорректный формат списка жанров от сервера.");
        }
        // В любом случае (даже если список пуст) испускаем сигнал
        qDebug() << "[Client] Emitting genresListReceived with" << genres.count() << "genres.";
        qDebug() << "[Client] Parsed genres before emitting:" << genres;
        emit genresListReceived(genres);
    }

    else if (command.endsWith('+')) {
        emit operationStatusReceived(true, "Operation successful");
    }
    else if (command.endsWith('-')) {
        QString errorMsg = parts.size() > 1 ? parts[1] : "Operation failed";
        if (command == "report_data-") {
            errorMsg = "Ошибка формирования отчета на сервере: " + errorMsg;
        } else if (command == "genres_list-") {
            errorMsg = "Ошибка получения списка жанров: " + errorMsg;
        }
        emit operationStatusReceived(false, errorMsg);
    }

    else {
        // Неизвестная команда от сервера
        qWarning() << "Received unknown command from server:" << command;
        // Можно просто проигнорировать или показать общее сообщение
        // emit operationStatusReceived(false, "Получен неизвестный ответ от сервера: " + command);
    }

    // Общий сигнал для отображения сырого ответа в логе (независимо от успеха/ошибки)
    emit responseReceived(responseStr);
}


void Client::onDisconnected() {
    qDebug() << "Disconnected from server";
    bool wasGraceful = mIsDisconnectingGracefully; // Сохраняем значение флага
    mIsDisconnectingGracefully = false; // Сбрасываем флаг СРАЗУ

    // ★ Испускаем сигнал, ТОЛЬКО если это НЕ штатный выход ★
    if (!wasGraceful) {
        emit operationStatusReceived(false, "Disconnected from server");
    } else {
        qDebug() << "Graceful disconnection, suppressing 'Disconnected' message signal.";
    }
}

void Client::viewAllBooks() {
    // Отправляем ту же команду, что и библиотекарь для обновления списка
    QString request = "get_all_books";
    sendRequest(request);
}

void Client::searchBooks(const QString& criteria, const QString& searchTerm) {
    QString request = QString("search_books&%1&%2")
    .arg(criteria)
        .arg(searchTerm);
    sendRequest(request);
}

// Отправка запроса на получение аннотации
void Client::getBookAnnotation(int book_id) {
    QString request = QString("get_book_annotation&%1").arg(book_id);
    sendRequest(request);
}

// Отправка запроса на обновление аннотации
void Client::updateBookAnnotation(int book_id, const QString& annotation) {
    QString request = QString("update_book_annotation&%1&%2").arg(book_id).arg(annotation);
    sendRequest(request);
}

void Client::getAllUsers() {
    sendRequest("get_all_users");
}

// Отправка запроса на импорт CSV
void Client::importBooksCsv(const QString& csvData) {
    // ВАЖНО: Если csvData может содержать '&', это сломает разбор на сервере.
    // Нужен более надежный способ передачи данных (Base64, JSON, POST-like).
    // Пока предполагаем, что '&' нет.
    QString request = QString("import_books_csv&%1").arg(csvData);
    sendRequest(request);
}

// Отправка запроса на экспорт CSV
void Client::exportBooksCsv() {
    sendRequest("export_books_csv");
}

// Реализации новых методов для управления пользователями
void Client::blockUser(int user_id) {
    sendRequest(QString("block_user&%1").arg(user_id));
}

void Client::unblockUser(int user_id) {
    sendRequest(QString("unblock_user&%1").arg(user_id));
}

// ★ Метод теперь принимает и отправляет новый пароль ★
void Client::resetUserPassword(int user_id, const QString& new_password) {
    // Проверка на '&' (лучше бы использовать Base64)
    if (new_password.contains('&')) {
        qWarning() << "New password contains '&', this might break the protocol!";
        emit operationStatusReceived(false, "Пароль содержит недопустимый символ '&'");
        return;
    }
    sendRequest(QString("reset_user_password&%1&%2").arg(user_id).arg(new_password));
}

void Client::updateUserEmail(int user_id, const QString& new_email) {
    // Простая проверка на клиенте (более строгая на сервере)
    if (new_email.contains('&') || new_email.contains('\r') || new_email.contains('\n')) {
        qWarning() << "Email contains forbidden characters for current protocol.";
        emit operationStatusReceived(false, "Email содержит недопустимые символы.");
        return;
    }
    sendRequest(QString("update_user_email&%1&%2").arg(user_id).arg(new_email));
}

// Запрос истории аренды текущего пользователя
void Client::viewRentalHistory() {
    if (mUserId <= 0) {
        qWarning() << "Cannot view rental history: User not logged in.";
        emit operationStatusReceived(false, "Для просмотра истории необходимо войти.");
        return;
    }
    sendRequest(QString("get_rental_history&%1").arg(mUserId));
}

// Запрос статистики библиотеки
void Client::getLibraryStats() {
    sendRequest("get_library_stats");
}

// --- Новый метод для запроса ДЕТАЛЬНЫХ ОТЧЕТОВ ---
void Client::getStatisticsReport(const QString& reportType, const QDate& startDate, const QDate& endDate,
                                 const QString& optionalFilter) {
    // Формируем строку запроса к серверу
    // Формат: get_statistics_report&ТипОтчета&ДатаНачала&ДатаКонца&Фильтр
    QString request = QString("get_statistics_report&%1&%2&%3&%4")
                          .arg(reportType)                      // Тип отчета
                          .arg(startDate.toString(Qt::ISODate)) // Дата начала в формате yyyy-MM-dd
                          .arg(endDate.toString(Qt::ISODate))   // Дата конца в формате yyyy-MM-dd
                          .arg(optionalFilter);                 // Опциональный фильтр (может быть пустым)
    sendRequest(request); // Отправляем сформированный запрос на сервер
}

// Реализация запроса жанров
void Client::requestGenresList() {
    sendRequest("get_genres_list"); // Команда серверу для получения жанров
}
